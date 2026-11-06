/*
 * JSON de la passerelle → Recette (voir recette_json.h).
 */
#include "recette_json.h"

#include <stdio.h>
#include <string.h>

#include "json.h"

/* Une recette de la base fait au plus ~600 jetons ; large marge. */
#define MAX_JETONS 4096
static JsonJeton jetons[MAX_JETONS];   /* statique : trop gros pour la pile */

/* Raccourcis sur le document en cours. */
static const char *T;
#define J jetons

static void texte_ou_vide(int indice, char *sortie, int taille)
{
    if (!json_texte(T, J, indice, sortie, taille)) sortie[0] = '\0';
}

static void lire_ingredient(int objet, int groupe, Recette *r)
{
    if (r->nb_ingredients >= RECETTE_MAX_INGREDIENTS) {
        r->tronquee = true;
        return;
    }
    Ingredient *i = &r->ingredients[r->nb_ingredients];
    memset(i, 0, sizeof(*i));
    texte_ou_vide(json_cle(T, J, objet, "nom"), i->nom, TAILLE_NOM);
    if (!i->nom[0]) return;                       /* ligne vide : ignorée */
    i->a_quantite = json_nombre(T, J, json_cle(T, J, objet, "quantite"), &i->quantite)
                    && i->quantite > 0;
    i->a_quantite_max = i->a_quantite
                        && json_nombre(T, J, json_cle(T, J, objet, "quantite_max"), &i->quantite_max)
                        && i->quantite_max > i->quantite;
    if (!i->a_quantite) i->quantite = 0;
    if (!i->a_quantite_max) i->quantite_max = 0;
    texte_ou_vide(json_cle(T, J, objet, "unite"), i->unite, TAILLE_UNITE);
    json_booleen(J, json_cle(T, J, objet, "allergene"), &i->allergene);
    i->groupe = groupe;
    r->nb_ingredients++;
}

static void lire_groupes(int tableau, Recette *r)
{
    if (tableau < 0 || J[tableau].type != JSON_TABLEAU) return;
    int g = json_premier(J, tableau);
    for (int n = 0; n < J[tableau].taille; n++, g = json_suivant(J, g)) {
        if (J[g].type != JSON_OBJET) continue;
        int indice;
        if (r->nb_groupes < RECETTE_MAX_GROUPES) {
            indice = r->nb_groupes++;
            texte_ou_vide(json_cle(T, J, g, "titre"), r->titres_groupes[indice], TAILLE_SECTION);
        } else {
            /* Trop de groupes : les ingrédients rejoignent le dernier. */
            indice = RECETTE_MAX_GROUPES - 1;
            r->tronquee = true;
        }
        int items = json_cle(T, J, g, "items");
        if (items < 0 || J[items].type != JSON_TABLEAU) continue;
        int it = json_premier(J, items);
        for (int k = 0; k < J[items].taille; k++, it = json_suivant(J, it))
            if (J[it].type == JSON_OBJET) lire_ingredient(it, indice, r);
    }
}

static void lire_etapes(int tableau, Recette *r)
{
    if (tableau < 0 || J[tableau].type != JSON_TABLEAU) return;
    int e = json_premier(J, tableau);
    for (int n = 0; n < J[tableau].taille; n++, e = json_suivant(J, e)) {
        if (J[e].type != JSON_OBJET) continue;
        if (r->nb_etapes >= RECETTE_MAX_ETAPES) {
            r->tronquee = true;
            return;
        }
        Etape *et = &r->etapes[r->nb_etapes];
        int texte = json_cle(T, J, e, "texte");
        texte_ou_vide(texte, et->texte, TAILLE_ETAPE);
        /* Texte plus long que la place prévue : coupé, on le signale. */
        if (texte >= 0 && J[texte].fin - J[texte].debut >= TAILLE_ETAPE) r->tronquee = true;
        if (!et->texte[0]) continue;
        texte_ou_vide(json_cle(T, J, e, "section"), et->section, TAILLE_SECTION);
        r->nb_etapes++;
    }
}

static void lire_minuteurs(int tableau, Recette *r)
{
    if (tableau < 0 || J[tableau].type != JSON_TABLEAU) return;
    int m = json_premier(J, tableau);
    for (int n = 0; n < J[tableau].taille; n++, m = json_suivant(J, m)) {
        if (J[m].type != JSON_OBJET) continue;
        if (r->nb_minuteurs >= RECETTE_MAX_MINUTEURS) {
            r->tronquee = true;
            return;
        }
        DefMinuteur *d = &r->minuteurs[r->nb_minuteurs];
        int etape = -1, secondes = 0, secondes_max = 0;
        json_entier(T, J, json_cle(T, J, m, "etape"), &etape);
        json_entier(T, J, json_cle(T, J, m, "secondes"), &secondes);
        json_entier(T, J, json_cle(T, J, m, "secondes_max"), &secondes_max);
        /* Minuteur incohérent (étape inexistante, durée nulle) : ignoré. */
        if (etape < 0 || etape >= r->nb_etapes || secondes <= 0) continue;
        d->etape = etape;
        d->secondes = secondes;
        d->secondes_max = secondes_max > secondes ? secondes_max : 0;
        texte_ou_vide(json_cle(T, J, m, "libelle"), d->libelle, TAILLE_LIBELLE);
        r->nb_minuteurs++;
    }
}

static void lire_notes(int tableau, Recette *r)
{
    if (tableau < 0 || J[tableau].type != JSON_TABLEAU) return;
    int n = json_premier(J, tableau);
    for (int k = 0; k < J[tableau].taille; k++, n = json_suivant(J, n)) {
        if (r->nb_notes >= RECETTE_MAX_NOTES) {
            r->tronquee = true;
            return;
        }
        if (json_texte(T, J, n, r->notes[r->nb_notes], TAILLE_NOTE) && r->notes[r->nb_notes][0])
            r->nb_notes++;
    }
}

bool recette_depuis_json(const char *texte, int longueur, Recette *r,
                         char *erreur, int taille_erreur)
{
    memset(r, 0, sizeof(*r));
    int position = 0;
    int nb = json_analyser(texte, longueur, jetons, MAX_JETONS, &position);
    if (nb < 0) {
        snprintf(erreur, (size_t)taille_erreur, "%s (octet %d)", json_message(nb), position);
        return false;
    }
    if (J[0].type != JSON_OBJET) {
        snprintf(erreur, (size_t)taille_erreur, "le fichier ne contient pas une recette");
        return false;
    }
    T = texte;

    int version = 1;
    json_entier(T, J, json_cle(T, J, 0, "v"), &version);
    if (version > RECETTE_JSON_VERSION) {
        snprintf(erreur, (size_t)taille_erreur,
                 "format v%d trop récent : mettre l'appli à jour", version);
        return false;
    }

    texte_ou_vide(json_cle(T, J, 0, "id"), r->id, TAILLE_ID);
    texte_ou_vide(json_cle(T, J, 0, "titre"), r->titre, TAILLE_TITRE);
    texte_ou_vide(json_cle(T, J, 0, "rev"), r->rev, sizeof(r->rev));
    if (!r->id[0] || !r->titre[0]) {
        snprintf(erreur, (size_t)taille_erreur, "id ou titre manquant");
        return false;
    }

    int portions = json_cle(T, J, 0, "portions");
    if (portions >= 0 && J[portions].type == JSON_OBJET) {
        int nombre = 0;
        if (json_entier(T, J, json_cle(T, J, portions, "nombre"), &nombre) && nombre > 0) {
            r->a_portions = true;
            r->portions_nombre = nombre;
            texte_ou_vide(json_cle(T, J, portions, "unite"), r->portions_unite, TAILLE_UNITE);
            if (!r->portions_unite[0]) copier_texte(r->portions_unite, "portions", TAILLE_UNITE);
        }
    }
    json_booleen(J, json_cle(T, J, 0, "congelable"), &r->congelable);
    json_booleen(J, json_cle(T, J, 0, "image"), &r->image);

    lire_groupes(json_cle(T, J, 0, "groupes"), r);
    lire_etapes(json_cle(T, J, 0, "etapes"), r);
    lire_minuteurs(json_cle(T, J, 0, "minuteurs"), r);   /* après les étapes */
    lire_notes(json_cle(T, J, 0, "notes"), r);

    if (r->nb_etapes == 0) {
        snprintf(erreur, (size_t)taille_erreur, "la recette n'a aucune étape");
        return false;
    }
    erreur[0] = '\0';
    return true;
}
