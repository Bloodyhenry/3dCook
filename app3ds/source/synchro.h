/*
 * Synchronisation avec la passerelle : télécharge le menu et les recettes
 * qui ont changé, et les range dans le cache de la carte SD.
 *
 *   1. GET /menu ; s'il échoue, on s'arrête et le cache reste intact
 *      (on cuisine alors hors ligne avec ce qui est déjà sur la carte) ;
 *   2. pour chaque recette du menu : si le fichier local a la même « rev »,
 *      rien à faire ; sinon GET /recipes/<id>, vérification, écriture atomique ;
 *   3. les recettes qui ne sont plus au menu sont retirées du cache.
 *
 * Une recette qui ne se télécharge pas, ou qui arrive abîmée, ne remplace
 * jamais la version déjà présente sur la carte.
 *
 * Le téléchargement est fourni par l'appelant (httpc sur la console, un faux
 * serveur dans les tests) : ce module est portable.
 */
#ifndef SYNCHRO_H
#define SYNCHRO_H

#include <stdbool.h>

/* Télécharge `url` dans `tampon` ; renvoie la taille, ou -1 avec `erreur` rempli. */
typedef int (*FonctionTelecharger)(void *contexte, const char *url, char *tampon, int max,
                                   char *erreur, int taille_erreur);

/* Appelée avant chaque recette, pour afficher l'avancement (peut être NULL). */
typedef void (*FonctionProgres)(void *contexte, int fait, int total, const char *titre);

typedef struct {
    bool menu_ok;          /* le menu a été reçu */
    int  nb_menu;          /* recettes au menu */
    int  telechargees;
    int  a_jour;           /* déjà présentes avec la bonne version */
    int  echecs;
    int  supprimees;       /* retirées du cache (plus au menu) */
    char erreur[200];      /* première erreur rencontrée, "" sinon */
} BilanSynchro;

void synchroniser(const char *adresse, const char *dossier_cache, const char *chemin_menu,
                  FonctionTelecharger telecharger, FonctionProgres progres, void *contexte,
                  BilanSynchro *bilan);

/* Résumé d'une ligne pour l'écran (« 3 recettes téléchargées, 2 à jour »). */
void synchro_resume(const BilanSynchro *b, char *sortie, int taille);

#endif
