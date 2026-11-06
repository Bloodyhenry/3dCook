/* Synchronisation avec la passerelle (voir synchro.h). */
#include "synchro.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "catalogue.h"
#include "fichiers.h"
#include "menu.h"
#include "recette_json.h"

static char reponse[TAILLE_FICHIER_MAX];
static Recette recette;
static Menu menu;

static void noter_erreur(BilanSynchro *b, const char *format, const char *a, const char *c)
{
    if (b->erreur[0]) return;   /* on garde la première */
    snprintf(b->erreur, sizeof(b->erreur), format, a, c);
}

/* La recette locale <id>.json est-elle déjà à la version `rev` ? */
static bool deja_a_jour(const char *dossier, const EntreeMenu *e)
{
    if (!e->rev[0]) return false;
    char nom[TAILLE_NOM_FICHIER], erreur[128];
    snprintf(nom, sizeof(nom), "%s.json", e->id);
    if (!catalogue_charger(dossier, nom, &recette, erreur, sizeof(erreur))) return false;
    return strcmp(recette.rev, e->rev) == 0;
}

static bool au_menu(const char *id, int longueur)
{
    for (int k = 0; k < menu.nb; k++)
        if ((int)strlen(menu.entrees[k].id) == longueur
            && strncmp(menu.entrees[k].id, id, (size_t)longueur) == 0)
            return true;
    return false;
}

/* Retire du cache les fichiers des recettes qui ne sont plus au menu. */
static void nettoyer(const char *dossier, BilanSynchro *b)
{
    DIR *d = opendir(dossier);
    if (!d) return;
    static char a_supprimer[CATALOGUE_MAX * 2][TAILLE_NOM_FICHIER];   /* pas sur la pile */
    int n = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && n < CATALOGUE_MAX * 2) {
        const char *nom = ent->d_name;
        const char *point = strchr(nom, '.');
        if (!point || nom[0] == '.' || strlen(nom) >= TAILLE_NOM_FICHIER) continue;
        bool json = strcmp(point, ".json") == 0;
        bool photo = strcmp(point, ".jpg") == 0;
        bool reste = strcmp(point, ".json.tmp") == 0;   /* écriture interrompue */
        if (!json && !photo && !reste) continue;
        if (reste || !au_menu(nom, (int)(point - nom))) {
            snprintf(a_supprimer[n++], TAILLE_NOM_FICHIER, "%.63s", nom);
            if (json) b->supprimees++;
        }
    }
    closedir(d);
    /* Supprimer après avoir fini de lire le dossier. */
    for (int i = 0; i < n; i++) {
        char chemin[300];
        snprintf(chemin, sizeof(chemin), "%.200s/%.63s", dossier, a_supprimer[i]);
        remove(chemin);
    }
}

void synchroniser(const char *adresse, const char *dossier, const char *chemin_menu,
                  FonctionTelecharger telecharger, FonctionProgres progres, void *contexte,
                  BilanSynchro *b)
{
    memset(b, 0, sizeof(*b));
    char url[300], erreur[160];
    dossier_creer(dossier);

    /* 1. Le menu. */
    if (progres) progres(contexte, 0, 0, "menu");
    snprintf(url, sizeof(url), "%s/menu", adresse);
    int lg = telecharger(contexte, url, reponse, sizeof(reponse), erreur, sizeof(erreur));
    if (lg < 0) {
        noter_erreur(b, "Passerelle injoignable (%s) : %s", adresse, erreur);
        return;
    }
    if (!menu_depuis_json(reponse, lg, &menu, erreur, sizeof(erreur))) {
        noter_erreur(b, "%s%s", erreur, "");
        return;
    }
    b->menu_ok = true;
    b->nb_menu = menu.nb;
    if (!fichier_ecrire_atomique(chemin_menu, reponse, lg))
        noter_erreur(b, "Impossible d'écrire %s%s", chemin_menu, " sur la carte SD");

    /* 2. Les recettes qui ont changé. */
    for (int k = 0; k < menu.nb; k++) {
        const EntreeMenu *e = &menu.entrees[k];
        if (progres) progres(contexte, k, menu.nb, e->titre);
        if (deja_a_jour(dossier, e)) {
            b->a_jour++;
            continue;
        }
        snprintf(url, sizeof(url), "%s/recipes/%s", adresse, e->id);
        lg = telecharger(contexte, url, reponse, sizeof(reponse), erreur, sizeof(erreur));
        if (lg < 0) {
            b->echecs++;
            noter_erreur(b, "%s : %s", e->titre, erreur);
            continue;
        }
        /* On vérifie la recette AVANT d'écraser celle de la carte. */
        if (!recette_depuis_json(reponse, lg, &recette, erreur, sizeof(erreur))
            || strcmp(recette.id, e->id) != 0) {
            b->echecs++;
            noter_erreur(b, "%s : %s", e->titre, erreur[0] ? erreur : "identifiant inattendu");
            continue;
        }
        char chemin[300];
        snprintf(chemin, sizeof(chemin), "%s/%s.json", dossier, e->id);
        if (fichier_ecrire_atomique(chemin, reponse, lg)) {
            b->telechargees++;
        } else {
            b->echecs++;
            noter_erreur(b, "%s : %s", e->titre, "écriture impossible sur la carte SD");
        }
    }

    /* 3. Ménage. */
    nettoyer(dossier, b);
    if (progres) progres(contexte, menu.nb, menu.nb, "");
}

void synchro_resume(const BilanSynchro *b, char *sortie, int taille)
{
    if (!b->menu_ok) {
        snprintf(sortie, (size_t)taille, "%s Recettes de la carte SD utilisées.", b->erreur);
        return;
    }
    int n = snprintf(sortie, (size_t)taille, "Synchronisé : %d recette%s au menu", b->nb_menu,
                     b->nb_menu > 1 ? "s" : "");
    if (b->telechargees && n < taille)
        n += snprintf(sortie + n, (size_t)(taille - n), ", %d téléchargée%s", b->telechargees,
                      b->telechargees > 1 ? "s" : "");
    if (b->echecs && n < taille)
        n += snprintf(sortie + n, (size_t)(taille - n), ", %d en échec (%s)", b->echecs, b->erreur);
    if (n < taille) snprintf(sortie + n, (size_t)(taille - n), ".");
}
