/*
 * Lecture d'une recette au format JSON de la passerelle (docs/schema.md,
 * version 1) vers la structure Recette.
 *
 * - Champs obligatoires : id, titre, etapes (au moins une).
 * - Champs inconnus : ignorés (la passerelle peut en ajouter sans casser l'appli).
 * - Version (« v ») supérieure à 1 : refusée, l'appli est trop ancienne.
 * - Dépassement d'une limite de recette.h : la suite est ignorée et
 *   Recette.tronquee passe à true.
 *
 * Portable : aucune dépendance à libctru.
 */
#ifndef RECETTE_JSON_H
#define RECETTE_JSON_H

#include "recette.h"

#define RECETTE_JSON_VERSION 1

/* Renvoie true si la recette est lisible ; sinon `erreur` explique pourquoi. */
bool recette_depuis_json(const char *texte, int longueur, Recette *r,
                         char *erreur, int taille_erreur);

#endif
