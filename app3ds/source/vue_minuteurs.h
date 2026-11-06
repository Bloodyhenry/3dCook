/*
 * Affichage des minuteurs, commun aux écrans :
 * - bandeau en bas de l'écran du haut (lisible de loin) avec les minuteurs
 *   en cours, qui clignote quand l'un d'eux sonne ;
 * - alerte plein écran sur l'écran du bas quand un minuteur sonne
 *   (n'importe quel toucher ou bouton l'arrête) ;
 * - bouton d'un minuteur d'étape (lancer / temps restant / « Arrêter ? »).
 */
#ifndef VUE_MINUTEURS_H
#define VUE_MINUTEURS_H

#include "dessin.h"
#include "minuterie.h"

#define BANDEAU_Y 194
#define BANDEAU_H 38

/* Dessine le bandeau sur l'écran du haut (scène déjà ouverte) s'il y a des minuteurs. */
void vm_bandeau(C2D_TextBuf tampon, const Minuterie *t, uint64_t maintenant);

/* Alerte plein écran sur l'écran du bas (scène déjà ouverte), si un minuteur sonne. */
void vm_alerte(C2D_TextBuf tampon, const Minuterie *t, uint64_t maintenant);

/* Bouton d'un minuteur défini dans la recette (rid, def). */
void vm_bouton(C2D_TextBuf tampon, const Zone *z, const Minuterie *t, const char *rid, int def,
               const char *libelle, uint64_t maintenant);

/* Clignotement : vrai une demi-seconde sur deux. */
bool vm_clignote(uint64_t maintenant);

#endif
