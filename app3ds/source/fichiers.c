/* Fichiers sur la carte SD (voir fichiers.h). */
#include "fichiers.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int fichier_lire(const char *chemin, char *tampon, int max)
{
    FILE *f = fopen(chemin, "rb");
    if (!f) return FICHIER_ILLISIBLE;
    int lu = (int)fread(tampon, 1, (size_t)max, f);
    bool reste = fgetc(f) != EOF;
    fclose(f);
    return reste ? FICHIER_TROP_GROS : lu;
}

bool fichier_ecrire_atomique(const char *chemin, const char *donnees, int taille)
{
    char temporaire[300];
    if (snprintf(temporaire, sizeof(temporaire), "%s.tmp", chemin) >= (int)sizeof(temporaire))
        return false;
    FILE *f = fopen(temporaire, "wb");
    if (!f) return false;
    bool ok = fwrite(donnees, 1, (size_t)taille, f) == (size_t)taille;
    ok = (fclose(f) == 0) && ok;
    if (!ok) {
        remove(temporaire);
        return false;
    }
    /* Sur la carte SD de la 3DS, renommer vers un fichier existant échoue :
     * on retire l'ancien juste avant. Si la console s'éteint entre les deux,
     * il reste le « .tmp » complet et l'ancien manque : la recette sera
     * simplement téléchargée de nouveau. */
    remove(chemin);
    if (rename(temporaire, chemin) != 0) {
        remove(temporaire);
        return false;
    }
    return true;
}

void dossier_creer(const char *chemin)
{
    char c[256];
    snprintf(c, sizeof(c), "%s", chemin);
    /* Crée chaque niveau : sdmc:/3ds, sdmc:/3ds/recettes, … */
    for (char *p = c + 1; ; p++) {
        if (*p == '/' || *p == '\0') {
            char fin = *p;
            *p = '\0';
            if (p[-1] != ':') mkdir(c, 0777);   /* « sdmc: » n'est pas un dossier */
            *p = fin;
            if (!fin) break;
        }
    }
}
