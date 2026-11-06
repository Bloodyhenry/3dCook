/* Capture du micro (voir micro.h). */
#include "micro.h"

#include <3ds.h>
#include <malloc.h>
#include <string.h>

#define TAILLE_TAMPON 0x30000   /* mémoire partagée avec le service mic */

static u8 *tampon;
static u32 taille_donnees;     /* partie du tampon qui contient les échantillons */
static u32 derniere_position;
static bool pret;

bool micro_init(void)
{
    tampon = memalign(0x1000, TAILLE_TAMPON);
    if (!tampon) return false;
    if (R_FAILED(micInit(tampon, TAILLE_TAMPON))) {
        free(tampon);
        tampon = NULL;
        return false;
    }
    taille_donnees = micGetSampleDataSize();
    MICU_SetPower(true);
    if (R_FAILED(MICU_StartSampling(MICU_ENCODING_PCM16_SIGNED, MICU_SAMPLE_RATE_16360, 0,
                                    taille_donnees, true))) {
        micExit();
        free(tampon);
        tampon = NULL;
        return false;
    }
    derniere_position = micGetLastSampleOffset();
    pret = true;
    return true;
}

void micro_fin(void)
{
    if (!pret) return;
    MICU_StopSampling();
    micExit();
    free(tampon);
    tampon = NULL;
    pret = false;
}

bool micro_pret(void)
{
    return pret;
}

int micro_lire(int16_t *sortie, int max)
{
    if (!pret) return 0;
    u32 position = micGetLastSampleOffset() & ~1u;   /* aligné sur un échantillon */
    u32 octets = position >= derniere_position
                     ? position - derniere_position
                     : taille_donnees - derniere_position + position;   /* le tampon a bouclé */
    u32 voulu = (u32)max * 2;
    if (octets > voulu) {
        /* Retard (synchro, etc.) : on ne garde que les échantillons les plus récents. */
        derniere_position = (position + taille_donnees - voulu) % taille_donnees;
        octets = voulu;
    }
    u32 copie = 0;
    while (copie < octets) {
        u32 morceau = octets - copie;
        if (derniere_position + morceau > taille_donnees) morceau = taille_donnees - derniere_position;
        memcpy((u8 *)sortie + copie, tampon + derniere_position, morceau);
        copie += morceau;
        derniere_position = (derniere_position + morceau) % taille_donnees;
    }
    return (int)(octets / 2);
}
