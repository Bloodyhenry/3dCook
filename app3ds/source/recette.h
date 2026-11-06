/*
 * Modèle de données d'une recette, calqué sur le JSON de la passerelle
 * (docs/schema.md). Tailles fixes : pas d'allocation dynamique, et une
 * recette tient dans environ 40 Ko.
 *
 * Ce fichier ne dépend pas de libctru : il est aussi compilé sur le Mac
 * pour les tests.
 */
#ifndef RECETTE_H
#define RECETTE_H

#include <stdbool.h>

#define RECETTE_MAX_GROUPES     8
#define RECETTE_MAX_INGREDIENTS 48
#define RECETTE_MAX_ETAPES      32
#define RECETTE_MAX_MINUTEURS   32
#define RECETTE_MAX_NOTES       8

/* Tailles des textes, terminateur compris (octets UTF-8). */
#define TAILLE_ID       33
#define TAILLE_TITRE    128
#define TAILLE_UNITE    32
#define TAILLE_NOM      160
#define TAILLE_ETAPE    1280   /* plus longue étape de la base : 941 octets */
#define TAILLE_SECTION  80
#define TAILLE_LIBELLE  48
#define TAILLE_NOTE     320

typedef struct {
    bool   a_quantite;        /* false : ligne non analysée, jamais mise à l'échelle */
    double quantite;
    bool   a_quantite_max;    /* plage « 80-100 g » */
    double quantite_max;
    char   unite[TAILLE_UNITE];   /* "" si pas d'unité */
    char   nom[TAILLE_NOM];
    bool   allergene;
    int    groupe;            /* indice dans Recette.titres_groupes */
} Ingredient;

typedef struct {
    char texte[TAILLE_ETAPE];
    char section[TAILLE_SECTION];  /* "" si aucune */
} Etape;

typedef struct {
    int  etape;               /* indice dans Recette.etapes */
    int  secondes;            /* durée de départ */
    int  secondes_max;        /* 0 si pas de plage */
    char libelle[TAILLE_LIBELLE];
} DefMinuteur;

typedef struct {
    char id[TAILLE_ID];
    char titre[TAILLE_TITRE];
    char rev[12];

    bool a_portions;
    int  portions_nombre;
    char portions_unite[TAILLE_UNITE];

    bool congelable;
    bool image;

    int  nb_groupes;
    char titres_groupes[RECETTE_MAX_GROUPES][TAILLE_SECTION];   /* "" = sans titre */

    int        nb_ingredients;
    Ingredient ingredients[RECETTE_MAX_INGREDIENTS];

    int   nb_etapes;
    Etape etapes[RECETTE_MAX_ETAPES];

    int         nb_minuteurs;
    DefMinuteur minuteurs[RECETTE_MAX_MINUTEURS];

    int  nb_notes;
    char notes[RECETTE_MAX_NOTES][TAILLE_NOTE];

    bool tronquee;    /* la recette dépassait une limite : une partie a été ignorée */
} Recette;

/* Copie bornée qui termine toujours la chaîne et ne coupe pas un caractère UTF-8. */
void copier_texte(char *dest, const char *src, int taille);

/* Remplit `r` avec une recette d'exemple (étape 1 : pas encore de réseau ni de SD). */
void recette_demo(Recette *r);

#endif
