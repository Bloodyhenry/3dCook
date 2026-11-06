/*
 * Petit analyseur JSON sans allocation, dans l'esprit de jsmn.
 *
 * Le texte n'est parcouru qu'une fois et découpé en « jetons » rangés dans un
 * tableau fourni par l'appelant. Un jeton ne copie rien : il indique où
 * commence et où finit sa valeur dans le texte d'origine. Les jetons sont
 * dans l'ordre du document (un objet, puis sa première clé, sa première
 * valeur, etc.) et chacun sait où commence son voisin suivant, ce qui permet
 * de sauter un sous-arbre entier d'un coup.
 *
 * Le JSON est vérifié strictement (RFC 8259) ; la profondeur d'imbrication
 * est limitée pour ne pas épuiser la pile de la console.
 *
 * Portable : aucune dépendance à libctru.
 */
#ifndef JSON_H
#define JSON_H

#include <stdbool.h>

#define JSON_PROFONDEUR_MAX 32

typedef enum {
    JSON_OBJET,
    JSON_TABLEAU,
    JSON_TEXTE,      /* debut/fin : contenu entre les guillemets, encore échappé */
    JSON_NOMBRE,
    JSON_VRAI,
    JSON_FAUX,
    JSON_NUL,
} JsonType;

typedef struct {
    JsonType type;
    int debut, fin;     /* position dans le texte : [debut, fin[ */
    int taille;         /* objet : nombre de paires ; tableau : nombre d'éléments */
    int suivant;        /* indice du jeton qui suit tout ce sous-arbre */
} JsonJeton;

/* Codes d'erreur (valeurs négatives renvoyées par json_analyser). */
#define JSON_ERR_SYNTAXE    (-1)
#define JSON_ERR_JETONS     (-2)   /* tableau de jetons trop petit */
#define JSON_ERR_PROFONDEUR (-3)

/*
 * Analyse `texte` (longueur `longueur`, pas besoin de zéro final).
 * Renvoie le nombre de jetons, ou un code d'erreur négatif. En cas d'erreur,
 * *position (si non NULL) indique où l'analyse s'est arrêtée.
 */
int json_analyser(const char *texte, int longueur, JsonJeton *jetons, int max_jetons,
                  int *position);

/* Message lisible pour un code d'erreur. */
const char *json_message(int code);

/* --- Lecture des jetons (j : tableau de jetons, t : texte d'origine) ---------- */

/* Valeur associée à `cle` dans l'objet `objet`, ou -1. */
int json_cle(const char *t, const JsonJeton *j, int objet, const char *cle);

/* Premier enfant d'un objet (sa première clé) ou d'un tableau, ou -1. */
int json_premier(const JsonJeton *j, int conteneur);

/* Élément suivant d'un tableau (pour un objet : utiliser json_cle). */
int json_suivant(const JsonJeton *j, int element);

/*
 * Copie un texte JSON en UTF-8 (échappements décodés, \uXXXX compris) dans
 * `sortie`, sans couper un caractère. Renvoie false si ce n'est pas un texte.
 */
bool json_texte(const char *t, const JsonJeton *j, int indice, char *sortie, int taille);

bool json_nombre(const char *t, const JsonJeton *j, int indice, double *sortie);
bool json_entier(const char *t, const JsonJeton *j, int indice, int *sortie);
bool json_booleen(const JsonJeton *j, int indice, bool *sortie);
bool json_est_nul(const JsonJeton *j, int indice);

#endif
