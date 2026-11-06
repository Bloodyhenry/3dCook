/* Réglages de l'appli (voir config.h). */
#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "fichiers.h"

/* « HTTP://… » commence-t-il par « http:// » ? (sans tenir compte de la casse) */
static bool commence_par(const char *s, const char *prefixe)
{
    for (; *prefixe; s++, prefixe++)
        if (tolower((unsigned char)*s) != *prefixe) return false;
    return true;
}

static char *rogner(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    char *fin = s + strlen(s);
    while (fin > s && isspace((unsigned char)fin[-1])) *--fin = '\0';
    return s;
}

void config_defaut(Config *c)
{
    memset(c, 0, sizeof(*c));
    c->micro = true;
    c->sensibilite = 3;
    c->camera = false;
    c->sensibilite_camera = 3;
}

/* « oui » / « non » (ou 1 / 0) ; toute autre valeur laisse `valeur` inchangée. */
static void lire_oui_non(const char *texte, bool *valeur)
{
    if (strcmp(texte, "oui") == 0 || strcmp(texte, "1") == 0) *valeur = true;
    else if (strcmp(texte, "non") == 0 || strcmp(texte, "0") == 0) *valeur = false;
}

/* Un chiffre de 1 à 5 ; toute autre valeur laisse `valeur` inchangée. */
static void lire_sensibilite(const char *texte, int *valeur)
{
    if (texte[0] >= '1' && texte[0] <= '5' && texte[1] == '\0') *valeur = texte[0] - '0';
}

bool config_lire(const char *chemin, Config *c)
{
    config_defaut(c);
    char contenu[2048];
    int lg = fichier_lire(chemin, contenu, sizeof(contenu) - 1);
    if (lg < 0) return false;
    contenu[lg] = '\0';

    char *ligne = contenu;
    while (ligne && *ligne) {
        char *suite = strpbrk(ligne, "\r\n");
        if (suite) *suite++ = '\0';
        char *egal = strchr(ligne, '=');
        char *debut = rogner(ligne);
        if (egal && *debut != '#' && *debut != ';') {
            *egal = '\0';
            char *cle = rogner(debut);
            char *valeur = rogner(egal + 1);
            if (strcmp(cle, "passerelle") == 0) {
                /* Une valeur abîmée ne remplace pas une adresse valide. */
                char propre[TAILLE_ADRESSE];
                if (config_normaliser_adresse(valeur, propre, sizeof(propre)))
                    snprintf(c->passerelle, sizeof(c->passerelle), "%s", propre);
            } else if (strcmp(cle, "micro") == 0) {
                lire_oui_non(valeur, &c->micro);
            } else if (strcmp(cle, "sensibilite") == 0) {
                lire_sensibilite(valeur, &c->sensibilite);
            } else if (strcmp(cle, "camera") == 0) {
                lire_oui_non(valeur, &c->camera);
            } else if (strcmp(cle, "sensibilite_camera") == 0) {
                lire_sensibilite(valeur, &c->sensibilite_camera);
            }
        }
        ligne = suite;
    }
    return true;
}

bool config_ecrire(const char *chemin, const Config *c)
{
    char contenu[512];
    int lg = snprintf(contenu, sizeof(contenu),
                      "# Réglages de l'appli Recettes (3DS).\n"
                      "# Modifiables depuis la console : liste des recettes, bouton « Réglages ».\n"
                      "\n"
                      "# Adresse de la passerelle (affichée sur sa page web).\n"
                      "passerelle = %s\n"
                      "\n"
                      "# Souffler dans le micro pour passer à l'étape suivante (oui / non),\n"
                      "# et sensibilité de 1 (souffler fort) à 5 (souffle léger).\n"
                      "micro = %s\n"
                      "sensibilite = %d\n"
                      "\n"
                      "# Passer la main devant la caméra intérieure (oui / non), et sensibilité.\n"
                      "camera = %s\n"
                      "sensibilite_camera = %d\n",
                      c->passerelle, c->micro ? "oui" : "non", c->sensibilite,
                      c->camera ? "oui" : "non", c->sensibilite_camera);
    if (lg <= 0 || lg >= (int)sizeof(contenu)) return false;
    return fichier_ecrire_atomique(chemin, contenu, lg);
}

bool config_normaliser_adresse(const char *saisie, char *sortie, int taille)
{
    char tampon[TAILLE_ADRESSE];
    snprintf(tampon, sizeof(tampon), "%s", saisie);
    char *s = rogner(tampon);

    /* Schéma : http:// facultatif, https:// refusé (pas de TLS sur la 3DS ici). */
    if (commence_par(s, "https://")) return false;
    if (commence_par(s, "http://")) s += 7;

    /* Retire les « / » de fin. */
    size_t lg = strlen(s);
    while (lg > 0 && s[lg - 1] == '/') s[--lg] = '\0';
    if (lg == 0 || strchr(s, '/')) return false;   /* pas de chemin : la passerelle est à la racine */

    /* Hôte : lettres, chiffres, « . » et « - » ; puis « :port » facultatif. */
    char *deux_points = strchr(s, ':');
    size_t lg_hote = deux_points ? (size_t)(deux_points - s) : lg;
    if (lg_hote == 0) return false;
    for (size_t i = 0; i < lg_hote; i++)
        if (!isalnum((unsigned char)s[i]) && s[i] != '.' && s[i] != '-') return false;

    const char *port = PORT_PAR_DEFAUT;
    if (deux_points) {
        port = deux_points + 1;
        size_t lp = strlen(port);
        if (lp == 0 || lp > 5) return false;
        long valeur = 0;
        for (size_t i = 0; i < lp; i++) {
            if (!isdigit((unsigned char)port[i])) return false;
            valeur = valeur * 10 + (port[i] - '0');
        }
        if (valeur < 1 || valeur > 65535) return false;
    }
    char hote[TAILLE_ADRESSE];
    snprintf(hote, sizeof(hote), "%.*s", (int)lg_hote, s);
    for (char *p = hote; *p; p++) *p = (char)tolower((unsigned char)*p);
    return snprintf(sortie, (size_t)taille, "http://%s:%s", hote, port) < taille;
}
