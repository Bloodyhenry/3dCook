/*
 * Affichage de la photo d'une recette (fichier <id>.r565 du cache, voir
 * photo.h) : une seule texture 512 × 256 en mémoire, rechargée quand on
 * change de recette.
 */
#ifndef PHOTO_ECRAN_H
#define PHOTO_ECRAN_H

#include <stdbool.h>

/* Charge la photo de la recette `id` (rien à faire si c'est déjà elle).
 * Renvoie false s'il n'y en a pas. À appeler entre C3D_FrameBegin et C3D_FrameEnd. */
bool photo_ecran_charger(const char *dossier, const char *id);

/* Oublie la photo chargée (après une synchronisation, elle a pu changer). */
void photo_ecran_oublier(void);

/* Dessine la photo chargée, centrée sur l'écran du haut (scène déjà ouverte). */
void photo_ecran_dessiner(void);

void photo_ecran_fin(void);

#endif
