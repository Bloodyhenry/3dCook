/* Tests de la détection de main, avec des images simulées à 15 images/s. */
#include <stdlib.h>

#include "mouvement.h"
#include "verif.h"

static Mouvement m;
static uint64_t horloge = 1000000;
static uint8_t scene[MVT_L * MVT_H], image[MVT_L * MVT_H];

/* Image = scène fixe texturée + bruit ±3 + décalage de lumière, avec en option
 * un carré sombre (main ou objet) de côté `c` en (x, y). */
static void composer(int lumiere, int c, int x, int y)
{
    for (int j = 0; j < MVT_H; j++)
        for (int i = 0; i < MVT_L; i++) {
            int v = scene[j * MVT_L + i] + lumiere + rand() % 7 - 3;
            if (c && i >= x && i < x + c && j >= y && j < y + c) v = 20 + rand() % 7;
            image[j * MVT_L + i] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
        }
}

static int images(int n, int lumiere, int c, int x, int y)
{
    int d = 0;
    for (int k = 0; k < n; k++) {
        composer(lumiere, c, x, y);
        horloge += 66;
        d += mvt_analyser(&m, image, horloge);
    }
    return d;
}

int main(void)
{
    srand(3);
    for (int i = 0; i < MVT_L * MVT_H; i++) scene[i] = (uint8_t)(60 + rand() % 140);
    mvt_init(&m, 3);

    /* Réduction YUYV : 160×120 uniforme → 40×30 de même gris. */
    static uint8_t yuyv[160 * 120 * 2];
    for (int i = 0; i < 160 * 120; i++) { yuyv[2 * i] = 90; yuyv[2 * i + 1] = 128; }
    uint8_t red[MVT_L * MVT_H];
    mvt_reduire(yuyv, 160, 120, red);
    VERIF_NOMBRE(red[0] == 90 && red[MVT_L * MVT_H - 1] == 90, 1);

    /* Réduction RGB565 : blanc → ~250, noir → 0, vert pur plus clair que bleu pur. */
    static uint16_t rgb[160 * 120];
    for (int i = 0; i < 160 * 120; i++) rgb[i] = 0xFFFF;
    mvt_reduire_rgb565(rgb, 160, 120, red);
    VERIF_NOMBRE(red[0] >= 245, 1);
    for (int i = 0; i < 160 * 120; i++) rgb[i] = 0;
    mvt_reduire_rgb565(rgb, 160, 120, red);
    VERIF_NOMBRE(red[0], 0);
    uint8_t vert[MVT_L * MVT_H], bleu[MVT_L * MVT_H];
    for (int i = 0; i < 160 * 120; i++) rgb[i] = 0x07E0;
    mvt_reduire_rgb565(rgb, 160, 120, vert);
    for (int i = 0; i < 160 * 120; i++) rgb[i] = 0x001F;
    mvt_reduire_rgb565(rgb, 160, 120, bleu);
    VERIF_NOMBRE(vert[0] > bleu[0], 1);

    VERIF_NOMBRE(images(30, 0, 0, 0, 0), 0);                 /* scène immobile */
    VERIF_NOMBRE(images(30, 0, 0, 0, 0) + images(10, 50, 0, 0, 0), 0);   /* lumière qui saute */
    int loin = 0;                                              /* petit objet qui bouge (au loin) */
    for (int k = 0; k < 20; k++) loin += images(1, 0, 6, k, 10);
    VERIF_NOMBRE(loin, 0);
    VERIF_NOMBRE(images(15, 0, 0, 0, 0), 0);

    /* Main qui passe : grand carré qui traverse en ~0,4 s → un déclenchement. */
    int main1 = 0;
    for (int x = -24; x < 40; x += 10) main1 += images(1, 0, 26, x < 0 ? 0 : x, 2);
    VERIF_NOMBRE(main1, 1);
    VERIF_NOMBRE(images(30, 0, 0, 0, 0), 0);                 /* retour au calme */

    /* Main laissée devant 3 s : l'arrivée puis le départ comptent (limite connue :
     * il faut passer la main, pas la laisser). */
    VERIF_NOMBRE(images(45, 0, 30, 5, 0), 1);
    VERIF_NOMBRE(images(30, 0, 0, 0, 0), 1);                 /* départ de la main */
    VERIF_NOMBRE(images(40, 0, 0, 0, 0), 0);

    /* Deuxième passage après le calme : déclenche de nouveau. */
    int main2 = 0;
    for (int x = 0; x < 40; x += 8) main2 += images(1, 0, 26, x, 2);
    VERIF_NOMBRE(main2, 1);

    /* Ignoré (après un toucher, pendant l'alarme). */
    VERIF_NOMBRE(images(30, 0, 0, 0, 0), 0);
    mvt_ignorer(&m, horloge + 2000);
    int ign = 0;
    for (int x = 0; x < 40; x += 8) ign += images(1, 0, 26, x, 2);
    VERIF_NOMBRE(ign, 0);

    /* Sensibilité bornée et ordonnée. */
    VERIF_NOMBRE(mvt_seuil(1) > mvt_seuil(5), 1);
    VERIF_NOMBRE(mvt_seuil(0), mvt_seuil(1));
    return VERIF_BILAN("test_mouvement");
}
