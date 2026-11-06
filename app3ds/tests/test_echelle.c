/*
 * Mêmes cas que passerelle/tests/test_echelle.py : le C et le Python
 * doivent arrondir et afficher de la même façon.
 */
#include "echelle.h"
#include "verif.h"

static Ingredient item(double q, double q2, const char *unite, const char *nom)
{
    Ingredient i = {0};
    i.a_quantite = q >= 0;
    i.quantite = q >= 0 ? q : 0;
    i.a_quantite_max = q2 > 0;
    i.quantite_max = q2 > 0 ? q2 : 0;
    copier_texte(i.unite, unite, TAILLE_UNITE);
    copier_texte(i.nom, nom, TAILLE_NOM);
    return i;
}

static const char *ingr(double q, double q2, const char *unite, const char *nom, double f)
{
    static char sortie[256];
    Ingredient i = item(q, q2, unite, nom);
    echelle_formater_ingredient(&i, f, sortie, sizeof(sortie));
    return sortie;
}

static const char *nombre(double v, const char *unite)
{
    static char sortie[32];
    echelle_formater_nombre(v, unite, sortie, sizeof(sortie));
    return sortie;
}

static const char *pluriel(const char *unite, double q)
{
    static char sortie[64];
    echelle_pluriel(unite, q, sortie, sizeof(sortie));
    return sortie;
}

int main(void)
{
    /* Facteur */
    VERIF_NOMBRE(echelle_facteur(true, 6, 9), 1.5);
    VERIF_NOMBRE(echelle_facteur(true, 6, 3), 0.5);
    VERIF_NOMBRE(echelle_facteur(false, 0, 2), 2);

    /* Arrondis */
    VERIF_NOMBRE(echelle_arrondir(3.3, "g"), 3.5);
    VERIF_NOMBRE(echelle_arrondir(33.4, "g"), 33);
    VERIF_NOMBRE(echelle_arrondir(67.5, "g"), 70);
    VERIF_NOMBRE(echelle_arrondir(66, "g"), 65);
    VERIF_NOMBRE(echelle_arrondir(333, "g"), 330);
    VERIF_NOMBRE(echelle_arrondir(335, "g"), 340);
    VERIF_NOMBRE(echelle_arrondir(0.33, "l"), 0.35);
    VERIF_NOMBRE(echelle_arrondir(1.26, "l"), 1.3);
    VERIF_NOMBRE(echelle_arrondir(12.4, "cl"), 12);
    VERIF_NOMBRE(echelle_arrondir(0.33, NULL), 0.25);
    VERIF_NOMBRE(echelle_arrondir(1.4, NULL), 1.5);
    VERIF_NOMBRE(echelle_arrondir(0.1, "c. à café"), 0.25);
    VERIF_NOMBRE(echelle_arrondir(13.4, NULL), 13);
    VERIF_NOMBRE(echelle_appliquer(135, 1, "g"), 135);
    VERIF_NOMBRE(echelle_appliquer(0.33, 1, NULL), 0.33);

    /* Nombres */
    VERIF_TEXTE(nombre(2, "g"), "2");
    VERIF_TEXTE(nombre(1.5, "l"), "1,5");
    VERIF_TEXTE(nombre(0.35, "l"), "0,35");
    VERIF_TEXTE(nombre(1.5, NULL), "1 ½");
    VERIF_TEXTE(nombre(0.25, "c. à café"), "¼");
    VERIF_TEXTE(nombre(2.75, NULL), "2 ¾");

    /* Pluriels */
    VERIF_TEXTE(pluriel("gousse", 1), "gousse");
    VERIF_TEXTE(pluriel("gousse", 1.5), "gousse");
    VERIF_TEXTE(pluriel("gousse", 2), "gousses");
    VERIF_TEXTE(pluriel("petit pot", 3), "petits pots");
    VERIF_TEXTE(pluriel("morceau", 2), "morceaux");
    VERIF_TEXTE(pluriel("bocal", 2), "bocaux");
    VERIF_TEXTE(pluriel("c. à soupe", 3), "c. à soupe");
    VERIF_TEXTE(pluriel("g", 200), "g");

    /* Ingrédients complets */
    VERIF_TEXTE(ingr(220, 0, "g", "de poisson blanc", 2), "440 g de poisson blanc");
    VERIF_TEXTE(ingr(0.5, 0, "", "oignon", 3), "1 ½ oignon");
    VERIF_TEXTE(ingr(1, 0, "gousse", "d'ail", 2), "2 gousses d'ail");
    VERIF_TEXTE(ingr(80, 100, "g", "de chocolat", 0.5), "40-50 g de chocolat");
    VERIF_TEXTE(ingr(-1, 0, "", "persil", 2), "persil");
    VERIF_TEXTE(ingr(135, 0, "g", "de farine", 1), "135 g de farine");

    /* Multiplicateurs */
    char m[16];
    echelle_formater_multiplicateur(0.5, m, sizeof(m));
    VERIF_TEXTE(m, "×½");
    echelle_formater_multiplicateur(1.5, m, sizeof(m));
    VERIF_TEXTE(m, "×1 ½");

    return VERIF_BILAN("test_echelle");
}
