/*
 * Photo d'une recette, au format préparé par la passerelle
 * (GET /recipes/<id>/image.rgb565) :
 *   « R565 », largeur, hauteur (u16 petit-boutiste), puis les pixels ligne
 *   par ligne depuis le haut, en RGB565 petit-boutiste.
 * Pas de décodage JPEG sur la console : il n'y a qu'à réordonner les pixels
 * en tuiles pour le GPU.
 *
 * Le GPU lit une texture de 512 × 256 découpée en tuiles de 8 × 8 pixels,
 * et, dans chaque tuile, les pixels sont rangés en « ordre Z » (Morton).
 *
 * Portable : testé sur le Mac.
 */
#ifndef PHOTO_H
#define PHOTO_H

#include <stdbool.h>
#include <stdint.h>

#define PHOTO_TEX_L      512
#define PHOTO_TEX_H      256
#define PHOTO_MAX_L      400
#define PHOTO_MAX_H      240
#define PHOTO_ENTETE     8
#define PHOTO_TAILLE_MAX (PHOTO_ENTETE + PHOTO_MAX_L * PHOTO_MAX_H * 2)
#define PHOTO_EXTENSION  ".r565"   /* fichier du cache : <id>.r565 */

/* Vérifie l'en-tête et la taille ; renvoie la largeur et la hauteur. */
bool photo_valider(const uint8_t *donnees, int longueur, int *largeur, int *hauteur);

/* Position (en pixels) du pixel (x, y) dans la texture tuilée de 512 de large. */
uint32_t photo_position_tuilee(uint32_t x, uint32_t y);

/*
 * Range les pixels (après l'en-tête) dans `texture` (512 × 256 pixels RGB565),
 * en haut à gauche. Le reste de la texture n'est pas touché.
 */
void photo_tuiler(const uint8_t *pixels, int largeur, int hauteur, uint16_t *texture);

#endif
