/* Mini-cadre de tests : compte les échecs et affiche un bilan. */
#ifndef VERIF_H
#define VERIF_H

#include <math.h>
#include <stdio.h>
#include <string.h>

static int verif_total = 0, verif_echecs = 0;

#define VERIF_TEXTE(obtenu, attendu) do {                                     \
    verif_total++;                                                            \
    if (strcmp((obtenu), (attendu)) != 0) {                                   \
        verif_echecs++;                                                       \
        printf("  ÉCHEC %s:%d : « %s » au lieu de « %s »\n",                  \
               __FILE__, __LINE__, (obtenu), (attendu));                      \
    }                                                                         \
} while (0)

#define VERIF_NOMBRE(obtenu, attendu) do {                                    \
    verif_total++;                                                            \
    double o_ = (obtenu), a_ = (attendu);                                     \
    if (fabs(o_ - a_) > 1e-9) {                                               \
        verif_echecs++;                                                       \
        printf("  ÉCHEC %s:%d : %g au lieu de %g\n", __FILE__, __LINE__, o_, a_); \
    }                                                                         \
} while (0)

#define VERIF_BILAN(nom) (printf("%s : %d vérifications, %d échec(s)\n",       \
                                 (nom), verif_total, verif_echecs),           \
                          verif_echecs ? 1 : 0)

#endif
