/* Affichage des minuteurs (voir vue_minuteurs.h). */
#include "vue_minuteurs.h"

#include <stdio.h>
#include <string.h>

#include "mise_en_page.h"

bool vm_clignote(uint64_t maintenant)
{
    return (maintenant / 500) % 2 == 0;
}

void vm_bandeau(C2D_TextBuf tampon, const Minuterie *t, uint64_t maintenant)
{
    int ordre[MINUTERIE_MAX];
    int n = minuterie_ordre(t, ordre, MINUTERIE_MAX);
    if (n == 0) return;

    C2D_DrawRectSolid(0, BANDEAU_Y, 0.5f, HAUT_LARGEUR, BANDEAU_H, C_BOUTON_OFF);
    int affiches = n > 2 ? 2 : n;
    float largeur = (HAUT_LARGEUR - 2 * MARGE) / 2.0f;
    for (int k = 0; k < affiches; k++) {
        const Minuteur *m = &t->m[ordre[k]];
        float x = MARGE + k * largeur;
        char court[TAILLE_LIBELLE], temps[16];
        bool sonne = m->sonne;
        if (sonne && vm_clignote(maintenant))
            C2D_DrawRectSolid(x - 4, BANDEAU_Y + 2, 0.5f, largeur - 4, BANDEAU_H - 4, C_ACCENT);
        u32 texte = sonne && vm_clignote(maintenant) ? C_TEXTE_FONCE : C_TEXTE;
        mep_tronquer(m->libelle, largeur - 12, 0.45f, dessin_mesure, NULL, court, sizeof(court));
        dessin_texte(tampon, court, x, BANDEAU_Y + 2, 0.45f, sonne ? texte : C_DOUX, 0);
        if (sonne) snprintf(temps, sizeof(temps), "Prêt !");
        else minuterie_formater(minuterie_restant(m, maintenant), temps, sizeof(temps));
        dessin_texte(tampon, temps, x, BANDEAU_Y + 14, 0.75f, sonne ? texte : C_ACCENT, 0);
    }
    if (n > 2) {
        char plus[16];
        snprintf(plus, sizeof(plus), "+%d", n - 2);
        dessin_texte(tampon, plus, HAUT_LARGEUR - 4, BANDEAU_Y + 2, 0.45f, C_DOUX, C2D_AlignRight);
    }
}

void vm_alerte(C2D_TextBuf tampon, const Minuterie *t, uint64_t maintenant)
{
    if (!minuterie_sonne(t)) return;
    bool vif = vm_clignote(maintenant);
    C2D_DrawRectSolid(0, 0, 0.5f, BAS_LARGEUR, 240, vif ? C_ACCENT : C_BOUTON);
    u32 texte = vif ? C_TEXTE_FONCE : C_TEXTE;
    dessin_texte(tampon, "C'est prêt !", BAS_LARGEUR / 2, 40, 1.0f, texte, C2D_AlignCenter);

    float y = 100;
    for (int i = 0; i < MINUTERIE_MAX && y < 170; i++) {
        const Minuteur *m = &t->m[i];
        if (!m->actif || !m->sonne) continue;
        char court[TAILLE_LIBELLE];
        mep_tronquer(m->libelle, BAS_LARGEUR - 2 * MARGE, 0.65f, dessin_mesure, NULL, court,
                     sizeof(court));
        dessin_texte(tampon, court, BAS_LARGEUR / 2, y, 0.65f, texte, C2D_AlignCenter);
        y += 24;
    }
    dessin_texte(tampon, "Touche l'écran ou appuie sur un bouton", BAS_LARGEUR / 2, 200, 0.5f,
                 texte, C2D_AlignCenter);
}

void vm_bouton(C2D_TextBuf tampon, const Zone *z, const Minuterie *t, const char *rid, int def,
               const char *libelle, uint64_t maintenant)
{
    int i = minuterie_trouver(t, rid, def);
    const Minuteur *m = i >= 0 ? &t->m[i] : NULL;
    char ligne1[TAILLE_LIBELLE + 16], ligne2[32];
    u32 fond, texte;

    if (!m) {
        fond = C_BOUTON;
        texte = C_TEXTE;
        snprintf(ligne1, sizeof(ligne1), "%s", libelle);
        snprintf(ligne2, sizeof(ligne2), "Toucher pour lancer");
    } else if (m->sonne) {
        bool vif = vm_clignote(maintenant);
        fond = vif ? C_ACCENT : C_BOUTON;
        texte = vif ? C_TEXTE_FONCE : C_TEXTE;
        snprintf(ligne1, sizeof(ligne1), "%s", libelle);
        snprintf(ligne2, sizeof(ligne2), "Prêt ! Toucher pour arrêter");
    } else if (maintenant < m->confirmer_jusqu_a) {
        fond = C_ALLERGENE;
        texte = C_TEXTE_FONCE;
        snprintf(ligne1, sizeof(ligne1), "Arrêter ce minuteur ?");
        snprintf(ligne2, sizeof(ligne2), "Toucher encore pour confirmer");
    } else {
        fond = C_BOUTON_OFF;
        texte = C_ACCENT;
        snprintf(ligne1, sizeof(ligne1), "%s", libelle);
        minuterie_formater(minuterie_restant(m, maintenant), ligne2, sizeof(ligne2));
    }
    C2D_DrawRectSolid(z->x, z->y, 0.5f, z->l, z->h, fond);
    if (m && !m->sonne && maintenant >= m->confirmer_jusqu_a)
        C2D_DrawRectSolid(z->x, z->y, 0.5f, 4, z->h, C_ACCENT);   /* repère « en cours » */

    char court[TAILLE_LIBELLE + 16];
    mep_tronquer(ligne1, z->l - 12, 0.45f, dessin_mesure, NULL, court, sizeof(court));
    dessin_texte(tampon, court, z->x + 8, z->y + 3, 0.45f, m ? texte : C_DOUX, 0);
    float echelle = (m && !m->sonne && maintenant >= m->confirmer_jusqu_a) ? 0.7f : 0.5f;
    dessin_texte(tampon, ligne2, z->x + 8, z->y + 18, echelle, texte, 0);
}
