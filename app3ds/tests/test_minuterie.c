/* Tests de la minuterie, avec une horloge factice en millisecondes. */
#include "minuterie.h"
#include "verif.h"

int main(void)
{
    static Minuterie t;
    minuterie_init(&t);
    uint64_t h = 1000000;   /* « maintenant » */
    char texte[32];

    /* Formatage */
    minuterie_formater(754, texte, sizeof(texte));
    VERIF_TEXTE(texte, "12:34");
    minuterie_formater(5, texte, sizeof(texte));
    VERIF_TEXTE(texte, "0:05");
    minuterie_formater(3723, texte, sizeof(texte));
    VERIF_TEXTE(texte, "1:02:03");
    minuterie_formater(-4, texte, sizeof(texte));
    VERIF_TEXTE(texte, "0:00");

    /* Lancer, décompte arrondi au-dessus */
    VERIF_NOMBRE(minuterie_active(&t), 0);
    int i = minuterie_lancer(&t, "416", 0, "Vapeur 15-20 min", 900, 1200, h);
    VERIF_NOMBRE(i >= 0, 1);
    VERIF_NOMBRE(minuterie_active(&t), 1);
    VERIF_NOMBRE(minuterie_restant(&t.m[i], h), 900);
    VERIF_NOMBRE(minuterie_restant(&t.m[i], h + 1), 900);       /* 899,999 s → 900 */
    VERIF_NOMBRE(minuterie_restant(&t.m[i], h + 1000), 899);
    VERIF_NOMBRE(t.m[i].secondes_max, 1200);
    /* Relancer le même minuteur ne le duplique pas. */
    VERIF_NOMBRE(minuterie_lancer(&t, "416", 0, "x", 60, 0, h + 5000), i);
    VERIF_NOMBRE(minuterie_restant(&t.m[i], h + 5000), 895);
    /* Durée invalide refusée. */
    VERIF_NOMBRE(minuterie_lancer(&t, "416", 9, "x", 0, 0, h), -1);

    /* Un second minuteur, plus court, d'une autre recette. */
    int j = minuterie_lancer(&t, "388", 1, "Four 180°C 10 min", 600, 0, h);
    VERIF_NOMBRE(j >= 0 && j != i, 1);
    VERIF_NOMBRE(minuterie_trouver(&t, "388", 1), j);
    VERIF_NOMBRE(minuterie_trouver(&t, "388", 0), -1);
    int ordre[MINUTERIE_MAX];
    VERIF_NOMBRE(minuterie_ordre(&t, ordre, MINUTERIE_MAX), 2);
    VERIF_NOMBRE(ordre[0], j);                                   /* échéance la plus proche */

    /* Sonnerie */
    VERIF_NOMBRE(minuterie_maj(&t, h + 599999), 0);
    VERIF_NOMBRE(minuterie_maj(&t, h + 600000), 1);
    VERIF_NOMBRE(minuterie_maj(&t, h + 600500), 0);              /* pas deux fois */
    VERIF_NOMBRE(minuterie_sonne(&t), 1);
    VERIF_NOMBRE(minuterie_restant(&t.m[j], h + 700000), 0);
    minuterie_ordre(&t, ordre, MINUTERIE_MAX);
    VERIF_NOMBRE(ordre[0], j);                                   /* qui sonne en premier */

    /* Toucher : un minuteur qui sonne s'arrête tout de suite. */
    VERIF_NOMBRE(minuterie_toucher(&t, "388", 1, "", 600, 0, h + 601000), TOUCHER_ARRETE);
    VERIF_NOMBRE(minuterie_sonne(&t), 0);
    VERIF_NOMBRE(minuterie_trouver(&t, "388", 1), -1);

    /* Toucher : un minuteur qui tourne demande confirmation (3 s). */
    VERIF_NOMBRE(minuterie_toucher(&t, "416", 0, "", 900, 0, h + 10000), TOUCHER_CONFIRMER);
    VERIF_NOMBRE(minuterie_toucher(&t, "416", 0, "", 900, 0, h + 14000), TOUCHER_CONFIRMER);  /* trop tard */
    VERIF_NOMBRE(minuterie_toucher(&t, "416", 0, "", 900, 0, h + 15000), TOUCHER_ARRETE);
    VERIF_NOMBRE(minuterie_active(&t), 0);

    /* Toucher : pas lancé → lancé. */
    VERIF_NOMBRE(minuterie_toucher(&t, "416", 2, "Repos 5 min", 300, 0, h), TOUCHER_LANCE);
    VERIF_TEXTE(t.m[minuterie_trouver(&t, "416", 2)].libelle, "Repos 5 min");

    /* Plein : au-delà de MINUTERIE_MAX minuteurs. */
    for (int k = 0; k < MINUTERIE_MAX; k++) minuterie_lancer(&t, "x", k, "m", 60 + k, 0, h);
    VERIF_NOMBRE(minuterie_toucher(&t, "y", 0, "m", 60, 0, h), TOUCHER_PLEIN);

    /* Arrêter toutes les sonneries d'un coup. */
    minuterie_maj(&t, h + 3600000);
    VERIF_NOMBRE(minuterie_arreter_sonneries(&t), MINUTERIE_MAX);
    VERIF_NOMBRE(minuterie_active(&t), 0);

    return VERIF_BILAN("test_minuterie");
}
