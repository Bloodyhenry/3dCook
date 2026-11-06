/*
 * Lecture et écriture de fichiers sur la carte SD, sans risque de corruption.
 *
 * Écriture « atomique » : on écrit d'abord un fichier temporaire « .tmp »,
 * puis on le renomme. Une coupure (batterie, console éteinte) pendant
 * l'écriture laisse donc soit l'ancien fichier, soit le nouveau, jamais un
 * fichier à moitié écrit.
 *
 * Portable (stdio) : testé sur le Mac.
 */
#ifndef FICHIERS_H
#define FICHIERS_H

#include <stdbool.h>

#define FICHIER_ILLISIBLE  (-1)
#define FICHIER_TROP_GROS  (-2)

/* Lit un fichier entier ; renvoie sa taille ou un code négatif ci-dessus. */
int fichier_lire(const char *chemin, char *tampon, int max);

/* Écrit `taille` octets dans `chemin` de façon atomique. */
bool fichier_ecrire_atomique(const char *chemin, const char *donnees, int taille);

/* Crée le dossier et ses parents s'ils n'existent pas (« sdmc: » est ignoré). */
void dossier_creer(const char *chemin);

#endif
