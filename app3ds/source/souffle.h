/*
 * Détection d'un souffle dans le micro, pour passer à l'étape suivante
 * sans toucher la console.
 *
 * Principe : un souffle près du micro donne un son fort et continu. On mesure
 * le niveau de chaque bloc d'échantillons (moyenne des amplitudes, de 0 à 1) ;
 * le souffle est reconnu quand le niveau reste au-dessus du seuil pendant
 * SOUFFLE_DUREE_MS. Un bruit bref (casserole, claquement) ou la voix (des
 * syllabes séparées par de courts silences) ne suffisent pas.
 *
 * Après un déclenchement :
 *   - SOUFFLE_PAUSE_MS de pause, pendant lesquels on n'écoute pas ;
 *   - puis il faut un retour au calme (SOUFFLE_CALME_MS sous le seuil), pour
 *     qu'un long souffle ne fasse pas avancer de deux étapes.
 *
 * Le seuil dépend de la sensibilité réglée (1 = peu sensible, 5 = très
 * sensible). Portable : testé sur le Mac avec des sons simulés.
 */
#ifndef SOUFFLE_H
#define SOUFFLE_H

#include <stdbool.h>
#include <stdint.h>

#define SOUFFLE_FREQUENCE     16364   /* échantillons par seconde (MICU_SAMPLE_RATE_16360) */
#define SOUFFLE_DUREE_MS      250     /* durée minimale au-dessus du seuil */
#define SOUFFLE_TROU_MS       80      /* creux toléré sans remettre le compte à zéro */
#define SOUFFLE_PAUSE_MS      1500    /* pause après un déclenchement */
#define SOUFFLE_CALME_MS      300     /* calme exigé avant de réécouter */
#define SOUFFLE_SENSIBILITE_MIN 1
#define SOUFFLE_SENSIBILITE_MAX 5
#define SOUFFLE_SENSIBILITE_DEFAUT 3

typedef struct {
    float    seuil;           /* niveau à dépasser (0..1) */
    int      au_dessus_ms;    /* temps passé au-dessus du seuil */
    int      trou_ms;         /* creux en cours */
    int      calme_ms;        /* temps passé sous le seuil depuis le dernier déclenchement */
    bool     arme;            /* prêt à se déclencher */
    uint64_t pause_jusqu_a;
    float    niveau;          /* dernier niveau mesuré, pour la jauge */
} Souffle;

/* Seuil correspondant à une sensibilité (1 à 5). */
float souffle_seuil(int sensibilite);

void souffle_init(Souffle *s, int sensibilite);
void souffle_regler(Souffle *s, int sensibilite);

/* Niveau d'un bloc : moyenne des écarts à la moyenne, ramenée entre 0 et 1. */
float souffle_niveau(const int16_t *echantillons, int nombre);

/*
 * Analyse un bloc d'échantillons reçu à l'instant `maintenant` (ms).
 * Renvoie true si un souffle vient d'être reconnu.
 */
bool souffle_analyser(Souffle *s, const int16_t *echantillons, int nombre, uint64_t maintenant);

/* Oublie l'état en cours (ex. pendant une alarme, pour ne pas s'entendre biper). */
void souffle_ignorer(Souffle *s, uint64_t jusqu_a);

#endif
