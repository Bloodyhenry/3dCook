/* Affichage de la photo d'une recette (voir photo_ecran.h). */
#include "photo_ecran.h"

#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <string.h>

#include "dessin.h"
#include "fichiers.h"
#include "photo.h"
#include "recette.h"

static uint8_t contenu[PHOTO_TAILLE_MAX + 16];   /* fichier lu sur la carte */
static C3D_Tex texture;
static bool texture_prete;
static Tex3DS_SubTexture partie;                 /* zone de la texture occupée par la photo */
static char chargee[TAILLE_ID];                  /* recette dont la photo est chargée */
static bool a_photo;

bool photo_ecran_charger(const char *dossier, const char *id)
{
    if (strcmp(id, chargee) == 0) return a_photo;
    copier_texte(chargee, id, sizeof(chargee));
    a_photo = false;

    char chemin[300];
    snprintf(chemin, sizeof(chemin), "%s/%s%s", dossier, id, PHOTO_EXTENSION);
    int lg = fichier_lire(chemin, (char *)contenu, sizeof(contenu));
    int largeur, hauteur;
    if (lg < 0 || !photo_valider(contenu, lg, &largeur, &hauteur)) return false;

    if (!texture_prete) {
        if (!C3D_TexInit(&texture, PHOTO_TEX_L, PHOTO_TEX_H, GPU_RGB565)) return false;
        C3D_TexSetFilter(&texture, GPU_LINEAR, GPU_LINEAR);
        texture_prete = true;
    }
    photo_tuiler(contenu + PHOTO_ENTETE, largeur, hauteur, (uint16_t *)texture.data);
    C3D_TexFlush(&texture);   /* le GPU doit voir les nouveaux pixels */

    /* Coordonnées de texture : haut = 1, la photo occupe le coin haut gauche. */
    partie = (Tex3DS_SubTexture){(u16)largeur, (u16)hauteur, 0.0f, 1.0f,
                                 (float)largeur / PHOTO_TEX_L,
                                 1.0f - (float)hauteur / PHOTO_TEX_H};
    a_photo = true;
    return true;
}

void photo_ecran_oublier(void)
{
    chargee[0] = '\0';
    a_photo = false;
}

void photo_ecran_dessiner(void)
{
    if (!a_photo) return;
    C2D_Image image = {&texture, &partie};
    float x = (HAUT_LARGEUR - partie.width) / 2.0f;
    float y = (240 - partie.height) / 2.0f;
    C2D_DrawImageAt(image, x, y, PROFONDEUR, NULL, 1.0f, 1.0f);
}

void photo_ecran_fin(void)
{
    if (texture_prete) C3D_TexDelete(&texture);
    texture_prete = false;
    photo_ecran_oublier();
}
