/* Alarme des minuteurs et veille (voir alarme.h). */
#include "alarme.h"

#include <3ds.h>
#include <math.h>
#include <string.h>

#define FREQ_ECH    22050                 /* échantillons par seconde */
#define NB_ECH      FREQ_ECH              /* motif d'une seconde, joué en boucle */
#define CANAL_NDSP  0
#define CANAL_CSND  8
#define LED_PERIODE_MS 3500               /* le motif de LED est renvoyé à ce rythme */

typedef enum { SON_AUCUN, SON_NDSP, SON_CSND } Son;

static Son son = SON_AUCUN;
static s16 *pcm;                          /* en mémoire linéaire (lue par le DSP) */
static ndspWaveBuf onde;
static bool led_prete, sonne_avant, veille_bloquee;
static uint64_t led_envoyee_ms;

/* Double bip à 880 Hz (2 × 120 ms), puis silence : « bip-bip ... bip-bip ». */
static void fabriquer_bip(void)
{
    for (int i = 0; i < NB_ECH; i++) {
        float t = (float)i / FREQ_ECH;
        bool bip = (t < 0.12f) || (t >= 0.20f && t < 0.32f);
        float v = 0;
        if (bip) {
            float debut = t < 0.12f ? 0.0f : 0.20f;
            float local = t - debut;
            /* Fondu de 5 ms au début et à la fin de chaque bip : pas de « clic ». */
            float enveloppe = fminf(1.0f, fminf(local, 0.12f - local) / 0.005f);
            v = sinf(2.0f * (float)M_PI * 880.0f * t) * enveloppe * 0.7f;
        }
        pcm[i] = (s16)(v * 32767.0f);
    }
}

void alarme_init(void)
{
    pcm = linearAlloc(NB_ECH * sizeof(s16));
    if (pcm) {
        fabriquer_bip();
        if (R_SUCCEEDED(ndspInit())) {
            son = SON_NDSP;
            ndspSetOutputMode(NDSP_OUTPUT_STEREO);
            ndspChnReset(CANAL_NDSP);
            ndspChnSetInterp(CANAL_NDSP, NDSP_INTERP_LINEAR);
            ndspChnSetRate(CANAL_NDSP, FREQ_ECH);
            ndspChnSetFormat(CANAL_NDSP, NDSP_FORMAT_MONO_PCM16);
            float mix[12] = {1.0f, 1.0f};
            ndspChnSetMix(CANAL_NDSP, mix);
            DSP_FlushDataCache(pcm, NB_ECH * sizeof(s16));
        } else if (R_SUCCEEDED(csndInit())) {
            son = SON_CSND;
            GSPGPU_FlushDataCache(pcm, NB_ECH * sizeof(s16));
        }
    }
    led_prete = R_SUCCEEDED(mcuHwcInit());
}

static void son_jouer(bool jouer)
{
    if (son == SON_NDSP) {
        ndspChnWaveBufClear(CANAL_NDSP);
        if (jouer) {
            memset(&onde, 0, sizeof(onde));
            onde.data_pcm16 = pcm;
            onde.nsamples = NB_ECH;
            onde.looping = true;
            ndspChnWaveBufAdd(CANAL_NDSP, &onde);
        }
    } else if (son == SON_CSND) {
        if (jouer)
            csndPlaySound(CANAL_CSND, SOUND_FORMAT_16BIT | SOUND_REPEAT, FREQ_ECH, 1.0f, 0.0f,
                          pcm, pcm, NB_ECH * sizeof(s16));
        else {
            CSND_SetPlayState(CANAL_CSND, 0);
            csndExecCmds(true);
        }
    }
}

/* Clignotement orange (allumé 250 ms, éteint 250 ms) ou extinction. */
static void led(bool allumer)
{
    if (!led_prete) return;
    MCU_InfoLedPattern motif;
    memset(&motif, 0, sizeof(motif));
    motif.animation.ticksPerFrame = mcuTicksFromMs(125);
    motif.animation.frameTransitionDelay = 0;
    motif.animation.numLastFrameRepeats = 0;
    if (allumer)
        for (int i = 0; i < 32; i++) {
            bool on = (i / 2) % 2 == 0;
            motif.redPattern[i] = on ? 0xFF : 0;
            motif.greenPattern[i] = on ? 0x60 : 0;
        }
    MCUHWC_SetInfoLedPattern(&motif);
}

void alarme_maj(bool sonne, bool minuteur_actif, uint64_t maintenant)
{
    if (sonne != sonne_avant) {
        son_jouer(sonne);
        led(sonne);
        led_envoyee_ms = maintenant;
        sonne_avant = sonne;
    } else if (sonne && maintenant - led_envoyee_ms > LED_PERIODE_MS) {
        led(true);   /* le motif dure 4 s : on le relance avant la fin */
        led_envoyee_ms = maintenant;
    }

    if (minuteur_actif != veille_bloquee) {
        aptSetSleepAllowed(!minuteur_actif);
        veille_bloquee = minuteur_actif;
    }
}

void alarme_fin(void)
{
    alarme_maj(false, false, 0);
    if (son == SON_NDSP) ndspExit();
    if (son == SON_CSND) csndExit();
    if (led_prete) mcuHwcExit();
    if (pcm) linearFree(pcm);
}

const char *alarme_son(void)
{
    return son == SON_NDSP ? "ndsp" : son == SON_CSND ? "csnd" : NULL;
}
