/*
 * Tests de la lecture d'une recette JSON (format de la passerelle) et du
 * catalogue de la carte SD (avec un dossier temporaire).
 * Les recettes sont inventées : aucune recette réelle dans les tests.
 */
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "catalogue.h"
#include "recette_json.h"
#include "verif.h"

static const char *TARTE =
    "{\"v\":1,\"id\":\"9f3c\",\"titre\":\"Tarte d'été\",\"rev\":\"a1b2c3d4\","
    "\"portions\":{\"nombre\":6,\"unite\":\"parts\"},\"congelable\":true,\"image\":false,"
    "\"groupes\":[{\"titre\":null,\"items\":["
    "{\"quantite\":200,\"quantite_max\":null,\"unite\":\"g\",\"nom\":\"de farine de blé\",\"allergene\":true},"
    "{\"quantite\":0.5,\"quantite_max\":null,\"unite\":null,\"nom\":\"citron\",\"allergene\":false},"
    "{\"quantite\":80,\"quantite_max\":100,\"unite\":\"g\",\"nom\":\"de sucre\",\"allergene\":false}]},"
    "{\"titre\":\"Garniture\",\"items\":["
    "{\"quantite\":null,\"quantite_max\":null,\"unite\":null,\"nom\":\"fruits de saison\",\"allergene\":false}]}],"
    "\"etapes\":[{\"texte\":\"Mélanger la farine.\"},"
    "{\"texte\":\"Cuire 30 min à 180°C.\",\"section\":\"Cuisson\"}],"
    "\"minuteurs\":[{\"etape\":1,\"secondes\":1800,\"secondes_max\":null,\"libelle\":\"Four 180°C 30 min\"},"
    "{\"etape\":7,\"secondes\":60,\"secondes_max\":null,\"libelle\":\"étape inexistante\"}],"
    "\"notes\":[\"Se conserve 2 jours.\"],\"conseil\":\"Un conseil.\",\"champ_futur\":[1,2,3]}";

static bool lire(const char *json, Recette *r, char *erreur)
{
    return recette_depuis_json(json, (int)strlen(json), r, erreur, 160);
}

static void ecrire_fichier(const char *chemin, const char *contenu, size_t taille)
{
    FILE *f = fopen(chemin, "wb");
    fwrite(contenu, 1, taille, f);
    fclose(f);
}

int main(void)
{
    static Recette r;
    char erreur[160];

    /* --- Recette complète --- */
    VERIF_NOMBRE(lire(TARTE, &r, erreur), 1);
    VERIF_TEXTE(r.id, "9f3c");
    VERIF_TEXTE(r.titre, "Tarte d'été");
    VERIF_TEXTE(r.rev, "a1b2c3d4");
    VERIF_NOMBRE(r.a_portions && r.portions_nombre == 6, 1);
    VERIF_TEXTE(r.portions_unite, "parts");
    VERIF_NOMBRE(r.congelable && !r.image, 1);
    VERIF_NOMBRE(r.nb_groupes, 2);
    VERIF_TEXTE(r.titres_groupes[0], "");
    VERIF_TEXTE(r.titres_groupes[1], "Garniture");
    VERIF_NOMBRE(r.nb_ingredients, 4);
    VERIF_NOMBRE(r.ingredients[0].a_quantite && r.ingredients[0].quantite == 200, 1);
    VERIF_NOMBRE(r.ingredients[0].allergene, 1);
    VERIF_TEXTE(r.ingredients[0].unite, "g");
    VERIF_NOMBRE(r.ingredients[1].quantite, 0.5);
    VERIF_TEXTE(r.ingredients[1].unite, "");
    VERIF_NOMBRE(r.ingredients[2].a_quantite_max && r.ingredients[2].quantite_max == 100, 1);
    VERIF_NOMBRE(r.ingredients[3].a_quantite, 0);
    VERIF_NOMBRE(r.ingredients[3].groupe, 1);
    VERIF_NOMBRE(r.nb_etapes, 2);
    VERIF_TEXTE(r.etapes[1].section, "Cuisson");
    VERIF_TEXTE(r.etapes[0].section, "");
    /* Le minuteur qui vise l'étape 7 (inexistante) est ignoré. */
    VERIF_NOMBRE(r.nb_minuteurs, 1);
    VERIF_NOMBRE(r.minuteurs[0].etape == 1 && r.minuteurs[0].secondes == 1800, 1);
    VERIF_NOMBRE(r.minuteurs[0].secondes_max, 0);
    VERIF_TEXTE(r.minuteurs[0].libelle, "Four 180°C 30 min");
    VERIF_NOMBRE(r.nb_notes, 1);
    VERIF_NOMBRE(r.tronquee, 0);

    /* --- Portions absentes --- */
    VERIF_NOMBRE(lire("{\"id\":\"1\",\"titre\":\"T\",\"portions\":null,"
                      "\"etapes\":[{\"texte\":\"Cuire.\"}]}", &r, erreur), 1);
    VERIF_NOMBRE(r.a_portions, 0);
    VERIF_NOMBRE(r.nb_groupes, 0);

    /* --- Refus, avec un message clair --- */
    VERIF_NOMBRE(lire("{\"v\":2,\"id\":\"1\",\"titre\":\"T\",\"etapes\":[{\"texte\":\"x\"}]}",
                      &r, erreur), 0);
    VERIF_TEXTE(erreur, "format v2 trop récent : mettre l'appli à jour");
    VERIF_NOMBRE(lire("{\"titre\":\"T\",\"etapes\":[{\"texte\":\"x\"}]}", &r, erreur), 0);
    VERIF_TEXTE(erreur, "id ou titre manquant");
    VERIF_NOMBRE(lire("{\"id\":\"1\",\"titre\":\"T\",\"etapes\":[]}", &r, erreur), 0);
    VERIF_TEXTE(erreur, "la recette n'a aucune étape");
    VERIF_NOMBRE(lire("[1,2]", &r, erreur), 0);
    VERIF_TEXTE(erreur, "le fichier ne contient pas une recette");
    /* Fichier coupé (copie interrompue). */
    VERIF_NOMBRE(recette_depuis_json(TARTE, 100, &r, erreur, 160), 0);
    VERIF_TEXTE(erreur, "JSON invalide (octet 100)");

    /* --- Limites dépassées : la recette reste lisible, marquée tronquée --- */
    static char gros[32768];
    int n = snprintf(gros, sizeof(gros), "{\"id\":\"g\",\"titre\":\"Grosse\",\"groupes\":[{\"items\":[");
    for (int i = 0; i < RECETTE_MAX_INGREDIENTS + 10; i++)
        n += snprintf(gros + n, sizeof(gros) - (size_t)n, "%s{\"quantite\":1,\"nom\":\"x%d\"}",
                      i ? "," : "", i);
    n += snprintf(gros + n, sizeof(gros) - (size_t)n, "]}],\"etapes\":[");
    for (int i = 0; i < RECETTE_MAX_ETAPES + 5; i++)
        n += snprintf(gros + n, sizeof(gros) - (size_t)n, "%s{\"texte\":\"e%d\"}", i ? "," : "", i);
    snprintf(gros + n, sizeof(gros) - (size_t)n, "]}");
    VERIF_NOMBRE(lire(gros, &r, erreur), 1);
    VERIF_NOMBRE(r.nb_ingredients, RECETTE_MAX_INGREDIENTS);
    VERIF_NOMBRE(r.nb_etapes, RECETTE_MAX_ETAPES);
    VERIF_NOMBRE(r.tronquee, 1);

    /* Texte d'étape trop long : coupé proprement. */
    static char long_texte[4000];  /* 3000 octets d'« é » */
    n = snprintf(long_texte, sizeof(long_texte), "{\"id\":\"l\",\"titre\":\"L\",\"etapes\":[{\"texte\":\"");
    for (int i = 0; i < 3000; i++) long_texte[n++] = (i % 2) ? '\xA9' : '\xC3';   /* « é » × 500 */
    snprintf(long_texte + n, sizeof(long_texte) - (size_t)n, "\"}]}");
    VERIF_NOMBRE(lire(long_texte, &r, erreur), 1);
    VERIF_NOMBRE((int)strlen(r.etapes[0].texte), TAILLE_ETAPE - 2);   /* nombre pair d'octets */
    VERIF_NOMBRE(r.tronquee, 1);

    /* --- Clé de tri --- */
    char cle[64];
    catalogue_cle_tri("Œufs Brouillés À l'Été", cle, sizeof(cle));
    VERIF_TEXTE(cle, "oeufs brouilles a l'ete");

    /* --- Catalogue sur un dossier temporaire --- */
    char modele[] = "catalogue_XXXXXX";
    char *racine = mkdtemp(modele);
    char dossier[128], chemin[256];
    snprintf(dossier, sizeof(dossier), "%s/3ds/recettes/cache", racine);

    static Catalogue cat;
    catalogue_lire(&cat, dossier);
    VERIF_NOMBRE(cat.dossier_absent, 1);
    catalogue_preparer(dossier);
    struct stat st;
    VERIF_NOMBRE(stat(dossier, &st) == 0 && S_ISDIR(st.st_mode), 1);

    snprintf(chemin, sizeof(chemin), "%s/9f3c.json", dossier);
    ecrire_fichier(chemin, TARTE, strlen(TARTE));
    const char *abricot = "{\"id\":\"2\",\"titre\":\"abricots rôtis\",\"etapes\":[{\"texte\":\"Rôtir.\"}]}";
    snprintf(chemin, sizeof(chemin), "%s/2.JSON", dossier);
    ecrire_fichier(chemin, abricot, strlen(abricot));
    snprintf(chemin, sizeof(chemin), "%s/casse.json", dossier);
    ecrire_fichier(chemin, "{\"id\":", 6);
    snprintf(chemin, sizeof(chemin), "%s/._9f3c.json", dossier);       /* fichier macOS */
    ecrire_fichier(chemin, "\0\5\26\7", 4);
    snprintf(chemin, sizeof(chemin), "%s/lisezmoi.txt", dossier);
    ecrire_fichier(chemin, "x", 1);
    snprintf(chemin, sizeof(chemin), "%s/zz_enorme.json", dossier);
    static char enorme[TAILLE_FICHIER_MAX + 10];
    memset(enorme, ' ', sizeof(enorme));
    ecrire_fichier(chemin, enorme, sizeof(enorme));

    catalogue_lire(&cat, dossier);
    VERIF_NOMBRE(cat.dossier_absent, 0);
    VERIF_NOMBRE(cat.nb, 2);
    VERIF_TEXTE(cat.entrees[0].titre, "abricots rôtis");      /* tri sans accents ni casse */
    VERIF_TEXTE(cat.entrees[1].titre, "Tarte d'été");
    VERIF_TEXTE(cat.entrees[1].fichier, "9f3c.json");
    VERIF_NOMBRE(cat.entrees[1].nb_etapes == 2 && cat.entrees[1].portions_nombre == 6, 1);
    VERIF_NOMBRE(cat.nb_illisibles, 2);
    VERIF_NOMBRE(strstr(cat.erreur, ".json : ") != NULL, 1);

    VERIF_NOMBRE(catalogue_charger(dossier, "9f3c.json", &r, erreur, sizeof(erreur)), 1);
    VERIF_TEXTE(r.titre, "Tarte d'été");
    VERIF_NOMBRE(catalogue_charger(dossier, "zz_enorme.json", &r, erreur, sizeof(erreur)), 0);
    VERIF_TEXTE(erreur, "fichier trop gros (plus de 64 Ko)");
    VERIF_NOMBRE(catalogue_charger(dossier, "absent.json", &r, erreur, sizeof(erreur)), 0);

    char commande[300];
    snprintf(commande, sizeof(commande), "rm -rf '%s'", racine);
    VERIF_NOMBRE(system(commande), 0);

    return VERIF_BILAN("test_recette_json");
}
