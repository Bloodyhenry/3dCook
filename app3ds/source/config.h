/*
 * Réglages de l'appli : fichier sdmc:/3ds/recettes/config.ini.
 *
 *   # Réglages de l'appli Recettes
 *   passerelle = http://192.168.1.10:8383
 *   micro = oui
 *   sensibilite = 3
 *   camera = non
 *   sensibilite_camera = 3
 *
 * Le fichier est créé et modifié par l'appli elle-même (écran Réglages),
 * on n'a pas besoin de sortir la carte SD. Format : une ligne « clé = valeur »
 * par réglage, « # » pour les commentaires, clés inconnues ignorées.
 *
 * Portable : testé sur le Mac.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#define CHEMIN_CONFIG    "sdmc:/3ds/recettes/config.ini"
#define PORT_PAR_DEFAUT  "8383"
#define TAILLE_ADRESSE   128

typedef struct {
    char passerelle[TAILLE_ADRESSE];   /* "" si pas encore réglée */
    bool micro;                        /* souffler pour passer à l'étape suivante */
    int  sensibilite;                  /* du micro, de 1 (peu) à 5 (très sensible) */
    bool camera;                       /* passer la main devant la caméra intérieure */
    int  sensibilite_camera;           /* de 1 (peu) à 5 (très sensible) */
} Config;

/* Valeurs par défaut : pas d'adresse, micro activé, caméra coupée
 * (elle consomme plus de batterie), sensibilités à 3. */
void config_defaut(Config *c);

/* Lit le fichier ; renvoie false s'il n'existe pas (c garde les valeurs par défaut). */
bool config_lire(const char *chemin, Config *c);

/* Écrit le fichier de façon atomique. */
bool config_ecrire(const char *chemin, const Config *c);

/*
 * Nettoie une adresse saisie au clavier :
 *   « 192.168.1.10 »          → « http://192.168.1.10:8383 »
 *   « HTTP://nas.local:8383/ » → « http://nas.local:8383 »
 * Renvoie false si l'adresse est inutilisable (vide, https, caractères
 * interdits, port invalide).
 */
bool config_normaliser_adresse(const char *saisie, char *sortie, int taille);

#endif
