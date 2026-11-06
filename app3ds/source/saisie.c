/* Clavier logiciel de la console (voir saisie.h). */
#include "saisie.h"

#include <3ds.h>

bool saisir_adresse(const char *actuelle, char *sortie, int taille)
{
    SwkbdState clavier;
    swkbdInit(&clavier, SWKBD_TYPE_NORMAL, 2, taille - 1);   /* boutons Annuler / OK */
    swkbdSetHintText(&clavier, "Adresse de la passerelle, ex. 192.168.1.10:8383");
    swkbdSetInitialText(&clavier, actuelle);
    swkbdSetValidation(&clavier, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    SwkbdButton bouton = swkbdInputText(&clavier, sortie, (size_t)taille);
    return bouton == SWKBD_BUTTON_CONFIRM;
}
