/*
 * Détection d'une main passée devant la caméra intérieure.
 *
 * On compare chaque image réduite (MVT_L × MVT_H, niveaux de gris) à la
 * précédente, après avoir retiré la luminosité moyenne de chacune : un
 * changement de lumière global (lampe, exposition automatique) s'annule.
 * Le « mouvement » est la part des pixels qui ont beaucoup changé. Une main
 * proche en change une grande partie ; une personne au fond de la pièce,
 * très peu. Une seule image au-dessus du seuil suffit : la main change
 * l'image d'un coup en arrivant, puis l'image redevient stable.
 * Ensuite, comme pour le souffle : pause, puis retour au calme exigé.
 *
 * Limite connue : une main LAISSÉE devant la caméra plus de 2 s compte deux
 * fois (à l'arrivée et au départ). Il faut la passer, pas la poser.
 *
 * Portable : testé sur le Mac avec des images simulées.
 */
#ifndef MOUVEMENT_H
#define MOUVEMENT_H

#include <stdbool.h>
#include <stdint.h>

#define MVT_L 40
#define MVT_H 30
#define MVT_ECART       28     /* différence de gris qui compte comme « changé » */
#define MVT_IMAGES      1      /* une main qui arrive change l'image d'un coup, puis plus rien */
#define MVT_PAUSE_MS    1500
#define MVT_CALME_MS    500

typedef struct {
    float    seuil;            /* part de pixels changés à atteindre (0..1) */
    uint8_t  precedente[MVT_L * MVT_H];
    bool     a_precedente;
    int      consecutives;
    int      calme_ms;
    bool     arme;
    uint64_t pause_jusqu_a;
    uint64_t derniere_ms;
    float    mouvement;        /* dernière mesure, pour la jauge */
} Mouvement;

float mvt_seuil(int sensibilite);              /* 1 (peu sensible) à 5 */
void  mvt_init(Mouvement *m, int sensibilite);
void  mvt_regler(Mouvement *m, int sensibilite);

/* Réduit une image YUV422 (YUYV, w × h) en MVT_L × MVT_H niveaux de gris. */
void  mvt_reduire(const uint8_t *yuyv, int w, int h, uint8_t *sortie);

/* Même chose depuis une image RGB565 (format de la caméra utilisé sur la console). */
void  mvt_reduire_rgb565(const uint16_t *pixels, int w, int h, uint8_t *sortie);

/* Part des pixels qui ont changé entre deux images, lumière moyenne retirée. */
float mvt_mesurer(const uint8_t *a, const uint8_t *b);

/* Analyse une nouvelle image ; true si une main vient d'être reconnue. */
bool  mvt_analyser(Mouvement *m, const uint8_t *image, uint64_t maintenant);

/* Ignore tout jusqu'à `jusqu_a` et oublie l'image précédente. */
void  mvt_ignorer(Mouvement *m, uint64_t jusqu_a);

#endif
