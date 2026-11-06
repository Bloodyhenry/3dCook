/*
 * Catalogue des recettes présentes sur la carte SD.
 *
 * Les recettes sont des fichiers <id>.json (format de la passerelle) dans
 * le dossier DOSSIER_RECETTES. Aux étapes suivantes, l'appli y rangera
 * elle-même les recettes téléchargées ; pour l'instant on les copie à la main.
 *
 * Portable (stdio + dirent) : testé sur le Mac avec un dossier temporaire.
 */
#ifndef CATALOGUE_H
#define CATALOGUE_H

#include "menu.h"
#include "recette.h"

#define DOSSIER_RECETTES     "sdmc:/3ds/recettes/cache"
#define CATALOGUE_MAX        64
#define TAILLE_FICHIER_MAX   (64 * 1024)   /* une recette fait 2 à 6 Ko */
#define TAILLE_NOM_FICHIER   64

typedef struct {
    char fichier[TAILLE_NOM_FICHIER];
    char id[TAILLE_ID];
    char rev[12];
    char titre[TAILLE_TITRE];
    int  nb_etapes;
    int  nb_minuteurs;
    bool a_portions;
    int  portions_nombre;
    char portions_unite[TAILLE_UNITE];
    /* Renseignés par catalogue_appliquer_menu pour les recettes du menu. */
    bool   dans_menu;
    char   date[11];
    char   repas[24];
    double echelle;
} EntreeCatalogue;

typedef struct {
    int  nb;
    /* Triées par titre (sans tenir compte des accents), ou dans l'ordre du menu
     * après catalogue_appliquer_menu. */
    EntreeCatalogue entrees[CATALOGUE_MAX];
    int  nb_illisibles;                       /* fichiers .json qu'on n'a pas pu lire */
    char erreur[200];                         /* « fichier : raison » du premier illisible */
    bool dossier_absent;
    bool plein;                               /* plus de CATALOGUE_MAX recettes */
} Catalogue;

/* Crée le dossier (et ses parents) s'il n'existe pas. */
void catalogue_preparer(const char *dossier);

/* Parcourt le dossier et lit le titre de chaque recette. */
void catalogue_lire(Catalogue *c, const char *dossier);

/*
 * Met les recettes du menu en tête, dans l'ordre du menu (jour, repas), avec
 * leur date, leur repas et leur échelle. Les autres suivent, par titre.
 */
void catalogue_appliquer_menu(Catalogue *c, const Menu *m);

/* Charge une recette du catalogue. */
bool catalogue_charger(const char *dossier, const char *fichier, Recette *r,
                       char *erreur, int taille_erreur);

/* Clé de tri : minuscules sans accents (« Éclair » → « eclair »). */
void catalogue_cle_tri(const char *texte, char *sortie, int taille);

#endif
