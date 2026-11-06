/* Tests de l'analyseur JSON. */
#include <stdlib.h>

#include "json.h"
#include "verif.h"

#define MAX 256
static JsonJeton j[MAX];

static int analyser(const char *s)
{
    return json_analyser(s, (int)strlen(s), j, MAX, NULL);
}

static const char *texte_de(const char *s, int indice)
{
    static char sortie[128];
    json_texte(s, j, indice, sortie, sizeof(sortie));
    return sortie;
}

int main(void)
{
    /* --- Documents valides --- */
    const char *doc = " { \"a\" : 1, \"b\": [true, false, null, -2.5e1], \"c\": {\"d\": \"x\"}, \"e\": [] } ";
    VERIF_NOMBRE(analyser(doc), 15);
    VERIF_NOMBRE(j[0].type, JSON_OBJET);
    VERIF_NOMBRE(j[0].taille, 4);
    VERIF_NOMBRE(j[0].suivant, 15);

    int b = json_cle(doc, j, 0, "b");
    VERIF_NOMBRE(j[b].type, JSON_TABLEAU);
    VERIF_NOMBRE(j[b].taille, 4);
    bool vrai = false;
    int e = json_premier(j, b);
    VERIF_NOMBRE(json_booleen(j, e, &vrai) && vrai, 1);
    e = json_suivant(j, e);
    e = json_suivant(j, e);
    VERIF_NOMBRE(json_est_nul(j, e), 1);
    double v = 0;
    VERIF_NOMBRE(json_nombre(doc, j, json_suivant(j, e), &v), 1);
    VERIF_NOMBRE(v, -25);

    /* La clé « c » contient un sous-objet : json_cle doit le sauter pour trouver « e ». */
    int c = json_cle(doc, j, 0, "c");
    VERIF_TEXTE(texte_de(doc, json_cle(doc, j, c, "d")), "x");
    int vide = json_cle(doc, j, 0, "e");
    VERIF_NOMBRE(j[vide].taille, 0);
    VERIF_NOMBRE(json_premier(j, vide), -1);
    VERIF_NOMBRE(json_cle(doc, j, 0, "absente"), -1);
    VERIF_NOMBRE(json_cle(doc, j, b, "a"), -1);   /* pas un objet */

    int entier = 0;
    VERIF_NOMBRE(json_entier(doc, j, json_cle(doc, j, 0, "a"), &entier) && entier == 1, 1);
    VERIF_NOMBRE(json_entier(doc, j, json_premier(j, b), &entier), 0);   /* true n'est pas un nombre */

    /* --- Textes : UTF-8 brut, échappements, \u, paires de substitution --- */
    const char *s = "[\"Œufs à 200°C\", \"a\\\"b\\\\c\\/d\\ne\", \"\\u00e9t\\u00E9\", \"\\ud83d\\ude00!\", \"\\udc00\"]";
    VERIF_NOMBRE(analyser(s), 6);
    VERIF_TEXTE(texte_de(s, 1), "Œufs à 200°C");
    VERIF_TEXTE(texte_de(s, 2), "a\"b\\c/d\ne");
    VERIF_TEXTE(texte_de(s, 3), "été");
    VERIF_TEXTE(texte_de(s, 4), "\xF0\x9F\x98\x80!");
    VERIF_TEXTE(texte_de(s, 5), "\xEF\xBF\xBD");          /* demi-paire isolée → U+FFFD */

    /* Troncature : jamais au milieu d'un caractère. */
    char petit[5];
    json_texte(s, j, 1, petit, sizeof(petit));            /* « Œufs » : Œ fait 2 octets */
    VERIF_TEXTE(petit, "Œuf");
    json_texte(s, j, 3, petit, 3);                        /* « é » (2 octets) tient, pas « t » */
    VERIF_TEXTE(petit, "é");
    VERIF_NOMBRE(json_texte(s, j, 0, petit, sizeof(petit)), 0);   /* le tableau n'est pas un texte */

    /* --- Nombres --- */
    const char *nombres[] = {"0", "-0", "12", "1.5", "1e3", "1E-2", "-0.25"};
    for (int i = 0; i < 7; i++) VERIF_NOMBRE(analyser(nombres[i]) == 1, 1);

    /* --- Documents invalides --- */
    const char *invalides[] = {
        "", "   ", "{", "[1,]", "{\"a\":1,}", "{\"a\" 1}", "{a:1}", "[1 2]", "01", "1.",
        ".5", "-", "1e", "+1", "tru", "nul", "\"abc", "\"a\\x\"", "\"\\u12G4\"",
        "\"tab\tbrute\"", "[1]x", "{\"a\":}", "'a'", "[\"a\",]",
    };
    int nb_invalides = (int)(sizeof(invalides) / sizeof(invalides[0]));
    for (int i = 0; i < nb_invalides; i++) {
        int r = analyser(invalides[i]);
        if (r != JSON_ERR_SYNTAXE) printf("  pour « %s » : %d\n", invalides[i], r);
        VERIF_NOMBRE(r, JSON_ERR_SYNTAXE);
    }

    /* Texte tronqué en plein milieu (fichier copié à moitié sur la SD). */
    VERIF_NOMBRE(json_analyser(doc, 30, j, MAX, NULL), JSON_ERR_SYNTAXE);

    /* Trop de jetons. */
    VERIF_NOMBRE(json_analyser("[1,2,3,4]", 9, j, 3, NULL), JSON_ERR_JETONS);

    /* Imbrication excessive : pas de débordement de pile. */
    char profond[200];
    memset(profond, '[', 100);
    memset(profond + 100, ']', 100);
    VERIF_NOMBRE(json_analyser(profond, 200, j, MAX, NULL), JSON_ERR_PROFONDEUR);
    VERIF_NOMBRE(json_analyser(profond + 100 - JSON_PROFONDEUR_MAX, 2 * JSON_PROFONDEUR_MAX,
                               j, MAX, NULL), JSON_PROFONDEUR_MAX);

    /* Octets au hasard : l'analyseur ne doit jamais planter (ASan surveille). */
    srand(42);
    char hasard[64];
    for (int essai = 0; essai < 20000; essai++) {
        int lg = rand() % 64;
        for (int k = 0; k < lg; k++) hasard[k] = "{}[]\":,0-1.eE\\u tfnrlsa\xC3\xA9"[rand() % 26];
        int n = json_analyser(hasard, lg, j, MAX, NULL);
        if (n > 0)
            for (int k = 0; k < n; k++) {
                char tampon[16];
                json_texte(hasard, j, k, tampon, sizeof(tampon));
            }
    }
    VERIF_NOMBRE(1, 1);

    return VERIF_BILAN("test_json");
}
