/*
 * Recette d'exemple codée en dur (étape 1 du projet).
 * Elle reprend « Acras de poisson » telle que la passerelle la convertit,
 * avec en plus deux groupes, une section et une étape longue pour tester
 * la mise en page.
 */
#include "recette.h"

#include <string.h>

void copier_texte(char *dest, const char *src, int taille)
{
    if (taille <= 0) return;
    int n = (int)strlen(src);
    if (n >= taille) {
        n = taille - 1;
        /* Ne pas couper au milieu d'un caractère UTF-8 (octets 10xxxxxx). */
        while (n > 0 && ((unsigned char)src[n] & 0xC0) == 0x80) n--;
    }
    memcpy(dest, src, (size_t)n);
    dest[n] = '\0';
}

static int nouveau_groupe(Recette *r, const char *titre)
{
    if (r->nb_groupes >= RECETTE_MAX_GROUPES) return r->nb_groupes - 1;
    copier_texte(r->titres_groupes[r->nb_groupes], titre, TAILLE_SECTION);
    return r->nb_groupes++;
}

/* quantite < 0 : pas de quantité ; quantite_max <= 0 : pas de plage. */
static void ingredient(Recette *r, int groupe, double quantite, double quantite_max,
                       const char *unite, const char *nom, bool allergene)
{
    if (r->nb_ingredients >= RECETTE_MAX_INGREDIENTS) return;
    Ingredient *i = &r->ingredients[r->nb_ingredients++];
    i->a_quantite = quantite >= 0;
    i->quantite = quantite >= 0 ? quantite : 0;
    i->a_quantite_max = quantite_max > 0;
    i->quantite_max = quantite_max > 0 ? quantite_max : 0;
    copier_texte(i->unite, unite, TAILLE_UNITE);
    copier_texte(i->nom, nom, TAILLE_NOM);
    i->allergene = allergene;
    i->groupe = groupe;
}

static int etape(Recette *r, const char *section, const char *texte)
{
    if (r->nb_etapes >= RECETTE_MAX_ETAPES) return r->nb_etapes - 1;
    Etape *e = &r->etapes[r->nb_etapes];
    copier_texte(e->section, section, TAILLE_SECTION);
    copier_texte(e->texte, texte, TAILLE_ETAPE);
    return r->nb_etapes++;
}

static void minuteur(Recette *r, int indice_etape, int secondes, int secondes_max,
                     const char *libelle)
{
    if (r->nb_minuteurs >= RECETTE_MAX_MINUTEURS) return;
    DefMinuteur *m = &r->minuteurs[r->nb_minuteurs++];
    m->etape = indice_etape;
    m->secondes = secondes;
    m->secondes_max = secondes_max;
    copier_texte(m->libelle, libelle, TAILLE_LIBELLE);
}

void recette_demo(Recette *r)
{
    memset(r, 0, sizeof(*r));
    copier_texte(r->id, "exemple", TAILLE_ID);   /* ne se confond avec aucune recette du menu */
    /* Titre distinct de la vraie recette « Acras de poisson » du menu. */
    copier_texte(r->titre, "Recette d'exemple (test des minuteurs)", TAILLE_TITRE);
    copier_texte(r->rev, "demo", sizeof(r->rev));
    r->a_portions = true;
    r->portions_nombre = 15;
    copier_texte(r->portions_unite, "acras", TAILLE_UNITE);
    r->congelable = true;

    int pate = nouveau_groupe(r, "");
    ingredient(r, pate, 220, 0, "g", "de poisson blanc", true);
    ingredient(r, pate, 0.5, 0, "", "oignon", false);
    ingredient(r, pate, 0.5, 0, "", "échalote", false);
    ingredient(r, pate, 135, 0, "g", "de farine de blé T60", true);
    ingredient(r, pate, 20, 0, "g", "de farine de blé complète T110", true);
    ingredient(r, pate, 100, 0, "ml", "de lait d'amande", true);
    ingredient(r, pate, 1, 0, "c. à café", "de poudre à lever", false);
    ingredient(r, pate, -1, 0, "", "persil", false);
    ingredient(r, pate, -1, 0, "", "optionnel : épices", false);
    int cuisson = nouveau_groupe(r, "Pour la cuisson");
    ingredient(r, cuisson, 3, 0, "c. à soupe", "d'huile d'olive", false);
    ingredient(r, cuisson, 1, 2, "gousse", "d'ail", false);

    int e;
    e = etape(r, "", "Faire cuire le poisson à la vapeur 15-20min.");
    minuteur(r, e, 900, 1200, "Vapeur 15-20 min");
    e = etape(r, "", "Mixer le poisson avec l'oignon et l'échalote.");
    /* Minuteur court, pour essayer l'alarme sans attendre 15 minutes. */
    minuteur(r, e, 10, 0, "Test de l'alarme 10 s");
    etape(r, "", "Mélanger tous les ingrédients ensemble et mélanger à l'aide d'une fourchette.");
    etape(r, "Façonnage", "Déposer des petits tas dans un plat bien huilé.");
    etape(r, "", "Badigeonner les acras d'huile.");
    e = etape(r, "Cuisson",
              "Faire cuire à 200°C pendant 15-20min (en les retournant à mi cuisson). "
              "Surveiller la coloration : les acras doivent être bien dorés à l'extérieur "
              "et moelleux à l'intérieur. Si votre four chauffe fort, baissez à 180°C "
              "et prolongez de 5 minutes. Servir tiède avec une sauce au yaourt, "
              "du citron et quelques feuilles de coriandre ou de persil ciselé.");
    minuteur(r, e, 900, 1200, "Four 200°C 15-20 min");

    copier_texte(r->notes[r->nb_notes++], "Les acras se conservent 3 jours au réfrigérateur.",
                 TAILLE_NOTE);
}
