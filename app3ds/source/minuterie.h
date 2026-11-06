/*
 * Minuteurs de cuisine : plusieurs en parallèle, indépendants de l'écran
 * affiché (ils continuent quand on change d'étape ou de recette).
 *
 * L'heure est fournie par l'appelant en millisecondes (osGetTime() sur la
 * console, une horloge factice dans les tests) : ce module est portable.
 *
 * Arrêter un minuteur qui tourne demande deux touchers (le premier affiche
 * « Arrêter ? » pendant 3 s) pour éviter de l'annuler du bout d'un doigt
 * pressé. Un minuteur qui sonne s'arrête au premier toucher.
 */
#ifndef MINUTERIE_H
#define MINUTERIE_H

#include <stdbool.h>
#include <stdint.h>

#include "recette.h"

#define MINUTERIE_MAX        6
#define MINUTERIE_CONFIRM_MS 3000

typedef struct {
    bool     actif;          /* en cours ou en train de sonner */
    bool     sonne;
    char     rid[TAILLE_ID]; /* recette d'origine */
    int      def;            /* indice dans Recette.minuteurs */
    char     libelle[TAILLE_LIBELLE];
    int      secondes;       /* durée lancée */
    int      secondes_max;   /* haut de la plage, 0 sinon */
    uint64_t fin_ms;
    uint64_t confirmer_jusqu_a;   /* « Arrêter ? » affiché jusqu'à cette heure */
} Minuteur;

typedef struct {
    Minuteur m[MINUTERIE_MAX];
} Minuterie;

typedef enum { TOUCHER_RIEN, TOUCHER_LANCE, TOUCHER_CONFIRMER, TOUCHER_ARRETE,
               TOUCHER_PLEIN } ResultatToucher;

void minuterie_init(Minuterie *t);

/* Indice du minuteur (rid, def) actif, ou -1. */
int minuterie_trouver(const Minuterie *t, const char *rid, int def);

/* Lance un minuteur ; s'il tourne déjà, renvoie son indice. -1 si tout est plein. */
int minuterie_lancer(Minuterie *t, const char *rid, int def, const char *libelle,
                     int secondes, int secondes_max, uint64_t maintenant);

/* Secondes restantes, arrondies au-dessus (0 quand c'est fini). */
int minuterie_restant(const Minuteur *m, uint64_t maintenant);

/* Fait sonner les minuteurs arrivés à zéro ; renvoie combien viennent de sonner. */
int minuterie_maj(Minuterie *t, uint64_t maintenant);

bool minuterie_sonne(const Minuterie *t);
bool minuterie_active(const Minuterie *t);

/* Arrête tous les minuteurs qui sonnent ; renvoie leur nombre. */
int minuterie_arreter_sonneries(Minuterie *t);

void minuterie_arreter(Minuterie *t, int indice);

/*
 * Toucher du bouton d'un minuteur défini (rid, def) :
 * pas lancé → lancé ; sonne → arrêté ; tourne → « Arrêter ? », puis arrêté
 * si on touche de nouveau dans les 3 s.
 */
ResultatToucher minuterie_toucher(Minuterie *t, const char *rid, int def, const char *libelle,
                                  int secondes, int secondes_max, uint64_t maintenant);

/* Indices des minuteurs actifs : ceux qui sonnent, puis par échéance. */
int minuterie_ordre(const Minuterie *t, int *indices, int max);

/* 754 → « 12:34 », 3723 → « 1:02:03 ». */
void minuterie_formater(int secondes, char *sortie, int taille);

#endif
