/* Tests du découpage en lignes, avec une police factice de 10 px par caractère. */
#include "mise_en_page.h"
#include "recette.h"
#include "verif.h"

/* Largeur factice : 10 px × nombre de caractères UTF-8 × échelle. */
static float mesure(void *contexte, const char *texte, float echelle)
{
    (void)contexte;
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)texte; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n * 10.0f * echelle;
}

int main(void)
{
    Paragraphe p;

    /* 100 px à l'échelle 1 = 10 caractères par ligne. */
    VERIF_NOMBRE(mep_couper("Mixer le poisson avec l'oignon", 100, 1, mesure, NULL, &p), 4);
    VERIF_TEXTE(p.lignes[0], "Mixer le");
    VERIF_TEXTE(p.lignes[1], "poisson");
    VERIF_TEXTE(p.lignes[2], "avec");
    VERIF_TEXTE(p.lignes[3], "l'oignon");

    /* Accents : un « é » compte pour un caractère, pas deux octets. */
    VERIF_NOMBRE(mep_couper("échalote émincée", 160, 1, mesure, NULL, &p), 1);

    /* Mot plus long que la largeur : seul sur sa ligne, sans boucle infinie. */
    VERIF_NOMBRE(mep_couper("a anticonstitutionnellement b", 100, 1, mesure, NULL, &p), 3);
    VERIF_TEXTE(p.lignes[1], "anticonstitutionnellement");

    /* Espaces multiples et texte vide. */
    VERIF_NOMBRE(mep_couper("  un   deux  ", 100, 1, mesure, NULL, &p), 1);
    VERIF_TEXTE(p.lignes[0], "un deux");
    VERIF_NOMBRE(mep_couper("", 100, 1, mesure, NULL, &p), 0);

    /* Ajustement : grande police si ça tient, sinon plus petite. */
    const float echelles[] = {1.0f, 0.5f};
    mep_ajuster("court", 100, 60, 30, echelles, 2, mesure, NULL, &p);
    VERIF_NOMBRE(p.echelle, 1.0);
    /* 6 lignes de 30 px à l'échelle 1 > 60 px ; à 0,5 : 3 lignes × 15 px = 45 px. */
    mep_ajuster("Badigeonner les acras d'huile puis les cuire", 100, 60, 30,
                echelles, 2, mesure, NULL, &p);
    VERIF_NOMBRE(p.echelle, 0.5);
    VERIF_NOMBRE(p.nb_lignes, 3);

    /* La plus longue étape de la recette d'exemple tient à petite échelle. */
    static Recette r;
    recette_demo(&r);
    const float echelles_haut[] = {1.0f, 0.85f, 0.72f, 0.62f, 0.55f};
    mep_ajuster(r.etapes[5].texte, 380, 180, 30, echelles_haut, 5, mesure, NULL, &p);
    VERIF_NOMBRE(p.nb_lignes * 30 * p.echelle <= 180, 1);

    /* Troncature d'un titre trop long (10 px par caractère). */
    char court[64];
    mep_tronquer("Acras de poisson", 200, 1, mesure, NULL, court, sizeof(court));
    VERIF_TEXTE(court, "Acras de poisson");                  /* 16 car. : tient */
    mep_tronquer("Acras de poisson", 100, 1, mesure, NULL, court, sizeof(court));
    VERIF_TEXTE(court, "Acras d...");                        /* 10 car. = 100 px */
    mep_tronquer("Crème brûlée", 90, 1, mesure, NULL, court, sizeof(court));
    VERIF_TEXTE(court, "Crème...");                          /* espace final retiré */
    mep_tronquer("Éé", 10, 1, mesure, NULL, court, sizeof(court));
    VERIF_TEXTE(court, "...");
    mep_tronquer("abcdefghij", 1000, 1, mesure, NULL, court, 6);   /* tampon de sortie petit */
    VERIF_TEXTE(court, "ab...");

    /* copier_texte ne coupe jamais un caractère UTF-8 en deux. */
    char petit[4];
    copier_texte(petit, "aéb", 3);   /* « é » fait 2 octets : seul « a » tient */
    VERIF_TEXTE(petit, "a");

    return VERIF_BILAN("test_mise_en_page");
}
