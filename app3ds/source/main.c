/*
 * Recettes « mains sales » pour Nintendo 3DS.
 *
 * Au démarrage, l'appli télécharge le menu et les recettes depuis la
 * passerelle (Wi-Fi) et les garde sur la carte SD ; sans réseau, elle
 * utilise ce qui est déjà sur la carte. On choisit ensuite une recette.
 * Étape 4 : minuteurs en parallèle, alarme (son + LED), veille bloquée.
 * Étape 5 : souffler dans le micro pour passer à l'étape suivante.
 * Étape 6 : ou passer la main devant la caméra intérieure.
 */
#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>

#include "alarme.h"
#include "camera.h"
#include "catalogue.h"
#include "config.h"
#include "dessin.h"
#include "menu.h"
#include "micro.h"
#include "minuterie.h"
#include "mouvement.h"
#include "photo_ecran.h"
#include "recette.h"
#include "reseau.h"
#include "saisie.h"
#include "souffle.h"
#include "synchro.h"
#include "vue_liste.h"
#include "vue_minuteurs.h"
#include "vue_recette.h"
#include "vue_reglages.h"

/* Touches qui arrêtent une alarme (toutes sauf START). */
#define TOUCHES_ALARME (KEY_A | KEY_B | KEY_X | KEY_Y | KEY_L | KEY_R | KEY_SELECT | KEY_TOUCH \
                        | KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT)
#define CONFIRMER_QUITTER_MS 3000

typedef enum { ECRAN_LISTE, ECRAN_RECETTE, ECRAN_REGLAGES } Ecran;

/* Gros objets en statique : la pile du thread principal est petite. */
static Catalogue catalogue;
static Menu menu;
static Recette recette;
static VueListe liste;
static VueRecette vue;
static Config config;
static Minuterie minuterie;
static VueReglages reglages;
static Souffle souffle;
static int16_t echantillons[4096];   /* micro : ~273 par image, marge pour les retards */
static Mouvement mouvement;
static uint8_t image_camera[MVT_L * MVT_H];
static bool camera_en_echec;          /* la caméra n'a pas démarré : on ne réessaie pas en boucle */

#define PAUSE_APRES_GESTE_MS 1500     /* souffle ou main : l'autre commande se tait */
#define CAMERA_PRISE_EN_MAIN_MS 1500  /* exposition automatique qui se règle au démarrage */

/* Cibles de dessin, pour l'écran d'avancement pendant la synchro. */
static C3D_RenderTarget *haut, *bas;
static C2D_TextBuf tampon;

/* Relit la carte SD : recettes du cache, rangées dans l'ordre du menu. */
static void charger_liste(void)
{
    catalogue_preparer(DOSSIER_RECETTES);
    catalogue_lire(&catalogue, DOSSIER_RECETTES);
    if (menu_lire(CHEMIN_MENU, &menu)) catalogue_appliquer_menu(&catalogue, &menu);
    photo_ecran_oublier();   /* les photos ont pu changer */
    vl_init(&liste, &catalogue, config.passerelle);
}

static void afficher_progres(void *contexte, int fait, int total, const char *titre)
{
    (void)contexte;
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TextBufClear(tampon);
    vl_dessiner_progres(tampon, haut, bas, fait, total, titre);
    C3D_FrameEnd(0);
}

static void synchroniser_menu(void)
{
    if (!config.passerelle[0]) {
        vl_message(&liste, "Adresse de la passerelle non réglée : touche « Réglages ».", true);
        return;
    }
    if (!reseau_wifi_connecte()) {
        vl_message(&liste, "Wi-Fi non connecté : recettes de la carte SD utilisées.", true);
        return;
    }
    BilanSynchro bilan;
    synchroniser(config.passerelle, DOSSIER_RECETTES, CHEMIN_MENU, reseau_telecharger,
                 afficher_progres, NULL, &bilan);
    charger_liste();
    char resume[300];
    synchro_resume(&bilan, resume, sizeof(resume));
    vl_message(&liste, resume, !bilan.menu_ok || bilan.echecs > 0);
}

static void regler_adresse(void)
{
    char saisie[TAILLE_ADRESSE], propre[TAILLE_ADRESSE];
    const char *depart = config.passerelle[0] ? config.passerelle : "http://192.168.1.";
    if (!saisir_adresse(depart, saisie, sizeof(saisie))) return;   /* annulé */
    if (!config_normaliser_adresse(saisie, propre, sizeof(propre))) {
        char message[200];
        snprintf(message, sizeof(message),
                 "Adresse invalide : « %s ». Exemple : 192.168.1.10:8383 (sans https).", saisie);
        vl_message(&liste, message, true);
        return;
    }
    copier_texte(config.passerelle, propre, sizeof(config.passerelle));
    if (!config_ecrire(CHEMIN_CONFIG, &config)) {
        vl_message(&liste, "Impossible d'enregistrer l'adresse sur la carte SD.", true);
        return;
    }
    vl_init(&liste, &catalogue, config.passerelle);
    synchroniser_menu();
}

int main(void)
{
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    dessin_init();
    reseau_init();
    alarme_init();
    minuterie_init(&minuterie);
    micro_init();

    haut = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bas = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    /* Tampon des glyphes de l'image en cours, vidé à chaque image. */
    tampon = C2D_TextBufNew(4096);

    config_lire(CHEMIN_CONFIG, &config);
    souffle_init(&souffle, config.sensibilite);
    mvt_init(&mouvement, config.sensibilite_camera);
    vg_init(&reglages, &config);
    charger_liste();
    synchroniser_menu();   /* au démarrage : menu à jour si la passerelle répond */
    Ecran ecran = ECRAN_LISTE;
    uint64_t quitter_jusqu_a = 0;   /* START déjà appuyé une fois, minuteurs en cours */

    while (aptMainLoop()) {
        hidScanInput();
        u32 appuis = hidKeysDown();
        touchPosition toucher;
        hidTouchRead(&toucher);
        uint64_t maintenant = osGetTime();

        /* Minuteurs : sonnerie, son, LED, veille. */
        minuterie_maj(&minuterie, maintenant);
        if (minuterie_sonne(&minuterie) && (appuis & TOUCHES_ALARME)) {
            minuterie_arreter_sonneries(&minuterie);
            appuis = 0;   /* ce toucher sert seulement à couper l'alarme */
        }
        alarme_maj(minuterie_sonne(&minuterie), minuterie_active(&minuterie), maintenant);

        /* --- Commandes sans les mains : souffle (micro) et main (caméra) --- */

        /* Micro. On n'écoute pas pendant une alarme (et 1 s après) pour ne pas
         * s'entendre biper. */
        int nb = micro_lire(echantillons, sizeof(echantillons) / sizeof(echantillons[0]));
        if (minuterie_sonne(&minuterie)) souffle_ignorer(&souffle, maintenant + 1000);
        bool souffle_reconnu = config.micro && souffle_analyser(&souffle, echantillons, nb, maintenant);

        /* Caméra : allumée seulement dans une recette ou les Réglages. */
        bool camera_voulue = config.camera && !camera_en_echec
                             && (ecran == ECRAN_RECETTE || ecran == ECRAN_REGLAGES);
        if (camera_voulue && !camera_active()) {
            if (camera_demarrer()) mvt_ignorer(&mouvement, maintenant + CAMERA_PRISE_EN_MAIN_MS);
            else camera_en_echec = true;
        } else if (!camera_voulue && camera_active()) {
            camera_arreter();
        }
        /* Une main qui va toucher l'écran ou un bouton passe devant la caméra :
         * on l'ignore pendant 1 s. Pendant une alarme aussi. */
        if (hidKeysHeld() || minuterie_sonne(&minuterie))
            mvt_ignorer(&mouvement, maintenant + 1000);
        bool main_reconnue = false;
        if (camera_active() && camera_image(image_camera, maintenant))
            main_reconnue = mvt_analyser(&mouvement, image_camera, maintenant);

        /* Un geste à la fois : l'autre commande se tait un moment. */
        if (souffle_reconnu) mvt_ignorer(&mouvement, maintenant + PAUSE_APRES_GESTE_MS);
        if (main_reconnue) souffle_ignorer(&souffle, maintenant + PAUSE_APRES_GESTE_MS);
        if ((souffle_reconnu || main_reconnue) && ecran == ECRAN_RECETTE && vr_suivante(&vue))
            vue.geste_jusqu_a = maintenant + 600;
        if (ecran == ECRAN_REGLAGES) {
            if (souffle_reconnu) vg_signaler_souffle(&reglages, maintenant);
            if (main_reconnue) vg_signaler_main(&reglages, maintenant);
        }
        vue.micro_actif = config.micro && micro_pret();
        vue.micro_niveau = souffle.niveau;
        vue.micro_seuil = souffle.seuil;
        vue.camera_active = camera_active();
        vue.camera_niveau = mouvement.mouvement;
        vue.camera_seuil = mouvement.seuil;

        /* START quitte ; avec des minuteurs en cours, il faut appuyer deux fois. */
        if (appuis & KEY_START) {
            if (!minuterie_active(&minuterie) || maintenant < quitter_jusqu_a) break;
            quitter_jusqu_a = maintenant + CONFIRMER_QUITTER_MS;
        }

        if (ecran == ECRAN_LISTE) {
            switch (vl_entrees(&liste, appuis, &toucher)) {
            case VL_OUVRIR: {
                const EntreeCatalogue *e = &catalogue.entrees[liste.selection];
                char erreur[160], message[300];
                if (catalogue_charger(DOSSIER_RECETTES, e->fichier, &recette,
                                      erreur, sizeof(erreur))) {
                    vr_init(&vue, &recette, e->echelle);
                    ecran = ECRAN_RECETTE;
                } else {
                    snprintf(message, sizeof(message), "%s : %s", e->titre, erreur);
                    vl_message(&liste, message, true);
                }
                break;
            }
            case VL_EXEMPLE:
                recette_demo(&recette);
                vr_init(&vue, &recette, 1);
                ecran = ECRAN_RECETTE;
                break;
            case VL_SYNCHRO:
                synchroniser_menu();
                break;
            case VL_REGLAGES:
                ecran = ECRAN_REGLAGES;
                break;
            case VL_RIEN:
                break;
            }
        } else if (ecran == ECRAN_REGLAGES) {
            switch (vg_entrees(&reglages, appuis, &toucher)) {
            case VRG_RETOUR:
                ecran = ECRAN_LISTE;
                break;
            case VRG_ADRESSE:
                regler_adresse();
                break;
            case VRG_MODIFIE:
                souffle_regler(&souffle, config.sensibilite);
                mvt_regler(&mouvement, config.sensibilite_camera);
                camera_en_echec = false;   /* caméra réactivée : on peut réessayer */
                if (!config_ecrire(CHEMIN_CONFIG, &config))
                    vl_message(&liste, "Impossible d'enregistrer les réglages sur la carte SD.", true);
                break;
            case VRG_RIEN:
                break;
            }
        } else if (vr_entrees(&vue, appuis, &toucher, &minuterie, maintenant) == VR_RETOUR) {
            ecran = ECRAN_LISTE;
        }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TextBufClear(tampon);
        if (ecran == ECRAN_LISTE) vl_dessiner(&liste, tampon, haut, bas, &minuterie, maintenant);
        else if (ecran == ECRAN_REGLAGES)
            vg_dessiner(&reglages, tampon, haut, bas, &souffle, micro_pret(), &mouvement,
                        camera_active(), maintenant);
        else vr_dessiner(&vue, tampon, haut, bas, &minuterie, maintenant);
        if (maintenant < quitter_jusqu_a) {
            C2D_SceneBegin(haut);
            C2D_DrawRectSolid(0, 0, 0.5f, HAUT_LARGEUR, 28, C_ACCENT);
            dessin_texte(tampon, "Minuteurs en cours : START encore pour quitter", HAUT_LARGEUR / 2,
                         5, 0.5f, C_TEXTE_FONCE, C2D_AlignCenter);
        }
        C2D_SceneBegin(bas);
        vm_alerte(tampon, &minuterie, maintenant);   /* plein écran si un minuteur sonne */
        C3D_FrameEnd(0);
    }

    camera_arreter();
    photo_ecran_fin();
    micro_fin();
    alarme_fin();
    C2D_TextBufDelete(tampon);
    reseau_fin();
    dessin_fin();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
