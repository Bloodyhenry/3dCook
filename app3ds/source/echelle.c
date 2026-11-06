/*
 * Mise à l'échelle des quantités (voir echelle.h et docs/schema.md).
 *
 * Arrondi « au demi supérieur » sur un pas qui dépend de l'unité :
 *   g, ml   : < 10 → 0,5 ; < 50 → 1 ; < 250 → 5 ; sinon 10
 *   kg, l   : < 1 → 0,05 ; sinon 0,1
 *   cl, dl  : < 10 → 0,5 ; sinon 1
 *   mg      : 1
 *   autres  : < 10 → ¼ ; sinon 1
 * Une quantité positive n'est jamais arrondie à zéro.
 */
#include "echelle.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

const double ECHELLE_MULTIPLICATEURS[ECHELLE_NB_MULTIPLICATEURS] = {0.5, 1, 1.5, 2, 3, 4};

static const char *const METRIQUES[] = {"g", "kg", "mg", "ml", "cl", "dl", "l", NULL};
static const char *const INVARIABLES[] = {"g", "kg", "mg", "ml", "cl", "dl", "l",
                                          "c. à soupe", "c. à café", "noix", NULL};

static bool dans_liste(const char *mot, const char *const *liste)
{
    if (!mot) return false;
    for (int i = 0; liste[i]; i++)
        if (strcmp(mot, liste[i]) == 0) return true;
    return false;
}

static bool egal(const char *a, const char *b) { return a && strcmp(a, b) == 0; }

static bool proche(double a, double b) { return fabs(a - b) < 1e-9; }

double echelle_facteur(bool a_portions, int portions_base, double valeur)
{
    if (a_portions && portions_base > 0) return valeur / portions_base;
    return valeur;
}

double echelle_pas(double q, const char *unite)
{
    if (egal(unite, "g") || egal(unite, "ml"))
        return q < 10 ? 0.5 : q < 50 ? 1 : q < 250 ? 5 : 10;
    if (egal(unite, "kg") || egal(unite, "l"))
        return q < 1 ? 0.05 : 0.1;
    if (egal(unite, "cl") || egal(unite, "dl"))
        return q < 10 ? 0.5 : 1;
    if (egal(unite, "mg"))
        return 1;
    return q < 10 ? 0.25 : 1;
}

double echelle_arrondir(double q, const char *unite)
{
    double pas = echelle_pas(q, unite);
    /* Même calcul que math.floor(q / pas + 0.5) * pas en Python. */
    double r = floor(q / pas + 0.5) * pas;
    if (r <= 0 && q > 0) r = pas;
    return round(r * 1000.0) / 1000.0;
}

double echelle_appliquer(double q, double f, const char *unite)
{
    if (f == 1.0) return q;
    return echelle_arrondir(q * f, unite);
}

void echelle_formater_nombre(double v, const char *unite, char *sortie, size_t taille)
{
    if (proche(v, round(v))) {
        snprintf(sortie, taille, "%d", (int)round(v));
        return;
    }
    if (!dans_liste(unite, METRIQUES)) {
        int entier = (int)floor(v);
        double frac = round((v - entier) * 100.0) / 100.0;
        const char *symbole = proche(frac, 0.25) ? "¼" : proche(frac, 0.5) ? "½"
                            : proche(frac, 0.75) ? "¾" : NULL;
        if (symbole) {
            if (entier) snprintf(sortie, taille, "%d %s", entier, symbole);
            else snprintf(sortie, taille, "%s", symbole);
            return;
        }
    }
    /* Deux décimales au plus, zéros inutiles retirés, virgule française. */
    char tampon[32];
    snprintf(tampon, sizeof(tampon), "%.2f", v);
    int n = (int)strlen(tampon);
    while (n > 0 && tampon[n - 1] == '0') tampon[--n] = '\0';
    if (n > 0 && tampon[n - 1] == '.') tampon[--n] = '\0';
    for (char *c = tampon; *c; c++)
        if (*c == '.') *c = ',';
    snprintf(sortie, taille, "%s", tampon);
}

static bool finit_par(const char *mot, int longueur, const char *fin)
{
    int lf = (int)strlen(fin);
    return longueur >= lf && strncmp(mot + longueur - lf, fin, (size_t)lf) == 0;
}

void echelle_pluriel(const char *unite, double q, char *sortie, size_t taille)
{
    if (dans_liste(unite, INVARIABLES) || q < 2) {
        snprintf(sortie, taille, "%s", unite);
        return;
    }
    /* Mot par mot : « petit pot » → « petits pots ». */
    size_t pos = 0;
    sortie[0] = '\0';
    const char *debut = unite;
    while (*debut) {
        const char *fin = strchr(debut, ' ');
        int lg = fin ? (int)(fin - debut) : (int)strlen(debut);
        char mot[64];
        int lm = lg < 60 ? lg : 60;
        memcpy(mot, debut, (size_t)lm);
        mot[lm] = '\0';
        if (finit_par(mot, lm, "s") || finit_par(mot, lm, "x") || finit_par(mot, lm, "z")) {
            /* déjà au pluriel ou invariable */
        } else if (finit_par(mot, lm, "eau") || finit_par(mot, lm, "au")) {
            strcat(mot, "x");
        } else if (finit_par(mot, lm, "al")) {
            mot[lm - 2] = '\0';
            strcat(mot, "aux");
        } else {
            strcat(mot, "s");
        }
        pos += (size_t)snprintf(sortie + pos, pos < taille ? taille - pos : 0, "%s%s",
                                pos ? " " : "", mot);
        if (!fin) break;
        debut = fin + 1;
    }
}

void echelle_formater_ingredient(const Ingredient *i, double f, char *sortie, size_t taille)
{
    if (!i->a_quantite) {
        snprintf(sortie, taille, "%s", i->nom);
        return;
    }
    const char *unite = i->unite[0] ? i->unite : NULL;
    double q = echelle_appliquer(i->quantite, f, unite);
    char nombre[32], nombre_max[32] = "", unite_txt[TAILLE_UNITE + 8] = "";
    echelle_formater_nombre(q, unite, nombre, sizeof(nombre));

    double reference = q;
    if (i->a_quantite_max) {
        double q2 = echelle_appliquer(i->quantite_max, f, unite);
        if (q2 > 0) {
            nombre_max[0] = '-';
            echelle_formater_nombre(q2, unite, nombre_max + 1, sizeof(nombre_max) - 1);
            reference = q2;
        }
    }
    if (unite) {
        unite_txt[0] = ' ';
        echelle_pluriel(unite, reference, unite_txt + 1, sizeof(unite_txt) - 1);
    }
    snprintf(sortie, taille, "%s%s%s %s", nombre, nombre_max, unite_txt, i->nom);
}

void echelle_formater_multiplicateur(double m, char *sortie, size_t taille)
{
    char nombre[32];
    echelle_formater_nombre(m, NULL, nombre, sizeof(nombre));
    snprintf(sortie, taille, "×%s", nombre);
}
