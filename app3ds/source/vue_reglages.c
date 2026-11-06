/* Vue « Réglages » (voir vue_reglages.h). */
#include "vue_reglages.h"

#include <stdio.h>

#define JAUGE_H            22
#define SOUFFLE_PLEINE     0.6f   /* niveau du micro qui remplit la jauge */
#define MAIN_PLEINE        1.0f   /* part de l'image changée qui remplit la jauge */

static const Zone Z_ADRESSE  = {4, 4, 312, 40};
static const Zone Z_MICRO    = {4, 48, 154, 40};
static const Zone Z_CAMERA   = {162, 48, 154, 40};
static const Zone Z_MOINS_S  = {4, 92, 56, 40};
static const Zone Z_PLUS_S   = {260, 92, 56, 40};
static const Zone Z_MOINS_M  = {4, 136, 56, 40};
static const Zone Z_PLUS_M   = {260, 136, 56, 40};
static const Zone Z_RETOUR   = {4, 184, 312, 52};

void vg_init(VueReglages *v, Config *config)
{
    v->config = config;
    v->souffle_jusqu_a = v->main_jusqu_a = 0;
}

static bool changer(int *sensibilite, int sens)
{
    int s = *sensibilite + sens;
    if (s < SOUFFLE_SENSIBILITE_MIN || s > SOUFFLE_SENSIBILITE_MAX) return false;
    *sensibilite = s;
    return true;
}

ActionReglages vg_entrees(VueReglages *v, u32 appuis, const touchPosition *toucher)
{
    Config *c = v->config;
    if (appuis & (KEY_B | KEY_SELECT)) return VRG_RETOUR;
    if (appuis & KEY_X) return VRG_ADRESSE;
    if (appuis & KEY_A) { c->micro = !c->micro; return VRG_MODIFIE; }
    if (appuis & KEY_Y) { c->camera = !c->camera; return VRG_MODIFIE; }
    if ((appuis & KEY_DLEFT) && changer(&c->sensibilite, -1)) return VRG_MODIFIE;
    if ((appuis & KEY_DRIGHT) && changer(&c->sensibilite, +1)) return VRG_MODIFIE;
    if ((appuis & KEY_L) && changer(&c->sensibilite_camera, -1)) return VRG_MODIFIE;
    if ((appuis & KEY_R) && changer(&c->sensibilite_camera, +1)) return VRG_MODIFIE;

    if (appuis & KEY_TOUCH) {
        if (dans_zone(&Z_RETOUR, toucher)) return VRG_RETOUR;
        if (dans_zone(&Z_ADRESSE, toucher)) return VRG_ADRESSE;
        if (dans_zone(&Z_MICRO, toucher)) { c->micro = !c->micro; return VRG_MODIFIE; }
        if (dans_zone(&Z_CAMERA, toucher)) { c->camera = !c->camera; return VRG_MODIFIE; }
        if (dans_zone(&Z_MOINS_S, toucher) && changer(&c->sensibilite, -1)) return VRG_MODIFIE;
        if (dans_zone(&Z_PLUS_S, toucher) && changer(&c->sensibilite, +1)) return VRG_MODIFIE;
        if (dans_zone(&Z_MOINS_M, toucher) && changer(&c->sensibilite_camera, -1)) return VRG_MODIFIE;
        if (dans_zone(&Z_PLUS_M, toucher) && changer(&c->sensibilite_camera, +1)) return VRG_MODIFIE;
    }
    return VRG_RIEN;
}

void vg_signaler_souffle(VueReglages *v, uint64_t maintenant)
{
    v->souffle_jusqu_a = maintenant + 1000;
}

void vg_signaler_main(VueReglages *v, uint64_t maintenant)
{
    v->main_jusqu_a = maintenant + 1000;
}

/* Une jauge : titre, barre du niveau, trait du seuil, ou message si indisponible. */
static void jauge(C2D_TextBuf tampon, float y, const char *titre, const char *coupe,
                  bool disponible, bool active, float niveau, float seuil, float pleine,
                  bool detecte, const char *detecte_texte)
{
    dessin_texte(tampon, titre, MARGE, y, 0.5f, C_TEXTE, 0);
    float yb = y + 20;
    if (!disponible || !active) {
        dessin_texte(tampon, disponible ? coupe : "Indisponible sur cette console.", MARGE, yb + 2,
                     0.5f, disponible ? C_DOUX : C_ACCENT, 0);
        return;
    }
    float largeur = HAUT_LARGEUR - 2 * MARGE;
    float p = niveau / pleine, s = seuil / pleine;
    p = p > 1 ? 1 : p < 0 ? 0 : p;
    s = s > 1 ? 1 : s;
    bool fort = niveau >= seuil;
    C2D_DrawRectSolid(MARGE, yb, 0.5f, largeur, JAUGE_H, C_BOUTON_OFF);
    C2D_DrawRectSolid(MARGE, yb, 0.5f, largeur * p, JAUGE_H, fort ? C_ACCENT : C_DOUX);
    C2D_DrawRectSolid(MARGE + largeur * s - 1, yb - 4, 0.5f, 3, JAUGE_H + 8, C_TEXTE);
    if (detecte)
        dessin_texte(tampon, detecte_texte, HAUT_LARGEUR - MARGE, y, 0.55f, C_ACCENT, C2D_AlignRight);
}

void vg_dessiner(VueReglages *v, C2D_TextBuf tampon, C3D_RenderTarget *haut, C3D_RenderTarget *bas,
                 const Souffle *souffle, bool micro_pret,
                 const Mouvement *mouvement, bool camera_prete, uint64_t maintenant)
{
    const Config *c = v->config;
    char tmp[TAILLE_ADRESSE + 32];

    /* --- Écran du haut : jauges --- */
    C2D_TargetClear(haut, C_FOND);
    C2D_SceneBegin(haut);
    dessin_texte(tampon, "Réglages : commandes sans les mains", MARGE, 5, 0.5f, C_DOUX, 0);
    C2D_DrawRectSolid(MARGE, 26, 0.5f, HAUT_LARGEUR - 2 * MARGE, 1, C_BOUTON);

    jauge(tampon, 34, "Souffle (micro, sous l'écran du bas)", "Micro coupé : touche « Micro ».",
          micro_pret, c->micro, souffle->niveau, souffle->seuil, SOUFFLE_PLEINE,
          maintenant < v->souffle_jusqu_a, "Souffle détecté !");
    /* La caméra n'est allumée que si elle est activée : sinon, pas d'erreur à montrer. */
    jauge(tampon, 96, "Main (caméra, au-dessus de cet écran)", "Caméra coupée : touche « Caméra ».",
          camera_prete || !c->camera, c->camera, mouvement->mouvement, mouvement->seuil,
          MAIN_PLEINE, maintenant < v->main_jusqu_a, "Main détectée !");

    dessin_texte(tampon, "La barre doit dépasser le trait blanc. Trop facile à déclencher ?",
                 MARGE, 160, 0.42f, C_DOUX, 0);
    dessin_texte(tampon, "Baisse la sensibilité. Pas assez ? Monte-la.", MARGE, 174, 0.42f, C_DOUX, 0);
    snprintf(tmp, sizeof(tmp), "Passerelle : %s", c->passerelle[0] ? c->passerelle : "non réglée");
    dessin_texte(tampon, tmp, MARGE, 200, 0.42f, C_DOUX, 0);
    dessin_texte(tampon, "A micro · Y caméra · croix / L R : sensibilités · B retour", MARGE, 220,
                 0.42f, C_DOUX, 0);

    /* --- Écran du bas : boutons --- */
    C2D_TargetClear(bas, C_FOND);
    C2D_SceneBegin(bas);
    dessin_bouton(tampon, &Z_ADRESSE, "Adresse de la passerelle", true, false);
    dessin_bouton(tampon, &Z_MICRO, c->micro ? "Micro : oui" : "Micro : non", micro_pret,
                  c->micro && micro_pret);
    dessin_bouton(tampon, &Z_CAMERA, c->camera ? "Caméra : oui" : "Caméra : non", true, c->camera);

    bool s_actif = c->micro && micro_pret;
    dessin_bouton(tampon, &Z_MOINS_S, "-", s_actif && c->sensibilite > SOUFFLE_SENSIBILITE_MIN, false);
    dessin_bouton(tampon, &Z_PLUS_S, "+", s_actif && c->sensibilite < SOUFFLE_SENSIBILITE_MAX, false);
    snprintf(tmp, sizeof(tmp), "Souffle %d / %d", c->sensibilite, SOUFFLE_SENSIBILITE_MAX);
    dessin_texte(tampon, tmp, BAS_LARGEUR / 2, 102, 0.6f, s_actif ? C_TEXTE : C_BOUTON, C2D_AlignCenter);

    bool m_actif = c->camera;
    dessin_bouton(tampon, &Z_MOINS_M, "-", m_actif && c->sensibilite_camera > 1, false);
    dessin_bouton(tampon, &Z_PLUS_M, "+", m_actif && c->sensibilite_camera < 5, false);
    snprintf(tmp, sizeof(tmp), "Main %d / 5", c->sensibilite_camera);
    dessin_texte(tampon, tmp, BAS_LARGEUR / 2, 146, 0.6f, m_actif ? C_TEXTE : C_BOUTON, C2D_AlignCenter);

    dessin_bouton(tampon, &Z_RETOUR, "Retour", true, true);
}
