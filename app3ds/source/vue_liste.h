/*
 * Vue « liste » : le menu de la semaine (recettes gardées sur la carte SD).
 *
 * Écran du haut : titre de la recette sélectionnée en grand, son jour et son
 * repas, l'état de la dernière synchronisation et l'adresse de la passerelle.
 * Écran du bas : une ligne par recette (toucher = ouvrir), boutons de page,
 * « Synchro » (télécharger le menu) et « Réglages » (adresse, micro).
 *
 * Commandes : ↑ / ↓ : choisir ; A : ouvrir ; L / R : page précédente /
 *             suivante ; Y : synchroniser ; X : réglages ;
 *             SELECT : recette d'exemple (minuteur de test de 10 s) ;
 *             START : quitter. Sans recette : A ouvre aussi la recette d'exemple.
 */
#ifndef VUE_LISTE_H
#define VUE_LISTE_H

#include "catalogue.h"
#include "config.h"
#include "dessin.h"
#include "minuterie.h"
#include "mise_en_page.h"

typedef enum { VL_RIEN, VL_OUVRIR, VL_EXEMPLE, VL_SYNCHRO, VL_REGLAGES } ActionListe;

typedef struct {
    const Catalogue *catalogue;
    char adresse[TAILLE_ADRESSE];   /* passerelle réglée, "" sinon */
    int  selection;                 /* indice dans catalogue->entrees */
    int  premier;                   /* première ligne affichée en bas */
    bool a_preparer;                /* textes à remettre en page */
    char titres_courts[CATALOGUE_MAX][TAILLE_TITRE];
    int  titre_mis_en_page;         /* sélection pour laquelle `titre` est calculé */
    Paragraphe titre;
    char message[300];              /* résultat de synchro ou erreur, "" sinon */
    bool message_erreur;            /* message en orange (sinon en gris) */
} VueListe;

void vl_init(VueListe *v, const Catalogue *catalogue, const char *adresse);
void vl_message(VueListe *v, const char *message, bool erreur);
ActionListe vl_entrees(VueListe *v, u32 appuis, const touchPosition *toucher);
void vl_dessiner(VueListe *v, C2D_TextBuf tampon, C3D_RenderTarget *haut, C3D_RenderTarget *bas,
                 const Minuterie *minuterie, uint64_t maintenant);

/* Écran affiché pendant la synchronisation (« 2 / 5 : Acras de poisson »). */
void vl_dessiner_progres(C2D_TextBuf tampon, C3D_RenderTarget *haut, C3D_RenderTarget *bas,
                         int fait, int total, const char *titre);

#endif
