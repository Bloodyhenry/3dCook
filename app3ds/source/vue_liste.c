/*
 * Vue « liste » (voir vue_liste.h).
 */
#include "vue_liste.h"

#include <stdio.h>
#include <string.h>

#include "menu.h"
#include "photo_ecran.h"
#include "vue_minuteurs.h"

#define LIGNE_H      46          /* hauteur d'une ligne de la liste (doigt) */
#define NB_LIGNES    4
#define BARRE_Y      190
#define BARRE_H      46

#define TITRE_Y      34
#define TITRE_H      106
#define INFO_Y       144
#define MESSAGE_Y    168
#define ADRESSE_Y    204
#define AIDE_Y       222

static const Zone Z_HAUT    = {4, BARRE_Y, 72, BARRE_H};
static const Zone Z_BAS     = {80, BARRE_Y, 72, BARRE_H};
static const Zone Z_SYNCHRO = {156, BARRE_Y, 84, BARRE_H};
static const Zone Z_REGLAGES = {244, BARRE_Y, 72, BARRE_H};
static const Zone Z_EXEMPLE = {20, 96, 280, 56};

static const float ECHELLES_TITRE[] = {1.0f, 0.85f, 0.7f, 0.6f, 0.5f};
static const float ECHELLES_TITRE_PHOTO[] = {0.8f, 0.7f, 0.6f, 0.5f};
#define VOILE        C2D_Color32(0x1C, 0x1A, 0x17, 0xC8)   /* fond sombre translucide */
#define PHOTO_DELAI_MS 150      /* sélection immobile depuis ce délai : on charge la photo */

/* Textes mis en page une fois (pas à chaque image). */
static Paragraphe aide, message;
static char ligne_adresse[TAILLE_ADRESSE + 32];

void vl_init(VueListe *v, const Catalogue *catalogue, const char *adresse)
{
    memset(v, 0, sizeof(*v));
    v->catalogue = catalogue;
    copier_texte(v->adresse, adresse, sizeof(v->adresse));
    v->a_preparer = true;
    v->titre_mis_en_page = -1;
    v->selection_vue = -1;
    v->photo_pour = -1;
    v->titre_photo_pour = -1;
}

void vl_message(VueListe *v, const char *texte, bool erreur)
{
    copier_texte(v->message, texte, sizeof(v->message));
    v->message_erreur = erreur;
    v->a_preparer = true;
}

static void preparer(VueListe *v)
{
    const Catalogue *c = v->catalogue;
    float largeur = HAUT_LARGEUR - 2 * MARGE;
    for (int i = 0; i < c->nb; i++)
        mep_tronquer(c->entrees[i].titre, BAS_LARGEUR - 2 * MARGE, 0.6f, dessin_mesure, NULL,
                     v->titres_courts[i], TAILLE_TITRE);

    mep_couper(v->adresse[0]
                   ? "Ajoute des recettes au menu (page web de la passerelle, ou planning "
                     "de Mealie), puis touche « Synchro »."
                   : "Touche « Réglages », puis « Adresse de la passerelle » : l'adresse "
                     "est affichée en haut de la page web de la passerelle.",
               largeur, 0.55f, dessin_mesure, NULL, &aide);

    char texte[400] = "";
    if (v->message[0])
        snprintf(texte, sizeof(texte), "%s", v->message);
    if (c->nb_illisibles)
        snprintf(texte + strlen(texte), sizeof(texte) - strlen(texte), "%s%d fichier(s) illisible(s) : %s",
                 texte[0] ? " " : "", c->nb_illisibles, c->erreur);
    if (mep_couper(texte, largeur, 0.45f, dessin_mesure, NULL, &message) < 0)
        message.nb_lignes = 3;
    if (message.nb_lignes > 3) message.nb_lignes = 3;   /* place pour trois lignes */

    snprintf(ligne_adresse, sizeof(ligne_adresse), "Passerelle : %s",
             v->adresse[0] ? v->adresse : "non réglée");
    v->a_preparer = false;
}

/* --- Entrées ---------------------------------------------------------------------- */

static void choisir(VueListe *v, int indice)
{
    int nb = v->catalogue->nb;
    if (nb == 0) return;
    v->selection = indice < 0 ? 0 : indice >= nb ? nb - 1 : indice;
    /* Garde la sélection visible dans la liste du bas. */
    if (v->selection < v->premier) v->premier = v->selection;
    if (v->selection >= v->premier + NB_LIGNES) v->premier = v->selection - NB_LIGNES + 1;
}

ActionListe vl_entrees(VueListe *v, u32 appuis, const touchPosition *toucher)
{
    bool vide = v->catalogue->nb == 0;
    if (appuis & KEY_Y) return VL_SYNCHRO;
    if (appuis & KEY_X) return VL_REGLAGES;
    if (appuis & KEY_SELECT) return VL_EXEMPLE;   /* recette d'exemple, toujours accessible */
    if (appuis & KEY_A) return vide ? VL_EXEMPLE : VL_OUVRIR;
    if (appuis & KEY_UP)   choisir(v, v->selection - 1);
    if (appuis & KEY_DOWN) choisir(v, v->selection + 1);
    if (appuis & (KEY_L | KEY_DLEFT))  choisir(v, v->selection - NB_LIGNES);
    if (appuis & (KEY_R | KEY_DRIGHT)) choisir(v, v->selection + NB_LIGNES);

    if (appuis & KEY_TOUCH) {
        if (dans_zone(&Z_SYNCHRO, toucher)) return VL_SYNCHRO;
        if (dans_zone(&Z_REGLAGES, toucher)) return VL_REGLAGES;
        if (vide) return dans_zone(&Z_EXEMPLE, toucher) ? VL_EXEMPLE : VL_RIEN;
        if (dans_zone(&Z_HAUT, toucher)) choisir(v, v->selection - NB_LIGNES);
        else if (dans_zone(&Z_BAS, toucher)) choisir(v, v->selection + NB_LIGNES);
        else if (toucher->py < NB_LIGNES * LIGNE_H) {
            int indice = v->premier + toucher->py / LIGNE_H;
            if (indice < v->catalogue->nb) {
                choisir(v, indice);
                return VL_OUVRIR;
            }
        }
    }
    return VL_RIEN;
}

/* --- Dessin ----------------------------------------------------------------------------- */

/* « Samedi 7 · Dîner · 6 étapes · 2 minuteurs » */
static void texte_infos(const EntreeCatalogue *e, char *sortie, size_t taille)
{
    char jour[32];
    size_t n = 0;
    sortie[0] = '\0';
    menu_libelle_jour(e->date, jour, sizeof(jour));
    if (jour[0]) n += (size_t)snprintf(sortie + n, taille - n, "%s · ", jour);
    if (e->repas[0] && n < taille) n += (size_t)snprintf(sortie + n, taille - n, "%s · ", e->repas);
    if (n < taille)
        n += (size_t)snprintf(sortie + n, taille - n, "%d étape%s", e->nb_etapes,
                              e->nb_etapes > 1 ? "s" : "");
    if (e->nb_minuteurs && n < taille)
        snprintf(sortie + n, taille - n, " · %d minuteur%s", e->nb_minuteurs,
                 e->nb_minuteurs > 1 ? "s" : "");
}

static void entete(C2D_TextBuf tampon, const char *gauche, const char *droite)
{
    dessin_texte(tampon, gauche, MARGE, 5, 0.5f, C_DOUX, 0);
    if (droite) dessin_texte(tampon, droite, HAUT_LARGEUR - MARGE, 5, 0.5f, C_DOUX, C2D_AlignRight);
    C2D_DrawRectSolid(MARGE, 26, 0.5f, HAUT_LARGEUR - 2 * MARGE, 1, C_BOUTON);
}

/* Photo en plein écran, titre et informations sur un voile sombre en bas. */
static void dessiner_haut_photo(VueListe *v, C2D_TextBuf tampon, bool minuteurs)
{
    const Catalogue *c = v->catalogue;
    float lh = dessin_hauteur_ligne();
    char tmp[128];

    photo_ecran_dessiner();

    C2D_DrawRectSolid(0, 0, 0.5f, HAUT_LARGEUR, 26, VOILE);
    snprintf(tmp, sizeof(tmp), "%d recette%s", c->nb, c->nb > 1 ? "s" : "");
    dessin_texte(tampon, "Menu de la semaine", MARGE, 5, 0.5f, C_TEXTE, 0);
    dessin_texte(tampon, tmp, HAUT_LARGEUR - MARGE, 5, 0.5f, C_TEXTE, C2D_AlignRight);

    if (message.nb_lignes) {
        float h = message.nb_lignes * lh * 0.45f + 6;
        C2D_DrawRectSolid(0, 26, 0.5f, HAUT_LARGEUR, h, VOILE);
        u32 couleur = v->message_erreur || (!v->message[0] && c->nb_illisibles) ? C_ACCENT : C_TEXTE;
        for (int i = 0; i < message.nb_lignes; i++)
            dessin_texte(tampon, message.lignes[i], MARGE, 29 + i * lh * 0.45f, 0.45f, couleur, 0);
    }

    if (v->titre_photo_pour != v->selection) {
        mep_ajuster(c->entrees[v->selection].titre, HAUT_LARGEUR - 2 * MARGE, 52, lh,
                    ECHELLES_TITRE_PHOTO, 4, dessin_mesure, NULL, &v->titre_photo);
        v->titre_photo_pour = v->selection;
    }
    float h_titre = v->titre_photo.nb_lignes * lh * v->titre_photo.echelle;
    float hauteur = h_titre + 26;
    float y0 = (minuteurs ? BANDEAU_Y : 240) - hauteur;
    C2D_DrawRectSolid(0, y0, 0.5f, HAUT_LARGEUR, hauteur, VOILE);
    for (int i = 0; i < v->titre_photo.nb_lignes; i++)
        dessin_texte(tampon, v->titre_photo.lignes[i], MARGE,
                     y0 + 3 + i * lh * v->titre_photo.echelle, v->titre_photo.echelle, C_TEXTE, 0);
    texte_infos(&c->entrees[v->selection], tmp, sizeof(tmp));
    dessin_texte(tampon, tmp, MARGE, y0 + h_titre + 5, 0.5f, C_ACCENT, 0);
}

static void dessiner_haut(VueListe *v, C2D_TextBuf tampon)
{
    const Catalogue *c = v->catalogue;
    float lh = dessin_hauteur_ligne();
    char tmp[128];

    snprintf(tmp, sizeof(tmp), "%d recette%s", c->nb, c->nb > 1 ? "s" : "");
    entete(tampon, "Menu de la semaine", tmp);

    if (c->nb == 0) {
        dessin_texte(tampon, "Aucune recette sur la console.", MARGE, TITRE_Y, 0.75f, C_TEXTE, 0);
        for (int i = 0; i < aide.nb_lignes; i++)
            dessin_texte(tampon, aide.lignes[i], MARGE, TITRE_Y + 34 + i * lh * 0.55f, 0.55f,
                         C_DOUX, 0);
    } else {
        /* Titre de la recette sélectionnée, le plus gros possible. */
        if (v->titre_mis_en_page != v->selection) {
            mep_ajuster(c->entrees[v->selection].titre, HAUT_LARGEUR - 2 * MARGE, TITRE_H, lh,
                        ECHELLES_TITRE, 5, dessin_mesure, NULL, &v->titre);
            v->titre_mis_en_page = v->selection;
        }
        for (int i = 0; i < v->titre.nb_lignes; i++)
            dessin_texte(tampon, v->titre.lignes[i], MARGE, TITRE_Y + i * lh * v->titre.echelle,
                         v->titre.echelle, C_TEXTE, 0);
        texte_infos(&c->entrees[v->selection], tmp, sizeof(tmp));
        dessin_texte(tampon, tmp, MARGE, INFO_Y, 0.55f, C_ACCENT, 0);
    }

    u32 couleur = v->message_erreur || (!v->message[0] && c->nb_illisibles) ? C_ACCENT : C_DOUX;
    for (int i = 0; i < message.nb_lignes; i++)
        dessin_texte(tampon, message.lignes[i], MARGE, MESSAGE_Y + i * lh * 0.45f, 0.45f, couleur, 0);

    dessin_texte(tampon, ligne_adresse, MARGE, ADRESSE_Y, 0.42f, C_DOUX, 0);
    dessin_texte(tampon, c->nb ? "A ouvrir · Y synchro · X réglages · SELECT exemple · START quitter"
                               : "A ou SELECT : recette d'exemple · Y synchro · X réglages",
                 MARGE, AIDE_Y, 0.42f, C_DOUX, 0);
}

static void dessiner_bas(VueListe *v, C2D_TextBuf tampon)
{
    const Catalogue *c = v->catalogue;
    float lh = dessin_hauteur_ligne();

    if (c->nb == 0) {
        dessin_bouton(tampon, &Z_EXEMPLE, "Recette d'exemple", true, false);
    } else {
        char infos[128];
        for (int i = 0; i < NB_LIGNES && v->premier + i < c->nb; i++) {
            int k = v->premier + i;
            float y = (float)(i * LIGNE_H);
            if (k == v->selection)
                C2D_DrawRectSolid(0, y, 0.5f, BAS_LARGEUR, LIGNE_H - 2, C_BOUTON);
            dessin_texte(tampon, v->titres_courts[k], MARGE, y + 3, 0.6f, C_TEXTE, 0);
            texte_infos(&c->entrees[k], infos, sizeof(infos));
            dessin_texte(tampon, infos, MARGE, y + 4 + lh * 0.6f, 0.42f, C_DOUX, 0);
        }
        dessin_bouton(tampon, &Z_HAUT, "Haut", v->selection > 0, false);
        dessin_bouton(tampon, &Z_BAS, "Bas", v->selection < c->nb - 1, false);
    }
    dessin_bouton(tampon, &Z_SYNCHRO, "Synchro", v->adresse[0] != '\0', true);
    dessin_bouton(tampon, &Z_REGLAGES, "Réglages", true, false);
}

void vl_dessiner(VueListe *v, C2D_TextBuf tampon, C3D_RenderTarget *haut, C3D_RenderTarget *bas,
                 const Minuterie *minuterie, uint64_t maintenant)
{
    if (v->a_preparer) preparer(v);
    const Catalogue *c = v->catalogue;

    /* Photo de la sélection, une fois que la sélection ne bouge plus. */
    if (v->selection != v->selection_vue) {
        v->selection_vue = v->selection;
        v->selection_depuis = maintenant;
        v->avec_photo = false;
        v->photo_pour = -1;
    }
    if (c->nb && v->photo_pour != v->selection && maintenant - v->selection_depuis >= PHOTO_DELAI_MS) {
        v->avec_photo = photo_ecran_charger(DOSSIER_RECETTES, c->entrees[v->selection].id);
        v->photo_pour = v->selection;
    }

    C2D_TargetClear(haut, C_FOND);
    C2D_SceneBegin(haut);
    if (c->nb && v->avec_photo) dessiner_haut_photo(v, tampon, minuterie_active(minuterie));
    else dessiner_haut(v, tampon);
    vm_bandeau(tampon, minuterie, maintenant);   /* les minuteurs restent visibles */

    C2D_TargetClear(bas, C_FOND);
    C2D_SceneBegin(bas);
    dessiner_bas(v, tampon);
}

void vl_dessiner_progres(C2D_TextBuf tampon, C3D_RenderTarget *haut, C3D_RenderTarget *bas,
                         int fait, int total, const char *titre)
{
    char tmp[64];
    C2D_TargetClear(haut, C_FOND);
    C2D_SceneBegin(haut);
    entete(tampon, "Synchronisation", NULL);
    dessin_texte(tampon, total ? "Téléchargement des recettes..." : "Lecture du menu...",
                 MARGE, 60, 0.8f, C_TEXTE, 0);
    if (total) {
        snprintf(tmp, sizeof(tmp), "%d / %d", fait < total ? fait + 1 : total, total);
        dessin_texte(tampon, tmp, MARGE, 100, 0.7f, C_ACCENT, 0);
        char court[TAILLE_TITRE];
        mep_tronquer(titre, HAUT_LARGEUR - 2 * MARGE, 0.6f, dessin_mesure, NULL, court,
                     sizeof(court));
        dessin_texte(tampon, court, MARGE, 134, 0.6f, C_DOUX, 0);
        C2D_DrawRectSolid(MARGE, 180, 0.5f, HAUT_LARGEUR - 2 * MARGE, 8, C_BOUTON);
        C2D_DrawRectSolid(MARGE, 180, 0.5f, (HAUT_LARGEUR - 2 * MARGE) * fait / total, 8, C_ACCENT);
    }
    C2D_TargetClear(bas, C_FOND);
    C2D_SceneBegin(bas);
    dessin_texte(tampon, "Patiente un instant...", BAS_LARGEUR / 2, 100, 0.6f, C_DOUX,
                 C2D_AlignCenter);
}
