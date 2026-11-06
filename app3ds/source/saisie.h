/*
 * Saisie de texte avec le clavier logiciel de la console (applet swkbd).
 */
#ifndef SAISIE_H
#define SAISIE_H

#include <stdbool.h>

/*
 * Demande l'adresse de la passerelle, pré-remplie avec `actuelle`.
 * Renvoie false si l'utilisateur annule.
 */
bool saisir_adresse(const char *actuelle, char *sortie, int taille);

#endif
