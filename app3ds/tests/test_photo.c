/* Tests de la photo : en-tête, taille, découpage en tuiles 8 × 8 en ordre Z. */
#include <stdlib.h>

#include "photo.h"
#include "verif.h"

static uint8_t fichier[PHOTO_TAILLE_MAX + 16];
static uint16_t texture[PHOTO_TEX_L * PHOTO_TEX_H];

static int fabriquer(int l, int h)
{
    memcpy(fichier, "R565", 4);
    fichier[4] = (uint8_t)(l & 0xFF); fichier[5] = (uint8_t)(l >> 8);
    fichier[6] = (uint8_t)(h & 0xFF); fichier[7] = (uint8_t)(h >> 8);
    /* Pixel (x, y) = valeur unique x + 400 × y, pour retrouver chacun. */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < l; x++) {
            uint16_t v = (uint16_t)(x + 400 * y);
            uint8_t *p = fichier + PHOTO_ENTETE + 2 * (y * l + x);
            p[0] = (uint8_t)(v & 0xFF);
            p[1] = (uint8_t)(v >> 8);
        }
    return PHOTO_ENTETE + l * h * 2;
}

int main(void)
{
    int l, h;

    /* En-tête et taille */
    int lg = fabriquer(360, 240);
    VERIF_NOMBRE(photo_valider(fichier, lg, &l, &h), 1);
    VERIF_NOMBRE(l == 360 && h == 240, 1);
    VERIF_NOMBRE(photo_valider(fichier, lg - 1, &l, &h), 0);          /* fichier coupé */
    VERIF_NOMBRE(photo_valider(fichier, 4, &l, &h), 0);
    fichier[0] = 'X';
    VERIF_NOMBRE(photo_valider(fichier, lg, &l, &h), 0);              /* mauvaise signature */
    lg = fabriquer(401, 10);
    VERIF_NOMBRE(photo_valider(fichier, lg, &l, &h), 0);              /* trop large */
    lg = fabriquer(10, 0);
    VERIF_NOMBRE(photo_valider(fichier, lg, &l, &h), 0);              /* vide */

    /* Positions tuilées (ordre Z dans une tuile 8 × 8, tuiles de gauche à droite) */
    VERIF_NOMBRE(photo_position_tuilee(0, 0), 0);
    VERIF_NOMBRE(photo_position_tuilee(1, 0), 1);
    VERIF_NOMBRE(photo_position_tuilee(0, 1), 2);
    VERIF_NOMBRE(photo_position_tuilee(1, 1), 3);
    VERIF_NOMBRE(photo_position_tuilee(2, 0), 4);
    VERIF_NOMBRE(photo_position_tuilee(7, 7), 63);
    VERIF_NOMBRE(photo_position_tuilee(8, 0), 64);                    /* tuile suivante */
    VERIF_NOMBRE(photo_position_tuilee(0, 8), 64 * 64);               /* rangée de 64 tuiles */
    VERIF_NOMBRE(photo_position_tuilee(511, 255), 512 * 256 - 1);     /* dernier pixel */

    /* Toutes les positions d'une texture pleine sont distinctes (aucun écrasement). */
    static uint8_t vu[PHOTO_TEX_L * PHOTO_TEX_H];
    int doublons = 0;
    for (uint32_t y = 0; y < PHOTO_TEX_H; y++)
        for (uint32_t x = 0; x < PHOTO_TEX_L; x++) doublons += vu[photo_position_tuilee(x, y)]++ > 0;
    VERIF_NOMBRE(doublons, 0);

    /* Tuilage d'une vraie photo 400 × 240 : chaque pixel au bon endroit. */
    lg = fabriquer(400, 240);
    VERIF_NOMBRE(photo_valider(fichier, lg, &l, &h), 1);
    photo_tuiler(fichier + PHOTO_ENTETE, l, h, texture);
    int faux = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < l; x++)
            faux += texture[photo_position_tuilee((uint32_t)x, (uint32_t)y)] != (uint16_t)(x + 400 * y);
    VERIF_NOMBRE(faux, 0);

    return VERIF_BILAN("test_photo");
}
