/* Outils de dessin communs (voir dessin.h). */
#include "dessin.h"

static C2D_TextBuf tampon_mesure;   /* sert uniquement aux mesures de largeur */
static float hauteur_ligne = 30.0f;

bool dans_zone(const Zone *z, const touchPosition *t)
{
    return t->px >= z->x && t->px < z->x + z->l && t->py >= z->y && t->py < z->y + z->h;
}

void dessin_init(void)
{
    tampon_mesure = C2D_TextBufNew(1536);
    FINF_s *info = C2D_FontGetInfo(NULL);   /* police système */
    if (info && info->lineFeed) hauteur_ligne = info->lineFeed;
}

void dessin_fin(void)
{
    C2D_TextBufDelete(tampon_mesure);
}

float dessin_hauteur_ligne(void)
{
    return hauteur_ligne;
}

float dessin_mesure(void *contexte, const char *texte, float echelle)
{
    (void)contexte;
    C2D_TextBufClear(tampon_mesure);
    C2D_Text t;
    C2D_TextParse(&t, tampon_mesure, texte);
    float largeur = 0, hauteur = 0;
    C2D_TextGetDimensions(&t, echelle, echelle, &largeur, &hauteur);
    return largeur;
}

void dessin_texte(C2D_TextBuf tampon, const char *texte, float x, float y, float echelle,
                  u32 couleur, u32 drapeaux)
{
    C2D_Text t;
    C2D_TextParse(&t, tampon, texte);
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor | drapeaux, x, y, 0.5f, echelle, echelle, couleur);
}

void dessin_bouton(C2D_TextBuf tampon, const Zone *z, const char *libelle, bool actif,
                   bool principal)
{
    u32 fond = !actif ? C_BOUTON_OFF : principal ? C_ACCENT : C_BOUTON;
    u32 texte = !actif ? C_BOUTON : principal ? C_TEXTE_FONCE : C_TEXTE;
    C2D_DrawRectSolid(z->x, z->y, 0.5f, z->l, z->h, fond);
    float echelle = z->h >= 48 ? 0.7f : 0.6f;
    dessin_texte(tampon, libelle, z->x + z->l / 2, z->y + (z->h - hauteur_ligne * echelle) / 2,
                 echelle, texte, C2D_AlignCenter);
}
