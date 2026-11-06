/*
 * Alarme des minuteurs et veille de la console.
 *
 * - Son : un double bip répété, produit par le programme (pas de fichier).
 *   On passe par ndsp, qui a besoin du firmware DSP (sdmc:/3ds/dspfirm.cdc,
 *   extrait une fois avec l'outil DSP1) ; à défaut, par csnd. Si aucun des
 *   deux ne marche, l'alarme reste visuelle (écran + LED).
 * - LED de notification : clignote en orange tant qu'un minuteur sonne.
 * - Veille : interdite tant qu'un minuteur tourne (aptSetSleepAllowed),
 *   pour que le décompte continue console fermée.
 */
#ifndef ALARME_H
#define ALARME_H

#include <stdbool.h>
#include <stdint.h>

void alarme_init(void);
void alarme_fin(void);

/* À appeler à chaque image : démarre ou arrête son et LED selon `sonne`,
 * et bloque la veille tant que `minuteur_actif`. */
void alarme_maj(bool sonne, bool minuteur_actif, uint64_t maintenant_ms);

/* Nom du système de son utilisé : « ndsp », « csnd » ou NULL (pas de son). */
const char *alarme_son(void);

#endif
