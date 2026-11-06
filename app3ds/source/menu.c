/* Menu de la semaine (voir menu.h). */
#include "menu.h"

#include <stdio.h>
#include <string.h>

#include "fichiers.h"
#include "json.h"

#define MAX_JETONS 2048
static JsonJeton jetons[MAX_JETONS];
static char contenu[32 * 1024];

bool identifiant_valide(const char *id)
{
    int n = 0;
    for (; id[n]; n++) {
        char c = id[n];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                  || c == '_' || c == '-';
        if (!ok || n >= 32) return false;
    }
    return n > 0;
}

bool menu_depuis_json(const char *t, int longueur, Menu *m, char *erreur, int taille_erreur)
{
    memset(m, 0, sizeof(*m));
    int position = 0;
    int nb = json_analyser(t, longueur, jetons, MAX_JETONS, &position);
    if (nb < 0) {
        snprintf(erreur, (size_t)taille_erreur, "menu : %s (octet %d)", json_message(nb), position);
        return false;
    }
    const JsonJeton *j = jetons;
    int recettes = json_cle(t, j, 0, "recettes");
    if (j[0].type != JSON_OBJET || recettes < 0 || j[recettes].type != JSON_TABLEAU) {
        snprintf(erreur, (size_t)taille_erreur, "réponse inattendue : est-ce bien la passerelle ?");
        return false;
    }
    double maj = 0;
    if (json_nombre(t, j, json_cle(t, j, 0, "maj"), &maj)) m->maj = (long)maj;

    int e = json_premier(j, recettes);
    for (int k = 0; k < j[recettes].taille; k++, e = json_suivant(j, e)) {
        if (j[e].type != JSON_OBJET) continue;
        if (m->nb >= MENU_MAX) {
            m->ignorees++;
            continue;
        }
        EntreeMenu *x = &m->entrees[m->nb];
        memset(x, 0, sizeof(*x));
        json_texte(t, j, json_cle(t, j, e, "id"), x->id, sizeof(x->id));
        if (!identifiant_valide(x->id)) {
            m->ignorees++;
            continue;
        }
        json_texte(t, j, json_cle(t, j, e, "titre"), x->titre, sizeof(x->titre));
        json_texte(t, j, json_cle(t, j, e, "rev"), x->rev, sizeof(x->rev));
        json_booleen(j, json_cle(t, j, e, "image"), &x->image);
        json_texte(t, j, json_cle(t, j, e, "date"), x->date, sizeof(x->date));
        json_texte(t, j, json_cle(t, j, e, "repas"), x->repas, sizeof(x->repas));
        if (!json_nombre(t, j, json_cle(t, j, e, "echelle"), &x->echelle) || x->echelle <= 0)
            x->echelle = 1;
        m->nb++;
    }
    erreur[0] = '\0';
    return true;
}

bool menu_lire(const char *chemin, Menu *m)
{
    char erreur[128];
    int lg = fichier_lire(chemin, contenu, sizeof(contenu));
    if (lg < 0) {
        memset(m, 0, sizeof(*m));
        return false;
    }
    return menu_depuis_json(contenu, lg, m, erreur, sizeof(erreur));
}

/* Jour de la semaine (0 = dimanche), méthode de Sakamoto. */
static int jour_semaine(int a, int m, int j)
{
    static const int decalage[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) a -= 1;
    return (a + a / 4 - a / 100 + a / 400 + decalage[m - 1] + j) % 7;
}

void menu_libelle_jour(const char *date, char *sortie, int taille)
{
    static const char *const JOURS[] = {"Dimanche", "Lundi", "Mardi", "Mercredi",
                                        "Jeudi", "Vendredi", "Samedi"};
    int a, m, j;
    sortie[0] = '\0';
    if (strlen(date) != 10 || sscanf(date, "%4d-%2d-%2d", &a, &m, &j) != 3) return;
    if (a < 2000 || m < 1 || m > 12 || j < 1 || j > 31) return;
    snprintf(sortie, (size_t)taille, "%s %d", JOURS[jour_semaine(a, m, j)], j);
}
