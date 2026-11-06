/*
 * Tests de la détection du souffle avec des sons simulés, envoyés comme sur
 * la console : un bloc d'environ 1/60 s (273 échantillons) par image.
 */
#include <stdlib.h>

#include "souffle.h"
#include "verif.h"

#define BLOC 273   /* ≈ 16 364 / 60 */

static Souffle s_global;
static uint64_t horloge;
static int16_t bloc[BLOC];

/* Bruit aléatoire d'amplitude `a` (0..1) autour d'un décalage continu `dc`. */
static void bruit(float a, int dc)
{
    for (int i = 0; i < BLOC; i++) {
        float x = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * a;
        int v = (int)(x * 32767.0f) + dc;
        bloc[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
}

/* Joue `ms` millisecondes de bruit d'amplitude `a` ; renvoie le nombre de déclenchements. */
static int jouer(float a, int ms)
{
    int declenchements = 0;
    for (int t = 0; t < ms; t += 1000 / 60) {
        bruit(a, 0);
        horloge += 1000 / 60;
        if (souffle_analyser(&s_global, bloc, BLOC, horloge)) declenchements++;
    }
    return declenchements;
}


int main(void)
{
    srand(7);
    horloge = 1000000;

    /* Niveau : silence ≈ 0, bruit fort élevé, décalage continu ignoré. */
    for (int i = 0; i < BLOC; i++) bloc[i] = 1200;           /* décalage pur */
    VERIF_NOMBRE(souffle_niveau(bloc, BLOC), 0);
    bruit(0.8f, 3000);
    float n = souffle_niveau(bloc, BLOC);
    VERIF_NOMBRE(n > 0.35f && n < 0.45f, 1);                   /* bruit uniforme : moyenne a/2 */
    VERIF_NOMBRE(souffle_niveau(bloc, 0), 0);

    /* Seuils : plus sensible = seuil plus bas ; bornes. */
    VERIF_NOMBRE(souffle_seuil(1) > souffle_seuil(5), 1);
    VERIF_NOMBRE(souffle_seuil(0), souffle_seuil(1));
    VERIF_NOMBRE(souffle_seuil(9), souffle_seuil(5));

    souffle_init(&s_global, SOUFFLE_SENSIBILITE_DEFAUT);     /* seuil 0,20 */

    /* Silence, puis bruit de cuisine modéré : rien. */
    VERIF_NOMBRE(jouer(0.01f, 2000), 0);
    VERIF_NOMBRE(jouer(0.15f, 2000), 0);                     /* niveau ≈ 0,075 */

    /* Claquement bref (casserole) très fort : rien. */
    VERIF_NOMBRE(jouer(0.9f, 100), 0);
    VERIF_NOMBRE(jouer(0.01f, 500), 0);

    /* Voix : syllabes fortes de 150 ms séparées de 120 ms de silence : rien. */
    int voix = 0;
    for (int k = 0; k < 8; k++) {
        voix += jouer(0.6f, 150);
        voix += jouer(0.02f, 120);
    }
    VERIF_NOMBRE(voix, 0);
    VERIF_NOMBRE(jouer(0.01f, 500), 0);

    /* Souffle de 400 ms : un déclenchement. */
    VERIF_NOMBRE(jouer(0.7f, 400), 1);
    VERIF_NOMBRE(s_global.niveau > s_global.seuil, 1);

    /* Long souffle de 3 s : un seul déclenchement, pas un toutes les 250 ms. */
    VERIF_NOMBRE(jouer(0.01f, 2000), 0);
    VERIF_NOMBRE(jouer(0.7f, 3000), 1);

    /* Deux souffles séparés de 2 s de calme : deux déclenchements. */
    VERIF_NOMBRE(jouer(0.01f, 2000), 0);
    int deux = jouer(0.7f, 400);
    deux += jouer(0.01f, 2000);
    deux += jouer(0.7f, 400);
    VERIF_NOMBRE(deux, 2);

    /* Souffle pendant la pause qui suit un déclenchement : ignoré. */
    VERIF_NOMBRE(jouer(0.01f, 2000), 0);
    VERIF_NOMBRE(jouer(0.7f, 400), 1);
    VERIF_NOMBRE(jouer(0.01f, 400), 0);
    VERIF_NOMBRE(jouer(0.7f, 400), 0);                       /* encore en pause (1,5 s) */

    /* Un petit creux de 50 ms au milieu d'un souffle est toléré. */
    VERIF_NOMBRE(jouer(0.01f, 2000), 0);
    int creux = jouer(0.7f, 170);
    creux += jouer(0.01f, 50);
    creux += jouer(0.7f, 170);
    VERIF_NOMBRE(creux, 1);

    /* Pendant une alarme : on ignore tout. */
    VERIF_NOMBRE(jouer(0.01f, 2000), 0);
    souffle_ignorer(&s_global, horloge + 1000);
    VERIF_NOMBRE(jouer(0.7f, 600), 0);

    /* Sensibilité : à 1, un souffle moyen ne suffit plus ; à 5, si. */
    VERIF_NOMBRE(jouer(0.01f, 2000), 0);
    souffle_regler(&s_global, 1);                             /* seuil 0,40 */
    VERIF_NOMBRE(jouer(0.5f, 500), 0);                        /* niveau ≈ 0,25 */
    VERIF_NOMBRE(jouer(0.01f, 2000), 0);
    souffle_regler(&s_global, 5);                             /* seuil 0,09 */
    VERIF_NOMBRE(jouer(0.5f, 500), 1);

    return VERIF_BILAN("test_souffle");
}
