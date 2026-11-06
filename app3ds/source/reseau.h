/*
 * Accès réseau de la console : HTTP simple (sans TLS) avec le service httpc
 * de libctru, et état du Wi-Fi avec le service ac.
 */
#ifndef RESEAU_H
#define RESEAU_H

#include <stdbool.h>

void reseau_init(void);
void reseau_fin(void);

/* La console est-elle connectée à un point d'accès Wi-Fi ? */
bool reseau_wifi_connecte(void);

/*
 * GET `url` dans `tampon`. Renvoie la taille reçue, ou -1 avec `erreur`
 * rempli. Même signature que FonctionTelecharger (synchro.h) ; `contexte`
 * n'est pas utilisé.
 */
int reseau_telecharger(void *contexte, const char *url, char *tampon, int max,
                       char *erreur, int taille_erreur);

#endif
