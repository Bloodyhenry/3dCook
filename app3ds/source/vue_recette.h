/*
 * Vue « recette » : on cuisine.
 *
 * Écran du haut (400×240) : l'étape en cours, en gros caractères ; la taille
 * de police est la plus grande qui fait tenir l'étape.
 * Écran du bas (320×240, tactile) : retour à la liste, portions (- / +),
 * ingrédients mis à l'échelle, minuteurs de l'étape (toucher pour lancer),
 * gros boutons Précédent / Suivant.
 * Les minuteurs en cours s'affichent en bas de l'écran du haut.
 *
 * Commandes : A, R ou → : étape suivante (ou souffler dans le micro, ou
 *             passer la main devant la caméra) ;
 *             B, L ou ← : précédente ;
 *             ↑ / ↓ : ingrédients ; X / Y : portions + / - ;
 *             SELECT : retour à la liste.
 */
#ifndef VUE_RECETTE_H
#define VUE_RECETTE_H

#include "dessin.h"
#include "minuterie.h"
#include "mise_en_page.h"
#include "recette.h"

#define VR_MAX_LIGNES_INGR 112

typedef enum { LIGNE_NORMALE, LIGNE_ALLERGENE, LIGNE_TITRE } GenreLigne;
typedef enum { VR_RIEN, VR_RETOUR } ActionRecette;

typedef struct {
    const Recette *recette;
    int  etape;
    int  portions;          /* si la recette connaît ses portions */
    int  indice_mult;       /* sinon : indice dans ECHELLE_MULTIPLICATEURS */
    int  defilement;        /* première ligne d'ingrédient affichée */

    /* Mises en page recalculées seulement quand quelque chose change. */
    bool       etape_a_refaire;
    bool       ingredients_a_refaire;
    bool       avec_bandeau;     /* bandeau des minuteurs affiché en haut */

    /* Micro et caméra, renseignés par la boucle principale pour les
     * indicateurs de l'en-tête. */
    bool       micro_actif;
    float      micro_niveau, micro_seuil;
    bool       camera_active;
    float      camera_niveau, camera_seuil;
    uint64_t   geste_jusqu_a;    /* indicateurs allumés après un souffle ou une main reconnus */
    Paragraphe texte_etape;
    int        nb_lignes_ingr;
    char       lignes_ingr[VR_MAX_LIGNES_INGR][MEP_TAILLE_LIGNE];
    GenreLigne genres_ingr[VR_MAX_LIGNES_INGR];
} VueRecette;

/* `echelle` : multiplicateur choisi dans le planning (1 par défaut). */
void vr_init(VueRecette *v, const Recette *recette, double echelle);
/* Étape suivante (souffle) ; renvoie false si on est déjà à la dernière. */
bool vr_suivante(VueRecette *v);

ActionRecette vr_entrees(VueRecette *v, u32 appuis, const touchPosition *toucher,
                         Minuterie *minuterie, uint64_t maintenant);
void vr_dessiner(VueRecette *v, C2D_TextBuf tampon, C3D_RenderTarget *haut, C3D_RenderTarget *bas,
                 const Minuterie *minuterie, uint64_t maintenant);

#endif
