/* Photo d'une recette (voir photo.h). */
#include "photo.h"

#include <string.h>

bool photo_valider(const uint8_t *d, int lg, int *largeur, int *hauteur)
{
    if (lg < PHOTO_ENTETE || memcmp(d, "R565", 4) != 0) return false;
    int l = d[4] | (d[5] << 8);
    int h = d[6] | (d[7] << 8);
    if (l < 1 || l > PHOTO_MAX_L || h < 1 || h > PHOTO_MAX_H) return false;
    if (lg != PHOTO_ENTETE + l * h * 2) return false;
    *largeur = l;
    *hauteur = h;
    return true;
}

uint32_t photo_position_tuilee(uint32_t x, uint32_t y)
{
    /* Tuile de 8 × 8 (64 pixels), puis ordre Z des bits de x et y dans la tuile. */
    uint32_t tuile = ((y >> 3) * (PHOTO_TEX_L >> 3) + (x >> 3)) << 6;
    uint32_t z = (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2)
                 | ((x & 4) << 2) | ((y & 4) << 3);
    return tuile + z;
}

void photo_tuiler(const uint8_t *pixels, int largeur, int hauteur, uint16_t *texture)
{
    for (int y = 0; y < hauteur; y++)
        for (int x = 0; x < largeur; x++) {
            const uint8_t *p = pixels + 2 * (y * largeur + x);
            texture[photo_position_tuilee((uint32_t)x, (uint32_t)y)] = (uint16_t)(p[0] | (p[1] << 8));
        }
}
