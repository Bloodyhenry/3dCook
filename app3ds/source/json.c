/*
 * Analyseur JSON sans allocation (voir json.h).
 * Descente récursive classique ; chaque fonction avance `pos` dans le texte.
 */
#include "json.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *t;
    int n, pos;
    JsonJeton *j;
    int max, nb;
} Analyseur;

static void blancs(Analyseur *a)
{
    while (a->pos < a->n) {
        char c = a->t[a->pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
        a->pos++;
    }
}

static int nouveau(Analyseur *a, JsonType type)
{
    if (a->nb >= a->max) return JSON_ERR_JETONS;
    JsonJeton *j = &a->j[a->nb];
    j->type = type;
    j->debut = j->fin = a->pos;
    j->taille = 0;
    j->suivant = a->nb + 1;
    return a->nb++;
}

static bool hexa(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int chaine(Analyseur *a)
{
    int i = nouveau(a, JSON_TEXTE);
    if (i < 0) return i;
    a->pos++;                              /* guillemet ouvrant */
    a->j[i].debut = a->pos;
    while (a->pos < a->n) {
        unsigned char c = (unsigned char)a->t[a->pos];
        if (c == '"') {
            a->j[i].fin = a->pos++;
            return i;
        }
        if (c < 0x20) return JSON_ERR_SYNTAXE;   /* caractère de contrôle brut */
        if (c == '\\') {
            if (++a->pos >= a->n) return JSON_ERR_SYNTAXE;
            char e = a->t[a->pos];
            if (e == 'u') {
                for (int k = 1; k <= 4; k++)
                    if (a->pos + k >= a->n || !hexa(a->t[a->pos + k])) return JSON_ERR_SYNTAXE;
                a->pos += 4;
            } else if (!strchr("\"\\/bfnrt", e) || e == '\0') {
                return JSON_ERR_SYNTAXE;
            }
        }
        a->pos++;
    }
    return JSON_ERR_SYNTAXE;                  /* guillemet fermant manquant */
}

static bool chiffre(const Analyseur *a)
{
    return a->pos < a->n && a->t[a->pos] >= '0' && a->t[a->pos] <= '9';
}

static int nombre(Analyseur *a)
{
    int i = nouveau(a, JSON_NOMBRE);
    if (i < 0) return i;
    if (a->t[a->pos] == '-') a->pos++;
    if (!chiffre(a)) return JSON_ERR_SYNTAXE;
    if (a->t[a->pos] == '0') a->pos++;          /* pas de zéro en tête : « 01 » interdit */
    else while (chiffre(a)) a->pos++;
    if (a->pos < a->n && a->t[a->pos] == '.') {
        a->pos++;
        if (!chiffre(a)) return JSON_ERR_SYNTAXE;
        while (chiffre(a)) a->pos++;
    }
    if (a->pos < a->n && (a->t[a->pos] == 'e' || a->t[a->pos] == 'E')) {
        a->pos++;
        if (a->pos < a->n && (a->t[a->pos] == '+' || a->t[a->pos] == '-')) a->pos++;
        if (!chiffre(a)) return JSON_ERR_SYNTAXE;
        while (chiffre(a)) a->pos++;
    }
    a->j[i].fin = a->pos;
    return i;
}

static int litteral(Analyseur *a, const char *mot, JsonType type)
{
    int lg = (int)strlen(mot);
    if (a->pos + lg > a->n || strncmp(a->t + a->pos, mot, (size_t)lg) != 0)
        return JSON_ERR_SYNTAXE;
    int i = nouveau(a, type);
    if (i < 0) return i;
    a->pos += lg;
    a->j[i].fin = a->pos;
    return i;
}

static int valeur(Analyseur *a, int profondeur);

/* Objet ou tableau : `fermant` vaut '}' ou ']'. */
static int conteneur(Analyseur *a, int profondeur, JsonType type, char fermant)
{
    if (profondeur >= JSON_PROFONDEUR_MAX) return JSON_ERR_PROFONDEUR;
    int i = nouveau(a, type);
    if (i < 0) return i;
    a->pos++;                                  /* '{' ou '[' */
    blancs(a);
    if (a->pos < a->n && a->t[a->pos] == fermant) {
        a->pos++;
    } else {
        for (;;) {
            blancs(a);
            if (type == JSON_OBJET) {
                if (a->pos >= a->n || a->t[a->pos] != '"') return JSON_ERR_SYNTAXE;
                int cle = chaine(a);
                if (cle < 0) return cle;
                blancs(a);
                if (a->pos >= a->n || a->t[a->pos] != ':') return JSON_ERR_SYNTAXE;
                a->pos++;
            }
            int v = valeur(a, profondeur + 1);
            if (v < 0) return v;
            a->j[i].taille++;
            blancs(a);
            if (a->pos >= a->n) return JSON_ERR_SYNTAXE;
            char c = a->t[a->pos++];
            if (c == fermant) break;
            if (c != ',') return JSON_ERR_SYNTAXE;
        }
    }
    a->j[i].fin = a->pos;
    a->j[i].suivant = a->nb;
    return i;
}

static int valeur(Analyseur *a, int profondeur)
{
    blancs(a);
    if (a->pos >= a->n) return JSON_ERR_SYNTAXE;
    char c = a->t[a->pos];
    switch (c) {
    case '{': return conteneur(a, profondeur, JSON_OBJET, '}');
    case '[': return conteneur(a, profondeur, JSON_TABLEAU, ']');
    case '"': return chaine(a);
    case 't': return litteral(a, "true", JSON_VRAI);
    case 'f': return litteral(a, "false", JSON_FAUX);
    case 'n': return litteral(a, "null", JSON_NUL);
    default:
        if (c == '-' || (c >= '0' && c <= '9')) return nombre(a);
        return JSON_ERR_SYNTAXE;
    }
}

int json_analyser(const char *texte, int longueur, JsonJeton *jetons, int max_jetons,
                  int *position)
{
    Analyseur a = {texte, longueur, 0, jetons, max_jetons, 0};
    int r = valeur(&a, 0);
    if (r >= 0) {
        blancs(&a);
        if (a.pos != a.n) r = JSON_ERR_SYNTAXE;   /* texte en trop après la valeur */
    }
    if (position) *position = a.pos;
    return r < 0 ? r : a.nb;
}

const char *json_message(int code)
{
    switch (code) {
    case JSON_ERR_SYNTAXE:    return "JSON invalide";
    case JSON_ERR_JETONS:     return "JSON trop gros";
    case JSON_ERR_PROFONDEUR: return "JSON trop imbriqué";
    default:                  return "erreur inconnue";
    }
}

/* --- Lecture -------------------------------------------------------------------- */

int json_premier(const JsonJeton *j, int conteneur)
{
    if (conteneur < 0) return -1;
    if (j[conteneur].type != JSON_OBJET && j[conteneur].type != JSON_TABLEAU) return -1;
    return j[conteneur].taille > 0 ? conteneur + 1 : -1;
}

int json_suivant(const JsonJeton *j, int element)
{
    return element < 0 ? -1 : j[element].suivant;
}

int json_cle(const char *t, const JsonJeton *j, int objet, const char *cle)
{
    if (objet < 0 || j[objet].type != JSON_OBJET) return -1;
    int lg = (int)strlen(cle);
    int k = objet + 1;
    for (int n = 0; n < j[objet].taille; n++) {
        if (j[k].fin - j[k].debut == lg && memcmp(t + j[k].debut, cle, (size_t)lg) == 0)
            return k + 1;
        k = j[k + 1].suivant;                  /* saute la valeur et son sous-arbre */
    }
    return -1;
}

static unsigned valeur_hexa(const char *p)
{
    unsigned v = 0;
    for (int k = 0; k < 4; k++) {
        char c = p[k];
        v = v * 16 + (unsigned)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
    }
    return v;
}

/* Écrit le point de code `cp` en UTF-8 s'il reste la place ; renvoie false sinon. */
static bool ecrire_utf8(unsigned cp, char *sortie, int *pos, int taille)
{
    char tmp[4];
    int n;
    if (cp < 0x80) { tmp[0] = (char)cp; n = 1; }
    else if (cp < 0x800) { tmp[0] = (char)(0xC0 | (cp >> 6)); tmp[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
    else if (cp < 0x10000) {
        tmp[0] = (char)(0xE0 | (cp >> 12)); tmp[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        tmp[2] = (char)(0x80 | (cp & 0x3F)); n = 3;
    } else {
        tmp[0] = (char)(0xF0 | (cp >> 18)); tmp[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        tmp[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); tmp[3] = (char)(0x80 | (cp & 0x3F)); n = 4;
    }
    if (*pos + n > taille - 1) return false;
    memcpy(sortie + *pos, tmp, (size_t)n);
    *pos += n;
    return true;
}

bool json_texte(const char *t, const JsonJeton *j, int indice, char *sortie, int taille)
{
    if (taille <= 0) return false;
    sortie[0] = '\0';
    if (indice < 0 || j[indice].type != JSON_TEXTE) return false;
    int pos = 0;
    for (int i = j[indice].debut; i < j[indice].fin;) {
        unsigned char c = (unsigned char)t[i];
        if (c == '\\') {
            char e = t[i + 1];
            unsigned cp;
            i += 2;
            switch (e) {
            case 'b': cp = '\b'; break;
            case 'f': cp = '\f'; break;
            case 'n': cp = '\n'; break;
            case 'r': cp = '\r'; break;
            case 't': cp = '\t'; break;
            case 'u':
                cp = valeur_hexa(t + i);
                i += 4;
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    /* Paire de substitution : 😀 → U+1F600. */
                    if (i + 6 <= j[indice].fin && t[i] == '\\' && t[i + 1] == 'u') {
                        unsigned bas = valeur_hexa(t + i + 2);
                        if (bas >= 0xDC00 && bas <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (bas - 0xDC00);
                            i += 6;
                        } else {
                            cp = 0xFFFD;
                        }
                    } else {
                        cp = 0xFFFD;
                    }
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    cp = 0xFFFD;                /* demi-paire isolée */
                }
                break;
            default: cp = (unsigned char)e; break;   /* \" \\ \/ */
            }
            if (!ecrire_utf8(cp, sortie, &pos, taille)) break;
        } else {
            /* Octet UTF-8 brut : on copie le caractère entier ou rien. */
            int n = c < 0x80 ? 1 : c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
            if (i + n > j[indice].fin) n = j[indice].fin - i;
            if (pos + n > taille - 1) break;
            memcpy(sortie + pos, t + i, (size_t)n);
            pos += n;
            i += n;
        }
    }
    sortie[pos] = '\0';
    return true;
}

bool json_nombre(const char *t, const JsonJeton *j, int indice, double *sortie)
{
    if (indice < 0 || j[indice].type != JSON_NOMBRE) return false;
    char tampon[64];
    int lg = j[indice].fin - j[indice].debut;
    if (lg <= 0 || lg >= (int)sizeof(tampon)) return false;
    memcpy(tampon, t + j[indice].debut, (size_t)lg);
    tampon[lg] = '\0';
    *sortie = strtod(tampon, NULL);
    return isfinite(*sortie);
}

bool json_entier(const char *t, const JsonJeton *j, int indice, int *sortie)
{
    double v;
    if (!json_nombre(t, j, indice, &v)) return false;
    if (v != floor(v) || v < -2147483647.0 || v > 2147483647.0) return false;
    *sortie = (int)v;
    return true;
}

bool json_booleen(const JsonJeton *j, int indice, bool *sortie)
{
    if (indice < 0) return false;
    if (j[indice].type == JSON_VRAI) { *sortie = true; return true; }
    if (j[indice].type == JSON_FAUX) { *sortie = false; return true; }
    return false;
}

bool json_est_nul(const JsonJeton *j, int indice)
{
    return indice >= 0 && j[indice].type == JSON_NUL;
}
