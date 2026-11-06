/*
 * Outils de dessin communs aux écrans (citro2d) : couleurs, texte, boutons,
 * mesure de largeur pour la mise en page.
 * Thème sombre et contrasté, pour une lecture à 50 cm dans une cuisine.
 * Police : police système de la console (accents français compris).
 */
#ifndef DESSIN_H
#define DESSIN_H

#include <3ds.h>
#include <citro2d.h>

#define COULEUR(rgb) C2D_Color32(((rgb) >> 16) & 0xFF, ((rgb) >> 8) & 0xFF, (rgb) & 0xFF, 0xFF)
#define C_FOND        COULEUR(0x1C1A17)
#define C_TEXTE       COULEUR(0xF2EDE4)
#define C_DOUX        COULEUR(0xA59B8F)
#define C_ACCENT      COULEUR(0xE0774C)
#define C_ALLERGENE   COULEUR(0xF0A04B)
#define C_BOUTON      COULEUR(0x3A352F)
#define C_BOUTON_OFF  COULEUR(0x26231F)
#define C_TEXTE_FONCE COULEUR(0x1C1A17)

/*
 * Profondeur : TOUT se dessine à la même profondeur, PROFONDEUR (0,5).
 * À profondeur égale, ce qui est dessiné en dernier passe devant : un fond
 * dessiné puis son texte fonctionne. Un fond plus profond (0,6, 0,7…)
 * passerait devant le texte et le cacherait.
 */
#define PROFONDEUR 0.5f

#define HAUT_LARGEUR 400
#define BAS_LARGEUR  320
#define MARGE        10

typedef struct { float x, y, l, h; } Zone;

bool dans_zone(const Zone *z, const touchPosition *t);

/* À appeler après C2D_Init / avant C2D_Fini. */
void dessin_init(void);
void dessin_fin(void);

/* Hauteur d'une ligne de la police système à l'échelle 1. */
float dessin_hauteur_ligne(void);

/* Largeur d'un texte (signature FonctionMesure de mise_en_page.h ; contexte inutilisé). */
float dessin_mesure(void *contexte, const char *texte, float echelle);

void dessin_texte(C2D_TextBuf tampon, const char *texte, float x, float y, float echelle,
                  u32 couleur, u32 drapeaux);

/* Bouton plein avec libellé centré ; « principal » : couleur d'accent. */
void dessin_bouton(C2D_TextBuf tampon, const Zone *z, const char *libelle, bool actif,
                   bool principal);

#endif
