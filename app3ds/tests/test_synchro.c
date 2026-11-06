/*
 * Tests des réglages, du menu, du catalogue ordonné et de la synchronisation,
 * avec un faux serveur (table url → réponse) et un dossier temporaire.
 */
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "catalogue.h"
#include "config.h"
#include "fichiers.h"
#include "menu.h"
#include "photo.h"
#include "synchro.h"
#include "verif.h"

/* --- Faux serveur ------------------------------------------------------------------ */

typedef struct { const char *url; const char *reponse; int longueur; } Route;
static Route routes[16];
static int nb_routes, nb_appels;
static char derniers_appels[32][128];

static void route_binaire(const char *url, const char *reponse, int longueur)
{
    for (int i = 0; i < nb_routes; i++)
        if (strcmp(routes[i].url, url) == 0) {
            routes[i].reponse = reponse;
            routes[i].longueur = longueur;
            return;
        }
    routes[nb_routes++] = (Route){url, reponse, longueur};
}

static void route(const char *url, const char *reponse)
{
    route_binaire(url, reponse, reponse ? (int)strlen(reponse) : 0);
}

static int faux_telecharger(void *ctx, const char *url, char *tampon, int max,
                            char *erreur, int taille_erreur)
{
    (void)ctx;
    if (nb_appels < 32) snprintf(derniers_appels[nb_appels], 128, "%s", url);
    nb_appels++;
    for (int i = 0; i < nb_routes; i++) {
        if (strcmp(routes[i].url, url) != 0) continue;
        if (!routes[i].reponse) break;
        int lg = routes[i].longueur;
        if (lg > max) lg = max;
        memcpy(tampon, routes[i].reponse, (size_t)lg);
        return lg;
    }
    snprintf(erreur, (size_t)taille_erreur, "délai dépassé");
    return -1;
}

static int progres_appels;
static void progres(void *ctx, int fait, int total, const char *titre)
{
    (void)ctx; (void)fait; (void)total; (void)titre;
    progres_appels++;
}

static bool existe(const char *chemin)
{
    struct stat st;
    return stat(chemin, &st) == 0;
}

static void ecrire(const char *chemin, const char *texte)
{
    FILE *f = fopen(chemin, "wb");
    fputs(texte, f);
    fclose(f);
}

#define A  "http://mac:8383"
#define MENU_2 "{\"v\":1,\"maj\":1700000000,\"recettes\":[" \
    "{\"id\":\"416\",\"titre\":\"Acras\",\"rev\":\"r1\",\"image\":true," \
    "\"date\":\"2026-11-07\",\"repas\":\"Dîner\",\"echelle\":1.5}," \
    "{\"id\":\"388\",\"titre\":\"Cake coco\",\"rev\":\"c1\",\"image\":false}]}"
#define ACRAS_R1 "{\"v\":1,\"id\":\"416\",\"titre\":\"Acras\",\"rev\":\"r1\"," \
    "\"etapes\":[{\"texte\":\"Cuire.\"}]}"
#define ACRAS_R2 "{\"v\":1,\"id\":\"416\",\"titre\":\"Acras\",\"rev\":\"r2\"," \
    "\"etapes\":[{\"texte\":\"Cuire 20 min.\"}]}"
#define CAKE_C1 "{\"v\":1,\"id\":\"388\",\"titre\":\"Cake coco\",\"rev\":\"c1\"," \
    "\"etapes\":[{\"texte\":\"Mélanger.\"},{\"texte\":\"Cuire.\"}]}"

int main(void)
{
    char texte[256];

    /* --- Adresse de la passerelle --- */
    struct { const char *saisie, *attendu; } adresses[] = {
        {"192.168.1.10", "http://192.168.1.10:8383"},
        {"  http://192.168.1.10:8383/ ", "http://192.168.1.10:8383"},
        {"HTTP://NAS.local:9000", "http://nas.local:9000"},
        {"nas-cuisine", "http://nas-cuisine:8383"},
    };
    for (int i = 0; i < 4; i++) {
        VERIF_NOMBRE(config_normaliser_adresse(adresses[i].saisie, texte, sizeof(texte)), 1);
        VERIF_TEXTE(texte, adresses[i].attendu);
    }
    const char *refusees[] = {"", "   ", "https://nas:8383", "http://", "nas:0", "nas:70000",
                              "nas:12a", "nas/chemin", "n a s", "nas:", ":8383", "nas;rm"};
    for (int i = 0; i < 12; i++)
        VERIF_NOMBRE(config_normaliser_adresse(refusees[i], texte, sizeof(texte)), 0);

    /* --- Dossier temporaire = fausse carte SD --- */
    char modele[] = "synchro_XXXXXX";
    char *racine = mkdtemp(modele);
    char dossier[128], chemin_menu[128], chemin_config[128], chemin[256];
    snprintf(dossier, sizeof(dossier), "%s/3ds/recettes/cache", racine);
    snprintf(chemin_menu, sizeof(chemin_menu), "%s/3ds/recettes/menu.json", racine);
    snprintf(chemin_config, sizeof(chemin_config), "%s/3ds/recettes/config.ini", racine);

    /* --- Réglages : absent, écrit, relu, fichier abîmé --- */
    Config cfg;
    VERIF_NOMBRE(config_lire(chemin_config, &cfg), 0);
    VERIF_TEXTE(cfg.passerelle, "");
    dossier_creer(dossier);
    snprintf(cfg.passerelle, sizeof(cfg.passerelle), "%s", A);
    VERIF_NOMBRE(config_ecrire(chemin_config, &cfg), 1);
    Config relue;
    VERIF_NOMBRE(config_lire(chemin_config, &relue), 1);
    VERIF_TEXTE(relue.passerelle, A);
    snprintf(chemin, sizeof(chemin), "%s.tmp", chemin_config);
    VERIF_NOMBRE(existe(chemin), 0);                       /* pas de .tmp qui traîne */
    ecrire(chemin_config, "# commentaire\r\n  passerelle   =   192.168.1.20 \r\ninconnu = x\n");
    config_lire(chemin_config, &relue);
    VERIF_TEXTE(relue.passerelle, "http://192.168.1.20:8383");
    ecrire(chemin_config, "passerelle = https://pas-de-tls\n");
    config_lire(chemin_config, &relue);
    VERIF_TEXTE(relue.passerelle, "");

    /* Micro : valeurs par défaut, enregistrement, valeurs abîmées ignorées. */
    VERIF_NOMBRE(relue.micro == true && relue.sensibilite == 3, 1);
    cfg.micro = false;
    cfg.sensibilite = 5;
    VERIF_NOMBRE(config_ecrire(chemin_config, &cfg), 1);
    config_lire(chemin_config, &relue);
    VERIF_NOMBRE(relue.micro == false && relue.sensibilite == 5, 1);
    VERIF_TEXTE(relue.passerelle, A);
    ecrire(chemin_config, "micro = peut-etre\nsensibilite = 9\n");
    config_lire(chemin_config, &relue);
    VERIF_NOMBRE(relue.micro == true && relue.sensibilite == 3, 1);

    /* Caméra : coupée par défaut, enregistrée, valeurs abîmées ignorées. */
    VERIF_NOMBRE(relue.camera == false && relue.sensibilite_camera == 3, 1);
    cfg.camera = true;
    cfg.sensibilite_camera = 4;
    VERIF_NOMBRE(config_ecrire(chemin_config, &cfg), 1);
    config_lire(chemin_config, &relue);
    VERIF_NOMBRE(relue.camera == true && relue.sensibilite_camera == 4, 1);
    VERIF_NOMBRE(relue.micro == false && relue.sensibilite == 5, 1);   /* le reste ne bouge pas */
    ecrire(chemin_config, "camera = oui\nsensibilite_camera = 0\n");
    config_lire(chemin_config, &relue);
    VERIF_NOMBRE(relue.camera == true && relue.sensibilite_camera == 3, 1);

    /* --- Message d'erreur de la passerelle --- */
    const char *e502 = "{\"erreur\":\"Mealie refuse le jeton d'API\"}";
    VERIF_NOMBRE(synchro_erreur_passerelle(e502, (int)strlen(e502), texte, sizeof(texte)), 1);
    VERIF_TEXTE(texte, "Mealie refuse le jeton d'API");
    VERIF_NOMBRE(synchro_erreur_passerelle("<html>502</html>", 16, texte, sizeof(texte)), 0);
    VERIF_NOMBRE(synchro_erreur_passerelle("{\"autre\":1}", 11, texte, sizeof(texte)), 0);

    /* --- Menu --- */
    static Menu m;
    VERIF_NOMBRE(menu_depuis_json(MENU_2, (int)strlen(MENU_2), &m, texte, sizeof(texte)), 1);
    VERIF_NOMBRE(m.nb, 2);
    VERIF_NOMBRE(m.maj, 1700000000);
    VERIF_TEXTE(m.entrees[0].repas, "Dîner");
    VERIF_NOMBRE(m.entrees[0].echelle, 1.5);
    VERIF_NOMBRE(m.entrees[1].echelle, 1);                 /* absente → 1 */
    const char *piege = "{\"recettes\":[{\"id\":\"../../config\",\"rev\":\"x\"},{\"id\":\"ok\"}]}";
    VERIF_NOMBRE(menu_depuis_json(piege, (int)strlen(piege), &m, texte, sizeof(texte)), 1);
    VERIF_NOMBRE(m.nb == 1 && m.ignorees == 1, 1);         /* « ../ » rejeté */
    VERIF_NOMBRE(menu_depuis_json("[]", 2, &m, texte, sizeof(texte)), 0);
    VERIF_NOMBRE(identifiant_valide("6f1c2d3e4a5b4c6d8e9f0a1b2c3d4e5f"), 1);
    VERIF_NOMBRE(identifiant_valide("6f1c2d3e4a5b4c6d8e9f0a1b2c3d4e5f0"), 0);   /* 33 car. */
    VERIF_NOMBRE(identifiant_valide("a.json"), 0);

    menu_libelle_jour("2026-11-07", texte, sizeof(texte));
    VERIF_TEXTE(texte, "Samedi 7");
    menu_libelle_jour("2027-01-01", texte, sizeof(texte));
    VERIF_TEXTE(texte, "Vendredi 1");
    menu_libelle_jour("2028-02-29", texte, sizeof(texte));
    VERIF_TEXTE(texte, "Mardi 29");
    menu_libelle_jour("pas une date", texte, sizeof(texte));
    VERIF_TEXTE(texte, "");

    /* --- 1re synchro : passerelle injoignable, la carte reste intacte --- */
    BilanSynchro b;
    synchroniser(A, dossier, chemin_menu, faux_telecharger, progres, NULL, &b);
    VERIF_NOMBRE(b.menu_ok, 0);
    VERIF_TEXTE(b.erreur, "Passerelle injoignable (http://mac:8383) : délai dépassé");
    VERIF_NOMBRE(existe(chemin_menu), 0);

    /* --- 2e synchro : tout est téléchargé --- */
    route(A "/menu", MENU_2);
    route(A "/recipes/416", ACRAS_R1);
    route(A "/recipes/388", CAKE_C1);
    /* Photo 2 × 1 de l'acras (388 n'a pas de photo : "image": false). */
    static const char photo_ok[] = {'R', '5', '6', '5', 2, 0, 1, 0, 0x00, (char)0xF8, 0x1F, 0x00};
    route_binaire(A "/recipes/416/image.rgb565", photo_ok, (int)sizeof(photo_ok));
    snprintf(chemin, sizeof(chemin), "%s/388" PHOTO_EXTENSION, dossier);   /* photo périmée */
    ecrire(chemin, "R565");
    snprintf(chemin, sizeof(chemin), "%s/ancienne.json", dossier);   /* plus au menu */
    ecrire(chemin, CAKE_C1);
    snprintf(chemin, sizeof(chemin), "%s/ancienne.jpg", dossier);
    ecrire(chemin, "jpeg");
    snprintf(chemin, sizeof(chemin), "%s/388.json.tmp", dossier);    /* écriture interrompue */
    ecrire(chemin, "{");
    nb_appels = 0;
    progres_appels = 0;
    synchroniser(A, dossier, chemin_menu, faux_telecharger, progres, NULL, &b);
    VERIF_NOMBRE(b.menu_ok && b.nb_menu == 2 && b.telechargees == 2 && b.echecs == 0, 1);
    VERIF_NOMBRE(b.supprimees, 1);
    VERIF_NOMBRE(nb_appels, 4);                           /* menu, 2 recettes, 1 photo */
    VERIF_NOMBRE(b.photos == 1 && b.photos_echecs == 0, 1);
    snprintf(chemin, sizeof(chemin), "%s/416" PHOTO_EXTENSION, dossier);
    VERIF_NOMBRE(existe(chemin), 1);
    snprintf(chemin, sizeof(chemin), "%s/388" PHOTO_EXTENSION, dossier);
    VERIF_NOMBRE(existe(chemin), 0);                      /* plus de photo pour 388 */
    VERIF_NOMBRE(progres_appels, 4);                      /* menu + 2 recettes + fin */
    VERIF_NOMBRE(existe(chemin_menu), 1);
    snprintf(chemin, sizeof(chemin), "%s/ancienne.json", dossier);
    VERIF_NOMBRE(existe(chemin), 0);
    snprintf(chemin, sizeof(chemin), "%s/ancienne.jpg", dossier);
    VERIF_NOMBRE(existe(chemin), 0);
    snprintf(chemin, sizeof(chemin), "%s/388.json.tmp", dossier);
    VERIF_NOMBRE(existe(chemin), 0);
    synchro_resume(&b, texte, sizeof(texte));
    VERIF_TEXTE(texte, "Synchronisé : 2 recettes au menu, 2 téléchargées.");

    /* --- Catalogue dans l'ordre du menu --- */
    static Catalogue cat;
    catalogue_lire(&cat, dossier);
    VERIF_TEXTE(cat.entrees[0].titre, "Acras");           /* ordre alphabétique */
    menu_lire(chemin_menu, &m);
    route(A "/menu", "{\"recettes\":[{\"id\":\"388\",\"rev\":\"c1\"},{\"id\":\"416\",\"rev\":\"r1\"}]}");
    Menu inverse;
    const char *mi = "{\"recettes\":[{\"id\":\"388\",\"date\":\"2026-11-08\",\"repas\":\"Déjeuner\"},"
                     "{\"id\":\"absent\"},{\"id\":\"416\",\"echelle\":2}]}";
    menu_depuis_json(mi, (int)strlen(mi), &inverse, texte, sizeof(texte));
    catalogue_appliquer_menu(&cat, &inverse);
    VERIF_NOMBRE(cat.nb, 2);
    VERIF_TEXTE(cat.entrees[0].id, "388");
    VERIF_TEXTE(cat.entrees[0].repas, "Déjeuner");
    VERIF_NOMBRE(cat.entrees[0].dans_menu, 1);
    VERIF_NOMBRE(cat.entrees[1].echelle, 2);

    /* --- 3e synchro : rien n'a changé → seul le menu est téléchargé --- */
    route(A "/menu", MENU_2);
    nb_appels = 0;
    synchroniser(A, dossier, chemin_menu, faux_telecharger, NULL, NULL, &b);
    VERIF_NOMBRE(b.a_jour == 2 && b.telechargees == 0, 1);
    VERIF_NOMBRE(nb_appels, 1);                           /* la photo est déjà là */

    /* Photo effacée de la carte : elle seule est retéléchargée. */
    snprintf(chemin, sizeof(chemin), "%s/416" PHOTO_EXTENSION, dossier);
    remove(chemin);
    nb_appels = 0;
    synchroniser(A, dossier, chemin_menu, faux_telecharger, NULL, NULL, &b);
    VERIF_NOMBRE(nb_appels == 2 && b.photos == 1 && existe(chemin), 1);

    /* Photo abîmée ou absente du serveur : la recette reste, la photo est comptée en échec. */
    remove(chemin);
    static const char photo_abimee[] = {'R', '5', '6', '5', 2, 0, 1, 0, 0x00};
    route_binaire(A "/recipes/416/image.rgb565", photo_abimee, (int)sizeof(photo_abimee));
    synchroniser(A, dossier, chemin_menu, faux_telecharger, NULL, NULL, &b);
    VERIF_NOMBRE(b.photos_echecs == 1 && b.echecs == 0 && !existe(chemin), 1);
    synchro_resume(&b, texte, sizeof(texte));
    VERIF_TEXTE(texte, "Synchronisé : 2 recettes au menu, 1 photo non reçue.");
    route(A "/recipes/416/image.rgb565", NULL);            /* 404 */
    synchroniser(A, dossier, chemin_menu, faux_telecharger, NULL, NULL, &b);
    VERIF_NOMBRE(b.photos_echecs == 1 && b.echecs == 0, 1);
    route_binaire(A "/recipes/416/image.rgb565", photo_ok, (int)sizeof(photo_ok));

    /* --- 4e synchro : une recette change, l'autre arrive abîmée --- */
    char menu_modifie[512];
    snprintf(menu_modifie, sizeof(menu_modifie), "%s", MENU_2);
    memcpy(strstr(menu_modifie, "\"r1\""), "\"r2\"", 4);
    memcpy(strstr(menu_modifie, "\"c1\""), "\"c2\"", 4);
    route(A "/menu", menu_modifie);
    route(A "/recipes/416", ACRAS_R2);
    route(A "/recipes/388", "{\"v\":1,\"id\":\"388\",\"tit");          /* coupée */
    synchroniser(A, dossier, chemin_menu, faux_telecharger, NULL, NULL, &b);
    VERIF_NOMBRE(b.telechargees == 1 && b.echecs == 1, 1);
    static Recette r;
    VERIF_NOMBRE(catalogue_charger(dossier, "416.json", &r, texte, sizeof(texte)), 1);
    VERIF_TEXTE(r.rev, "r2");
    /* L'ancienne version du cake est toujours là, intacte. */
    VERIF_NOMBRE(catalogue_charger(dossier, "388.json", &r, texte, sizeof(texte)), 1);
    VERIF_TEXTE(r.rev, "c1");
    synchro_resume(&b, texte, sizeof(texte));
    VERIF_NOMBRE(strstr(texte, "1 en échec (Cake coco : JSON invalide") != NULL, 1);

    /* Une recette qui ne correspond pas à l'identifiant demandé est refusée. */
    route(A "/recipes/388", ACRAS_R2);
    synchroniser(A, dossier, chemin_menu, faux_telecharger, NULL, NULL, &b);
    VERIF_NOMBRE(b.echecs, 1);
    VERIF_NOMBRE(strstr(b.erreur, "identifiant inattendu") != NULL, 1);

    /* --- Menu vide : le cache est vidé --- */
    route(A "/menu", "{\"v\":1,\"recettes\":[]}");
    synchroniser(A, dossier, chemin_menu, faux_telecharger, NULL, NULL, &b);
    VERIF_NOMBRE(b.menu_ok && b.supprimees == 2, 1);
    catalogue_lire(&cat, dossier);
    VERIF_NOMBRE(cat.nb, 0);

    snprintf(chemin, sizeof(chemin), "rm -rf '%s'", racine);
    VERIF_NOMBRE(system(chemin), 0);
    return VERIF_BILAN("test_synchro");
}
