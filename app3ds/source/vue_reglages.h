/*
 * Vue « Réglages » : adresse de la passerelle, commandes sans les mains
 * (micro et caméra, activés ou non, et leur sensibilité), avec une jauge en
 * direct pour chacun, pour les régler dans sa propre cuisine.
 *
 * Commandes : A : micro oui / non ; Y : caméra oui / non ;
 *             croix gauche / droite : sensibilité du souffle ;
 *             L / R : sensibilité de la main ; X : adresse ;
 *             B ou SELECT : retour à la liste.
 */
#ifndef VUE_REGLAGES_H
#define VUE_REGLAGES_H

#include "config.h"
#include "dessin.h"
#include "mouvement.h"
#include "souffle.h"

typedef enum { VRG_RIEN, VRG_RETOUR, VRG_ADRESSE, VRG_MODIFIE } ActionReglages;

typedef struct {
    Config  *config;
    uint64_t souffle_jusqu_a;   /* « Souffle détecté ! » affiché jusqu'à cette heure */
    uint64_t main_jusqu_a;      /* « Main détectée ! » */
} VueReglages;

void vg_init(VueReglages *v, Config *config);

/* VRG_MODIFIE : la config a changé (micro, caméra, sensibilités), à enregistrer. */
ActionReglages vg_entrees(VueReglages *v, u32 appuis, const touchPosition *toucher);

/* Un souffle ou une main vient d'être reconnu : on l'affiche. */
void vg_signaler_souffle(VueReglages *v, uint64_t maintenant);
void vg_signaler_main(VueReglages *v, uint64_t maintenant);

void vg_dessiner(VueReglages *v, C2D_TextBuf tampon, C3D_RenderTarget *haut, C3D_RenderTarget *bas,
                 const Souffle *souffle, bool micro_pret,
                 const Mouvement *mouvement, bool camera_prete, uint64_t maintenant);

#endif
