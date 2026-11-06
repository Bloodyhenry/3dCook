/*
 * Découpage en lignes (voir mise_en_page.h).
 * Les mesures ne servent qu'au changement d'étape ou de portions,
 * jamais à chaque image : le coût n'a pas d'importance.
 */
#include "mise_en_page.h"

#include <string.h>

int mep_couper(const char *texte, float largeur, float echelle,
               FonctionMesure mesure, void *contexte, Paragraphe *sortie)
{
    sortie->nb_lignes = 0;
    sortie->echelle = echelle;

    char courante[MEP_TAILLE_LIGNE] = "";
    char essai[MEP_TAILLE_LIGNE];
    const char *p = texte;

    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char *fin = p;
        while (*fin && *fin != ' ') fin++;
        int lg = (int)(fin - p);
        if (lg >= MEP_TAILLE_LIGNE - 1) lg = MEP_TAILLE_LIGNE - 2;

        /* Ligne + espace + mot. */
        int lc = (int)strlen(courante);
        if (lc && lc + 1 + lg < MEP_TAILLE_LIGNE) {
            memcpy(essai, courante, (size_t)lc);
            essai[lc] = ' ';
            memcpy(essai + lc + 1, p, (size_t)lg);
            essai[lc + 1 + lg] = '\0';
        } else {
            memcpy(essai, p, (size_t)lg);
            essai[lg] = '\0';
        }

        if (lc == 0 || (lc + 1 + lg < MEP_TAILLE_LIGNE
                        && mesure(contexte, essai, echelle) <= largeur)) {
            strcpy(courante, essai);
        } else {
            /* La ligne est pleine : on la range et le mot commence la suivante. */
            if (sortie->nb_lignes >= MEP_MAX_LIGNES) return -1;
            strcpy(sortie->lignes[sortie->nb_lignes++], courante);
            memcpy(courante, p, (size_t)lg);
            courante[lg] = '\0';
        }
        p = fin;
    }
    if (courante[0]) {
        if (sortie->nb_lignes >= MEP_MAX_LIGNES) return -1;
        strcpy(sortie->lignes[sortie->nb_lignes++], courante);
    }
    return sortie->nb_lignes;
}

void mep_tronquer(const char *texte, float largeur, float echelle,
                  FonctionMesure mesure, void *contexte, char *sortie, int taille)
{
    if (taille <= 0) return;
    int n = (int)strlen(texte);
    if (n < taille && mesure(contexte, texte, echelle) <= largeur) {
        memcpy(sortie, texte, (size_t)n + 1);
        return;
    }
    if (taille < 4) {
        sortie[0] = '\0';
        return;
    }
    if (n > taille - 4) n = taille - 4;          /* place pour « ... » et le zéro */
    for (;;) {
        /* Jamais au milieu d'un caractère UTF-8 (octets 10xxxxxx). */
        while (n > 0 && ((unsigned char)texte[n] & 0xC0) == 0x80) n--;
        int fin = n;
        while (fin > 0 && texte[fin - 1] == ' ') fin--;   /* pas d'espace avant « ... » */
        memcpy(sortie, texte, (size_t)fin);
        memcpy(sortie + fin, "...", 4);
        if (n == 0 || mesure(contexte, sortie, echelle) <= largeur) return;
        n--;
    }
}

void mep_ajuster(const char *texte, float largeur, float hauteur, float hauteur_ligne,
                 const float *echelles, int nb_echelles,
                 FonctionMesure mesure, void *contexte, Paragraphe *sortie)
{
    for (int i = 0; i < nb_echelles; i++) {
        int n = mep_couper(texte, largeur, echelles[i], mesure, contexte, sortie);
        if (n >= 0 && n * hauteur_ligne * echelles[i] <= hauteur) return;
    }
    /* Rien ne tient : plus petite échelle, quitte à déborder. */
    if (mep_couper(texte, largeur, echelles[nb_echelles - 1], mesure, contexte, sortie) < 0)
        sortie->nb_lignes = MEP_MAX_LIGNES;
}
