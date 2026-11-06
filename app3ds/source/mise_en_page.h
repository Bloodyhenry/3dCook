/*
 * Découpage d'un texte UTF-8 en lignes qui tiennent dans une largeur donnée,
 * et choix de la plus grande taille de police qui fait tenir le texte.
 *
 * La mesure de la largeur est fournie par l'appelant (citro2d sur la
 * console, une mesure factice dans les tests) : ce module est portable.
 */
#ifndef MISE_EN_PAGE_H
#define MISE_EN_PAGE_H

#define MEP_MAX_LIGNES  24
#define MEP_TAILLE_LIGNE 256

/* Largeur en pixels de `texte` à l'échelle `echelle`. */
typedef float (*FonctionMesure)(void *contexte, const char *texte, float echelle);

typedef struct {
    int   nb_lignes;
    float echelle;
    char  lignes[MEP_MAX_LIGNES][MEP_TAILLE_LIGNE];
} Paragraphe;

/*
 * Coupe `texte` aux espaces pour ne pas dépasser `largeur`. Un mot trop long
 * reste seul sur sa ligne. Renvoie le nombre de lignes, ou -1 si le texte
 * ne tient pas dans MEP_MAX_LIGNES lignes.
 */
int mep_couper(const char *texte, float largeur, float echelle,
               FonctionMesure mesure, void *contexte, Paragraphe *sortie);

/*
 * Essaie les échelles de `echelles` (de la plus grande à la plus petite) et
 * garde la première où le texte tient en hauteur :
 * nb_lignes × hauteur_ligne × échelle ≤ hauteur.
 * À défaut, garde la plus petite (le texte déborde alors en bas).
 */
void mep_ajuster(const char *texte, float largeur, float hauteur, float hauteur_ligne,
                 const float *echelles, int nb_echelles,
                 FonctionMesure mesure, void *contexte, Paragraphe *sortie);

/*
 * Copie `texte` dans `sortie` en le raccourcissant si besoin (avec « ... »)
 * pour qu'il tienne sur une ligne de `largeur` pixels. Ne coupe jamais un
 * caractère UTF-8.
 */
void mep_tronquer(const char *texte, float largeur, float echelle,
                  FonctionMesure mesure, void *contexte, char *sortie, int taille);

#endif
