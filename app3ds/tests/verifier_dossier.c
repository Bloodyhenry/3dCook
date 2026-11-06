/*
 * Outil de vérification : lit tous les .json d'un dossier avec le code de
 * l'appli et affiche une ligne par recette (séparateur tabulation) :
 *   fichier  ok  ingrédients  étapes  minuteurs  notes  tronquée  erreur
 * Sert à confronter le lecteur C à toute une base exportée par la passerelle.
 *
 *   make -C tests verifier_dossier && tests/verifier_dossier chemin/du/dossier
 */
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "catalogue.h"

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage : %s dossier\n", argv[0]);
        return 2;
    }
    DIR *d = opendir(argv[1]);
    if (!d) {
        perror(argv[1]);
        return 2;
    }
    static Recette r;
    int lues = 0, echecs = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t lg = strlen(e->d_name);
        if (lg < 6 || strcmp(e->d_name + lg - 5, ".json") != 0 || e->d_name[0] == '.') continue;
        char erreur[160];
        bool ok = catalogue_charger(argv[1], e->d_name, &r, erreur, sizeof(erreur));
        lues++;
        if (!ok) echecs++;
        printf("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n", e->d_name, ok, r.nb_ingredients, r.nb_etapes,
               r.nb_minuteurs, r.nb_notes, r.tronquee, ok ? "" : erreur);
    }
    closedir(d);
    fprintf(stderr, "%d fichier(s) lu(s), %d échec(s)\n", lues, echecs);
    return echecs ? 1 : 0;
}
