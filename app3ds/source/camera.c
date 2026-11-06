/* Capture de la caméra intérieure (voir camera.h). */
#include "camera.h"

#include <3ds.h>
#include <malloc.h>

#include "mouvement.h"

#define LARGEUR      160          /* SIZE_QQVGA */
#define HAUTEUR      120
#define TAILLE_IMAGE (LARGEUR * HAUTEUR * 2)   /* RGB565 : 2 octets par pixel */
#define SILENCE_MAX_MS 1000       /* sans image depuis 1 s : on relance la capture */

static u8 *brute;                 /* image reçue de la caméra */
static Handle reception;          /* signalé quand une image complète est arrivée */
static Handle erreur_tampon;      /* signalé si la capture s'est interrompue */
static u32 octets_transfert;
static bool active;
static uint64_t derniere_image_ms;

/* Attente sans bloquer : vrai si l'événement est signalé.
 * (Le code « délai dépassé » n'a pas le bit d'erreur : on compare à 0.) */
static bool signale(Handle h)
{
    return h && svcWaitSynchronization(h, 0) == 0;
}

static void recevoir(void)
{
    if (reception) svcCloseHandle(reception);
    reception = 0;
    CAMU_SetReceiving(&reception, brute, PORT_CAM1, TAILLE_IMAGE, (s16)octets_transfert);
}

static void relancer(uint64_t maintenant)
{
    CAMU_StopCapture(PORT_CAM1);
    CAMU_ClearBuffer(PORT_CAM1);
    CAMU_StartCapture(PORT_CAM1);
    recevoir();
    derniere_image_ms = maintenant;
}

bool camera_demarrer(void)
{
    if (active) return true;
    brute = malloc(TAILLE_IMAGE);
    if (!brute) return false;
    if (R_FAILED(camInit())) {
        free(brute);
        brute = NULL;
        return false;
    }
    CAMU_SetSize(SELECT_IN1, SIZE_QQVGA, CONTEXT_A);
    /* RGB565, comme l'exemple officiel : ordre des octets connu (la luminosité
     * est calculée par mvt_reduire_rgb565). */
    CAMU_SetOutputFormat(SELECT_IN1, OUTPUT_RGB_565, CONTEXT_A);
    CAMU_SetFrameRate(SELECT_IN1, FRAME_RATE_15);
    CAMU_SetNoiseFilter(SELECT_IN1, true);
    CAMU_SetAutoExposure(SELECT_IN1, true);
    CAMU_SetAutoWhiteBalance(SELECT_IN1, true);
    CAMU_SetTrimming(PORT_CAM1, false);
    CAMU_GetMaxBytes(&octets_transfert, LARGEUR, HAUTEUR);
    CAMU_SetTransferBytes(PORT_CAM1, octets_transfert, LARGEUR, HAUTEUR);
    if (R_FAILED(CAMU_Activate(SELECT_IN1))) {
        camExit();
        free(brute);
        brute = NULL;
        return false;
    }
    CAMU_GetBufferErrorInterruptEvent(&erreur_tampon, PORT_CAM1);
    CAMU_ClearBuffer(PORT_CAM1);
    CAMU_StartCapture(PORT_CAM1);
    recevoir();
    active = true;
    derniere_image_ms = osGetTime();
    return true;
}

void camera_arreter(void)
{
    if (!active) return;
    CAMU_StopCapture(PORT_CAM1);
    if (reception) svcCloseHandle(reception);
    if (erreur_tampon) svcCloseHandle(erreur_tampon);
    reception = erreur_tampon = 0;
    CAMU_Activate(SELECT_NONE);
    camExit();
    free(brute);
    brute = NULL;
    active = false;
}

bool camera_active(void)
{
    return active;
}

bool camera_image(uint8_t *reduite, uint64_t maintenant)
{
    if (!active) return false;
    if (signale(erreur_tampon) || maintenant - derniere_image_ms > SILENCE_MAX_MS) {
        if (erreur_tampon) svcClearEvent(erreur_tampon);
        relancer(maintenant);
        return false;
    }
    if (!signale(reception)) return false;
    mvt_reduire_rgb565((const uint16_t *)brute, LARGEUR, HAUTEUR, reduite);
    derniere_image_ms = maintenant;
    recevoir();   /* image suivante */
    return true;
}
