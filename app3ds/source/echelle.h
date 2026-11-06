/*
 * Mise à l'échelle des quantités : portage en C de
 * passerelle/recettes3ds/echelle.py. Les deux doivent donner les mêmes
 * résultats (voir tests/test_echelle.c, mêmes cas que tests/test_echelle.py).
 *
 * Portable : aucune dépendance à libctru.
 */
#ifndef ECHELLE_H
#define ECHELLE_H

#include <stddef.h>
#include "recette.h"

/* Multiplicateurs proposés quand la recette ne connaît pas ses portions. */
#define ECHELLE_NB_MULTIPLICATEURS 6
extern const double ECHELLE_MULTIPLICATEURS[ECHELLE_NB_MULTIPLICATEURS];
#define ECHELLE_INDICE_X1 1   /* indice de ×1 dans le tableau */

/* Coefficient : portions voulues / portions de base, ou le multiplicateur. */
double echelle_facteur(bool a_portions, int portions_base, double valeur);

double echelle_pas(double q, const char *unite);
double echelle_arrondir(double q, const char *unite);
/* q × f arrondi ; f == 1 rend q inchangé. */
double echelle_appliquer(double q, double f, const char *unite);

/* 1.5 → « 1,5 » (métrique) ou « 1 ½ » (cuillères, pièces…). */
void echelle_formater_nombre(double v, const char *unite, char *sortie, size_t taille);
/* « gousse » → « gousses » à partir de 2. */
void echelle_pluriel(const char *unite, double q, char *sortie, size_t taille);
/* Ligne affichée : « 440 g de poisson blanc ». */
void echelle_formater_ingredient(const Ingredient *i, double f, char *sortie, size_t taille);
/* « ×1,5 », « ×½ » : texte du multiplicateur. */
void echelle_formater_multiplicateur(double m, char *sortie, size_t taille);

#endif
