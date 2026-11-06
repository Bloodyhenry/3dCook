/*
 * Menu de la semaine : réponse de GET /menu de la passerelle (docs/schema.md),
 * gardée sur la carte SD dans CHEMIN_MENU pour fonctionner sans Wi-Fi.
 *
 * Portable : testé sur le Mac.
 */
#ifndef MENU_H
#define MENU_H

#include <stdbool.h>

#include "recette.h"

#define CHEMIN_MENU  "sdmc:/3ds/recettes/menu.json"
#define MENU_MAX     64

typedef struct {
    char   id[TAILLE_ID];
    char   titre[TAILLE_TITRE];
    char   rev[12];
    bool   image;
    char   date[11];        /* « 2026-11-07 », ou "" */
    char   repas[24];       /* « Dîner », ou "" */
    double echelle;         /* multiplicateur choisi dans le planning (1 par défaut) */
} EntreeMenu;

typedef struct {
    int        nb;
    EntreeMenu entrees[MENU_MAX];
    long       maj;
    int        ignorees;    /* entrées rejetées (identifiant invalide, menu trop long) */
} Menu;

/* Lit la réponse de /menu. Renvoie false (avec `erreur`) si elle est inutilisable. */
bool menu_depuis_json(const char *texte, int longueur, Menu *m, char *erreur, int taille_erreur);

/* Lit le menu gardé sur la carte ; false s'il n'y en a pas ou s'il est abîmé. */
bool menu_lire(const char *chemin, Menu *m);

/*
 * Un identifiant sert de nom de fichier (<id>.json) : on n'accepte que
 * lettres, chiffres, « _ » et « - » (1 à 32 caractères). Ça empêche une
 * réponse malveillante d'écrire ailleurs que dans le cache (« ../ »).
 */
bool identifiant_valide(const char *id);

/* « 2026-11-07 » → « Samedi 7 » ; "" si la date est invalide. */
void menu_libelle_jour(const char *date, char *sortie, int taille);

#endif
