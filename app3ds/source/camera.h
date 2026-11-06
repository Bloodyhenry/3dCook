/*
 * Capture de la caméra intérieure (au-dessus de l'écran du haut), avec le
 * service cam de libctru : images 160×120 en RGB565 à 15 images/s, réduites
 * en 40×30 niveaux de gris pour la détection de mouvement (mouvement.h).
 *
 * La caméra n'est allumée que quand on en a besoin (recette ou Réglages,
 * caméra activée) : elle consomme de la batterie.
 */
#ifndef CAMERA_H
#define CAMERA_H

#include <stdbool.h>
#include <stdint.h>

bool camera_demarrer(void);   /* false si la caméra est indisponible */
void camera_arreter(void);
bool camera_active(void);

/*
 * Si une nouvelle image est arrivée, la réduit dans `reduite`
 * (MVT_L × MVT_H) et renvoie true. Ne bloque jamais. Relance la capture
 * si elle s'est interrompue (erreur de tampon, retour du menu HOME…).
 */
bool camera_image(uint8_t *reduite, uint64_t maintenant_ms);

#endif
