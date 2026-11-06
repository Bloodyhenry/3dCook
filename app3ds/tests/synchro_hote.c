/*
 * Outil de test de bout en bout : lance la VRAIE synchronisation de l'appli
 * contre une passerelle qui tourne, depuis le Mac. Le téléchargement passe
 * par curl au lieu de httpc ; tout le reste est le code de la console.
 *
 *   make -C tests synchro_hote
 *   tests/synchro_hote http://127.0.0.1:8383 /tmp/fausse-sd
 */
#include <stdio.h>
#include <string.h>

#include "catalogue.h"
#include "menu.h"
#include "synchro.h"

static int telecharger_curl(void *ctx, const char *url, char *tampon, int max,
                            char *erreur, int taille_erreur)
{
    (void)ctx;
    char commande[512];
    /* L'URL vient de l'appli (adresse + identifiant vérifié) : pas de guillemet possible. */
    if (strchr(url, '\'')) {
        snprintf(erreur, (size_t)taille_erreur, "adresse invalide");
        return -1;
    }
    snprintf(commande, sizeof(commande), "curl -s -f --max-time 6 '%s'", url);
    FILE *f = popen(commande, "r");
    if (!f) {
        snprintf(erreur, (size_t)taille_erreur, "curl indisponible");
        return -1;
    }
    int lu = (int)fread(tampon, 1, (size_t)max, f);
    int code = pclose(f);
    if (code != 0) {
        snprintf(erreur, (size_t)taille_erreur, "curl a échoué (code %d)", code);
        return -1;
    }
    return lu;
}

static void progres(void *ctx, int fait, int total, const char *titre)
{
    (void)ctx;
    if (total) fprintf(stderr, "  %d / %d : %s\n", fait < total ? fait + 1 : total, total, titre);
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage : %s http://adresse:port dossier-de-test\n", argv[0]);
        return 2;
    }
    char cache[300], chemin_menu[300];
    snprintf(cache, sizeof(cache), "%s/3ds/recettes/cache", argv[2]);
    snprintf(chemin_menu, sizeof(chemin_menu), "%s/3ds/recettes/menu.json", argv[2]);

    BilanSynchro b;
    synchroniser(argv[1], cache, chemin_menu, telecharger_curl, progres, NULL, &b);
    char resume[400];
    synchro_resume(&b, resume, sizeof(resume));
    printf("%s\n", resume);

    static Catalogue cat;
    static Menu m;
    catalogue_lire(&cat, cache);
    if (menu_lire(chemin_menu, &m)) catalogue_appliquer_menu(&cat, &m);
    for (int i = 0; i < cat.nb; i++) {
        char jour[32];
        menu_libelle_jour(cat.entrees[i].date, jour, sizeof(jour));
        printf("  %-12s %-10s ×%-4g %-38s %d étapes, %d minuteurs\n", jour, cat.entrees[i].repas,
               cat.entrees[i].echelle, cat.entrees[i].titre, cat.entrees[i].nb_etapes,
               cat.entrees[i].nb_minuteurs);
    }
    return b.menu_ok && b.echecs == 0 ? 0 : 1;
}
