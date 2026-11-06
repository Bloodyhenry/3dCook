/*
 * Capture du micro de la console (service mic de libctru).
 *
 * Le micro enregistre en continu, en PCM 16 bits signé à 16 364 Hz, dans un
 * tampon circulaire partagé (aligné sur 0x1000). micro_lire() rend les
 * échantillons arrivés depuis l'appel précédent (environ 273 par image).
 */
#ifndef MICRO_H
#define MICRO_H

#include <stdbool.h>
#include <stdint.h>

bool micro_init(void);   /* false si le micro est indisponible */
void micro_fin(void);
bool micro_pret(void);

/* Copie les nouveaux échantillons dans `sortie` (au plus `max`) ; renvoie leur nombre. */
int micro_lire(int16_t *sortie, int max);

#endif
