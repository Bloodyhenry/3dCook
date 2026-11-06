/*
 * Vue « recette » (voir vue_recette.h).
 */
#include "vue_recette.h"

#include <stdio.h>
#include <string.h>

#include "echelle.h"
#include "vue_minuteurs.h"

/* --- Géométrie --------------------------------------------------------------- */
#define HAUT_Y_TEXTE    34      /* sous l'en-tête */
#define HAUT_Y_FIN      230     /* au-dessus de la barre de progression */
#define HAUT_SECTION_H  22

#define BAS_BARRE_H     40      /* bandeau retour + portions */
#define BAS_LISTE_Y     46
#define BAS_LISTE_FIN   178     /* sans minuteur à l'étape */
#define BAS_MINUT_Y     136     /* boutons des minuteurs de l'étape */
#define BAS_MINUT_H     44
#define MAX_MINUT_ETAPE 2
#define BAS_BOUTON_Y    184
#define BAS_BOUTON_H    52
#define ECHELLE_INGR    0.5f

/* Plus grande taille d'abord ; la plus longue étape de la base (941 octets)
 * tient à 0,45. */
static const float ECHELLES_ETAPE[] = {1.0f, 0.9f, 0.8f, 0.7f, 0.62f, 0.55f, 0.5f, 0.45f};
#define NB_ECHELLES_ETAPE ((int)(sizeof(ECHELLES_ETAPE) / sizeof(ECHELLES_ETAPE[0])))

static const Zone Z_RETOUR    = {0, 0, 64, BAS_BARRE_H};
static const Zone Z_MOINS     = {68, 0, 48, BAS_BARRE_H};
static const Zone Z_PLUS      = {BAS_LARGEUR - 48, 0, 48, BAS_BARRE_H};
static const Zone Z_PRECEDENT = {4, BAS_BOUTON_Y, 152, BAS_BOUTON_H};
static const Zone Z_SUIVANT   = {164, BAS_BOUTON_Y, 152, BAS_BOUTON_H};

/* Minuteurs définis pour l'étape en cours (indices dans Recette.minuteurs). */
static int minuteurs_etape(const VueRecette *v, int *defs)
{
    int n = 0;
    for (int k = 0; k < v->recette->nb_minuteurs && n < MAX_MINUT_ETAPE; k++)
        if (v->recette->minuteurs[k].etape == v->etape) defs[n++] = k;
    return n;
}

static Zone zone_minuteur(int rang, int nombre)
{
    float largeur = (BAS_LARGEUR - 8 - 4 * (nombre - 1)) / (float)nombre;
    Zone z = {4 + rang * (largeur + 4), BAS_MINUT_Y, largeur, BAS_MINUT_H};
    return z;
}

static float fin_liste(const VueRecette *v)
{
    int defs[MAX_MINUT_ETAPE];
    return minuteurs_etape(v, defs) ? BAS_MINUT_Y - 4 : BAS_LISTE_FIN;
}

/* Titre raccourci pour l'en-tête (calculé une fois par recette). */
static char titre_court[TAILLE_TITRE];

/* --- Portions ---------------------------------------------------------------------- */

static double facteur(const VueRecette *v)
{
    const Recette *r = v->recette;
    double valeur = r->a_portions ? v->portions : ECHELLE_MULTIPLICATEURS[v->indice_mult];
    return echelle_facteur(r->a_portions, r->portions_nombre, valeur);
}

static void changer_portions(VueRecette *v, int sens)
{
    if (v->recette->a_portions) {
        int n = v->portions + sens;
        if (n < 1 || n > 200) return;
        v->portions = n;
    } else {
        int i = v->indice_mult + sens;
        if (i < 0 || i >= ECHELLE_NB_MULTIPLICATEURS) return;
        v->indice_mult = i;
    }
    v->ingredients_a_refaire = true;
}

/* --- Mises en page ---------------------------------------------------------------- */

static void ajouter_ligne(VueRecette *v, const char *texte, GenreLigne genre)
{
    if (v->nb_lignes_ingr >= VR_MAX_LIGNES_INGR) return;
    copier_texte(v->lignes_ingr[v->nb_lignes_ingr], texte, MEP_TAILLE_LIGNE);
    v->genres_ingr[v->nb_lignes_ingr++] = genre;
}

static void refaire_ingredients(VueRecette *v)
{
    const Recette *r = v->recette;
    double f = facteur(v);
    float largeur = BAS_LARGEUR - 2 * MARGE;
    v->nb_lignes_ingr = 0;
    int groupe = -1;
    static Paragraphe p;   /* gros : pas sur la pile */

    for (int i = 0; i < r->nb_ingredients; i++) {
        const Ingredient *ing = &r->ingredients[i];
        if (ing->groupe != groupe) {
            groupe = ing->groupe;
            if (groupe >= 0 && groupe < r->nb_groupes && r->titres_groupes[groupe][0])
                ajouter_ligne(v, r->titres_groupes[groupe], LIGNE_TITRE);
        }
        char texte[MEP_TAILLE_LIGNE];
        char ligne[MEP_TAILLE_LIGNE + 8];
        echelle_formater_ingredient(ing, f, texte, sizeof(texte));
        snprintf(ligne, sizeof(ligne), "- %s", texte);
        if (mep_couper(ligne, largeur, ECHELLE_INGR, dessin_mesure, NULL, &p) < 0) p.nb_lignes = 1;
        for (int l = 0; l < p.nb_lignes; l++) {
            char retrait[MEP_TAILLE_LIGNE + 4];
            /* Lignes suivantes décalées sous le texte, pas sous le tiret. */
            snprintf(retrait, sizeof(retrait), "%s%s", l ? "   " : "", p.lignes[l]);
            ajouter_ligne(v, retrait, ing->allergene ? LIGNE_ALLERGENE : LIGNE_NORMALE);
        }
    }
    v->ingredients_a_refaire = false;
}

static int lignes_visibles(const VueRecette *v)
{
    return (int)((fin_liste(v) - BAS_LISTE_Y) / (dessin_hauteur_ligne() * ECHELLE_INGR));
}

static void refaire_etape(VueRecette *v)
{
    const Etape *e = &v->recette->etapes[v->etape];
    float y_debut = HAUT_Y_TEXTE + (e->section[0] ? HAUT_SECTION_H : 0);
    float y_fin = v->avec_bandeau ? BANDEAU_Y - 4 : HAUT_Y_FIN;
    mep_ajuster(e->texte, HAUT_LARGEUR - 2 * MARGE, y_fin - y_debut,
                dessin_hauteur_ligne(), ECHELLES_ETAPE, NB_ECHELLES_ETAPE,
                dessin_mesure, NULL, &v->texte_etape);
    v->etape_a_refaire = false;
}

/* --- Cycle de vie et entrées ------------------------------------------------------------ */

void vr_init(VueRecette *v, const Recette *recette, double echelle)
{
    memset(v, 0, sizeof(*v));
    v->recette = recette;
    if (echelle <= 0) echelle = 1;
    /* Portions de départ : celles du planning (ex. ×1,5 sur 6 parts → 9 parts). */
    if (recette->a_portions) {
        int n = (int)(recette->portions_nombre * echelle + 0.5);
        v->portions = n < 1 ? 1 : n > 200 ? 200 : n;
    }
    /* Sinon : le multiplicateur proposé le plus proche. */
    v->indice_mult = ECHELLE_INDICE_X1;
    for (int i = 0; i < ECHELLE_NB_MULTIPLICATEURS; i++) {
        double ecart = ECHELLE_MULTIPLICATEURS[i] - echelle;
        double meilleur = ECHELLE_MULTIPLICATEURS[v->indice_mult] - echelle;
        if (ecart * ecart < meilleur * meilleur) v->indice_mult = i;
    }
    v->etape_a_refaire = true;
    v->ingredients_a_refaire = true;
    /* Place laissée à droite pour l'indicateur du micro et « Étape 12 / 12 ». */
    mep_tronquer(recette->titre, HAUT_LARGEUR - 2 * MARGE - 160, 0.5f, dessin_mesure, NULL,
                 titre_court, sizeof(titre_court));
}

static void aller_a(VueRecette *v, int etape)
{
    if (etape < 0 || etape >= v->recette->nb_etapes || etape == v->etape) return;
    v->etape = etape;
    v->etape_a_refaire = true;
}

bool vr_suivante(VueRecette *v)
{
    if (v->etape >= v->recette->nb_etapes - 1) return false;
    aller_a(v, v->etape + 1);
    return true;
}

static void defiler(VueRecette *v, int sens)
{
    int max = v->nb_lignes_ingr - lignes_visibles(v);
    int d = v->defilement + sens;
    v->defilement = d < 0 ? 0 : d > max ? (max > 0 ? max : 0) : d;
}

ActionRecette vr_entrees(VueRecette *v, u32 appuis, const touchPosition *toucher,
                         Minuterie *minuterie, uint64_t maintenant)
{
    if (appuis & KEY_SELECT) return VR_RETOUR;
    if (appuis & (KEY_A | KEY_R | KEY_DRIGHT)) aller_a(v, v->etape + 1);
    if (appuis & (KEY_B | KEY_L | KEY_DLEFT))  aller_a(v, v->etape - 1);
    if (appuis & KEY_UP)   defiler(v, -1);
    if (appuis & KEY_DOWN) defiler(v, +1);
    if (appuis & KEY_X)    changer_portions(v, +1);
    if (appuis & KEY_Y)    changer_portions(v, -1);

    if (appuis & KEY_TOUCH) {
        if (dans_zone(&Z_RETOUR, toucher))         return VR_RETOUR;
        if (dans_zone(&Z_SUIVANT, toucher))        aller_a(v, v->etape + 1);
        else if (dans_zone(&Z_PRECEDENT, toucher)) aller_a(v, v->etape - 1);
        else if (dans_zone(&Z_PLUS, toucher))      changer_portions(v, +1);
        else if (dans_zone(&Z_MOINS, toucher))     changer_portions(v, -1);
        else if (toucher->py >= BAS_MINUT_Y && toucher->py < BAS_MINUT_Y + BAS_MINUT_H) {
            int defs[MAX_MINUT_ETAPE];
            int n = minuteurs_etape(v, defs);
            for (int k = 0; k < n; k++) {
                Zone z = zone_minuteur(k, n);
                if (!dans_zone(&z, toucher)) continue;
                const DefMinuteur *d = &v->recette->minuteurs[defs[k]];
                minuterie_toucher(minuterie, v->recette->id, defs[k], d->libelle, d->secondes,
                                  d->secondes_max, maintenant);
            }
            if (n == 0) defiler(v, +3);   /* pas de minuteur : c'est encore la liste */
        } else if (toucher->py >= BAS_LISTE_Y && toucher->py < fin_liste(v))
            /* Toucher la moitié haute ou basse de la liste la fait défiler. */
            defiler(v, toucher->py < (BAS_LISTE_Y + fin_liste(v)) / 2 ? -3 : +3);
    }
    return VR_RIEN;
}

/* --- Dessin ----------------------------------------------------------------------------- */

static void mini_jauge(float x, float y, float l, float h, float niveau, float seuil, bool allume)
{
    niveau = niveau > 1 ? 1 : niveau < 0 ? 0 : niveau;
    seuil = seuil > 1 ? 1 : seuil;
    C2D_DrawRectSolid(x, y, 0.5f, l, h, allume ? C_ACCENT : C_BOUTON_OFF);
    C2D_DrawRectSolid(x, y, 0.5f, l * niveau, h, allume ? C_ACCENT : C_DOUX);
    C2D_DrawRectSolid(x + l * seuil, y - 1, 0.5f, 2, h + 2, C_TEXTE);
}

static void dessiner_haut(VueRecette *v, C2D_TextBuf tampon, const Minuterie *minuterie,
                          uint64_t maintenant)
{
    const Recette *r = v->recette;
    const Etape *e = &r->etapes[v->etape];
    char tmp[64];

    /* En-tête : titre et numéro d'étape. */
    dessin_texte(tampon, titre_court, MARGE, 5, 0.5f, C_DOUX, 0);
    snprintf(tmp, sizeof(tmp), "Étape %d / %d", v->etape + 1, r->nb_etapes);
    dessin_texte(tampon, tmp, HAUT_LARGEUR - MARGE, 5, 0.5f, C_DOUX, C2D_AlignRight);
    C2D_DrawRectSolid(MARGE, 26, 0.5f, HAUT_LARGEUR - 2 * MARGE, 1, C_BOUTON);

    /* Indicateurs du micro (en haut) et de la caméra (en dessous) : petites
     * jauges, orange quand un souffle ou une main viennent d'être reconnus. */
    bool geste = maintenant < v->geste_jusqu_a;
    int nb = (v->micro_actif ? 1 : 0) + (v->camera_active ? 1 : 0);
    float h = nb == 2 ? 6 : 9, y0 = nb == 2 ? 5 : 9;
    if (v->micro_actif) {
        mini_jauge(236, y0, 50, h, v->micro_niveau / 0.6f, v->micro_seuil / 0.6f, geste);
        y0 += h + 3;
    }
    if (v->camera_active) mini_jauge(236, y0, 50, h, v->camera_niveau, v->camera_seuil, geste);

    float y = HAUT_Y_TEXTE;
    if (e->section[0]) {
        dessin_texte(tampon, e->section, MARGE, y, 0.6f, C_ACCENT, 0);
        y += HAUT_SECTION_H;
    }

    /* Texte de l'étape, ligne par ligne. */
    const Paragraphe *p = &v->texte_etape;
    float pas = dessin_hauteur_ligne() * p->echelle;
    for (int i = 0; i < p->nb_lignes; i++)
        dessin_texte(tampon, p->lignes[i], MARGE, y + i * pas, p->echelle, C_TEXTE, 0);

    /* Minuteurs en cours (n'importe quelle étape ou recette). */
    vm_bandeau(tampon, minuterie, maintenant);

    /* Barre de progression. */
    float avance = (float)(v->etape + 1) / (float)r->nb_etapes;
    C2D_DrawRectSolid(0, 234, 0.5f, HAUT_LARGEUR, 6, C_BOUTON);
    C2D_DrawRectSolid(0, 234, 0.5f, HAUT_LARGEUR * avance, 6, C_ACCENT);
}

static void dessiner_bas(VueRecette *v, C2D_TextBuf tampon, const Minuterie *minuterie,
                         uint64_t maintenant)
{
    const Recette *r = v->recette;
    char tmp[64];

    /* Bandeau : retour à la liste, portions. */
    dessin_bouton(tampon, &Z_RETOUR, "Liste", true, false);
    if (r->a_portions) {
        snprintf(tmp, sizeof(tmp), "%d %s", v->portions, r->portions_unite);
    } else {
        echelle_formater_multiplicateur(ECHELLE_MULTIPLICATEURS[v->indice_mult], tmp, sizeof(tmp));
    }
    bool peut_moins = r->a_portions ? v->portions > 1 : v->indice_mult > 0;
    bool peut_plus = r->a_portions ? v->portions < 200
                                   : v->indice_mult < ECHELLE_NB_MULTIPLICATEURS - 1;
    dessin_bouton(tampon, &Z_MOINS, "-", peut_moins, false);
    dessin_bouton(tampon, &Z_PLUS, "+", peut_plus, false);
    float centre = (Z_MOINS.x + Z_MOINS.l + Z_PLUS.x) / 2;
    dessin_texte(tampon, tmp, centre, 10, 0.6f, C_TEXTE, C2D_AlignCenter);

    /* Ingrédients. */
    float pas = dessin_hauteur_ligne() * ECHELLE_INGR;
    int visibles = lignes_visibles(v);
    float fin = fin_liste(v);
    for (int i = 0; i < visibles && v->defilement + i < v->nb_lignes_ingr; i++) {
        int l = v->defilement + i;
        u32 couleur = v->genres_ingr[l] == LIGNE_TITRE ? C_ACCENT
                    : v->genres_ingr[l] == LIGNE_ALLERGENE ? C_ALLERGENE : C_TEXTE;
        dessin_texte(tampon, v->lignes_ingr[l], MARGE, BAS_LISTE_Y + i * pas, ECHELLE_INGR,
                     couleur, 0);
    }
    /* Flèches de défilement (triangles) quand la liste dépasse. */
    if (v->defilement > 0)
        C2D_DrawTriangle(306, BAS_LISTE_Y + 8, C_DOUX, 314, BAS_LISTE_Y + 8, C_DOUX,
                         310, BAS_LISTE_Y + 1, C_DOUX, 0.5f);
    if (v->defilement + visibles < v->nb_lignes_ingr)
        C2D_DrawTriangle(306, fin - 8, C_DOUX, 314, fin - 8, C_DOUX, 310, fin - 1, C_DOUX, 0.5f);

    /* Minuteurs de l'étape en cours. */
    int defs[MAX_MINUT_ETAPE];
    int n = minuteurs_etape(v, defs);
    for (int k = 0; k < n; k++) {
        Zone z = zone_minuteur(k, n);
        vm_bouton(tampon, &z, minuterie, r->id, defs[k], r->minuteurs[defs[k]].libelle, maintenant);
    }

    /* Navigation. */
    dessin_bouton(tampon, &Z_PRECEDENT, "Précédent", v->etape > 0, false);
    dessin_bouton(tampon, &Z_SUIVANT, "Suivant", v->etape < r->nb_etapes - 1, true);
}

void vr_dessiner(VueRecette *v, C2D_TextBuf tampon, C3D_RenderTarget *haut, C3D_RenderTarget *bas,
                 const Minuterie *minuterie, uint64_t maintenant)
{
    /* Le bandeau des minuteurs réduit la place du texte : remise en page. */
    bool bandeau = minuterie_active(minuterie);
    if (bandeau != v->avec_bandeau) {
        v->avec_bandeau = bandeau;
        v->etape_a_refaire = true;
    }
    if (v->etape_a_refaire) {
        refaire_etape(v);
        defiler(v, 0);   /* la liste a pu raccourcir (minuteurs de la nouvelle étape) */
    }
    if (v->ingredients_a_refaire) {
        refaire_ingredients(v);
        defiler(v, 0);   /* garde le défilement dans les bornes */
    }

    C2D_TargetClear(haut, C_FOND);
    C2D_SceneBegin(haut);
    dessiner_haut(v, tampon, minuterie, maintenant);

    C2D_TargetClear(bas, C_FOND);
    C2D_SceneBegin(bas);
    dessiner_bas(v, tampon, minuterie, maintenant);
}
