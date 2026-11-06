/*
 * Catalogue des recettes de la carte SD (voir catalogue.h).
 */
#include "catalogue.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fichiers.h"
#include "recette_json.h"

static char contenu[TAILLE_FICHIER_MAX];   /* fichier en cours de lecture */
static Recette temporaire;                 /* sert à lire les titres */

void catalogue_preparer(const char *dossier)
{
    dossier_creer(dossier);
}

bool catalogue_charger(const char *dossier, const char *fichier, Recette *r,
                       char *erreur, int taille_erreur)
{
    char chemin[256];
    snprintf(chemin, sizeof(chemin), "%s/%s", dossier, fichier);
    int lg = fichier_lire(chemin, contenu, sizeof(contenu));
    if (lg == FICHIER_ILLISIBLE) {
        snprintf(erreur, (size_t)taille_erreur, "fichier illisible");
        return false;
    }
    if (lg == FICHIER_TROP_GROS) {
        snprintf(erreur, (size_t)taille_erreur, "fichier trop gros (plus de %d Ko)",
                 TAILLE_FICHIER_MAX / 1024);
        return false;
    }
    return recette_depuis_json(contenu, lg, r, erreur, taille_erreur);
}

void catalogue_cle_tri(const char *texte, char *sortie, int taille)
{
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)texte; *p && n < taille - 2; p++) {
        char c = 0;
        const char *double_lettre = NULL;
        if (p[0] == 0xC3 && p[1]) {
            unsigned b = p[1] | 0x20;          /* majuscule → minuscule (À→à…) */
            if (b >= 0xA0 && b <= 0xA5) c = 'a';
            else if (b == 0xA7) c = 'c';
            else if (b >= 0xA8 && b <= 0xAB) c = 'e';
            else if (b >= 0xAC && b <= 0xAF) c = 'i';
            else if (b == 0xB1) c = 'n';
            else if (b >= 0xB2 && b <= 0xB6) c = 'o';
            else if (b >= 0xB9 && b <= 0xBC) c = 'u';
            else if (b == 0xBD || b == 0xBF) c = 'y';
            if (c) p++;
        } else if (p[0] == 0xC5 && (p[1] == 0x92 || p[1] == 0x93)) {
            double_lettre = "oe";              /* Œ œ */
            p++;
        }
        if (double_lettre) {
            sortie[n++] = double_lettre[0];
            sortie[n++] = double_lettre[1];
        } else {
            sortie[n++] = c ? c : (char)tolower(*p);
        }
    }
    sortie[n] = '\0';
}

static int comparer(const void *a, const void *b)
{
    char ka[TAILLE_TITRE + 8], kb[TAILLE_TITRE + 8];
    catalogue_cle_tri(((const EntreeCatalogue *)a)->titre, ka, sizeof(ka));
    catalogue_cle_tri(((const EntreeCatalogue *)b)->titre, kb, sizeof(kb));
    return strcmp(ka, kb);
}

static bool est_json(const char *nom)
{
    size_t lg = strlen(nom);
    /* Les fichiers « ._xxx » sont créés par macOS sur les cartes SD : on les ignore. */
    if (nom[0] == '.' || lg < 6 || lg >= TAILLE_NOM_FICHIER) return false;
    const char *ext = nom + lg - 5;
    return tolower((unsigned char)ext[0]) == '.' && tolower((unsigned char)ext[1]) == 'j'
        && tolower((unsigned char)ext[2]) == 's' && tolower((unsigned char)ext[3]) == 'o'
        && tolower((unsigned char)ext[4]) == 'n';
}

void catalogue_lire(Catalogue *c, const char *dossier)
{
    memset(c, 0, sizeof(*c));
    DIR *d = opendir(dossier);
    if (!d) {
        c->dossier_absent = true;
        return;
    }
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (!est_json(ent->d_name)) continue;
        if (c->nb >= CATALOGUE_MAX) {
            c->plein = true;
            break;
        }
        char erreur[128];
        if (!catalogue_charger(dossier, ent->d_name, &temporaire, erreur, sizeof(erreur))) {
            if (c->nb_illisibles++ == 0)
                snprintf(c->erreur, sizeof(c->erreur), "%s : %s", ent->d_name, erreur);
            continue;
        }
        EntreeCatalogue *e = &c->entrees[c->nb++];
        copier_texte(e->fichier, ent->d_name, TAILLE_NOM_FICHIER);
        copier_texte(e->id, temporaire.id, TAILLE_ID);
        copier_texte(e->rev, temporaire.rev, sizeof(e->rev));
        e->echelle = 1;
        copier_texte(e->titre, temporaire.titre, TAILLE_TITRE);
        e->nb_etapes = temporaire.nb_etapes;
        e->nb_minuteurs = temporaire.nb_minuteurs;
        e->a_portions = temporaire.a_portions;
        e->portions_nombre = temporaire.portions_nombre;
        copier_texte(e->portions_unite, temporaire.portions_unite, TAILLE_UNITE);
    }
    closedir(d);
    qsort(c->entrees, (size_t)c->nb, sizeof(c->entrees[0]), comparer);
}

void catalogue_appliquer_menu(Catalogue *c, const Menu *m)
{
    static EntreeCatalogue ordonnees[CATALOGUE_MAX];
    bool pris[CATALOGUE_MAX] = {false};
    int n = 0;
    for (int k = 0; k < m->nb; k++) {
        for (int i = 0; i < c->nb; i++) {
            if (pris[i] || strcmp(c->entrees[i].id, m->entrees[k].id) != 0) continue;
            EntreeCatalogue *e = &ordonnees[n++];
            *e = c->entrees[i];
            e->dans_menu = true;
            copier_texte(e->date, m->entrees[k].date, sizeof(e->date));
            copier_texte(e->repas, m->entrees[k].repas, sizeof(e->repas));
            e->echelle = m->entrees[k].echelle > 0 ? m->entrees[k].echelle : 1;
            pris[i] = true;
            break;
        }
    }
    for (int i = 0; i < c->nb; i++)
        if (!pris[i]) ordonnees[n++] = c->entrees[i];   /* déjà triées par titre */
    memcpy(c->entrees, ordonnees, sizeof(ordonnees[0]) * (size_t)n);
}
