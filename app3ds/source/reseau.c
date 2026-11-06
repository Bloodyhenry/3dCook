/* Accès réseau de la console (voir reseau.h). */
#include "reseau.h"

#include <3ds.h>
#include <stdio.h>

#include "synchro.h"

/* Délai d'attente de la réponse : au-delà, la passerelle est jugée injoignable. */
#define DELAI_REPONSE_NS 6000000000ULL   /* 6 s */

static bool httpc_pret, ac_pret;

void reseau_init(void)
{
    httpc_pret = R_SUCCEEDED(httpcInit(0));   /* 0 : pas besoin de mémoire pour envoyer */
    ac_pret = R_SUCCEEDED(acInit());
}

void reseau_fin(void)
{
    if (httpc_pret) httpcExit();
    if (ac_pret) acExit();
}

bool reseau_wifi_connecte(void)
{
    u32 etat = 0;
    if (!ac_pret || R_FAILED(ACU_GetWifiStatus(&etat))) return true;   /* inconnu : on essaie */
    return etat != 0;
}

int reseau_telecharger(void *contexte, const char *url, char *tampon, int max,
                       char *erreur, int taille_erreur)
{
    (void)contexte;
    if (!httpc_pret) {
        snprintf(erreur, (size_t)taille_erreur, "service réseau indisponible");
        return -1;
    }
    httpcContext ctx;
    if (R_FAILED(httpcOpenContext(&ctx, HTTPC_METHOD_GET, url, 1))) {
        snprintf(erreur, (size_t)taille_erreur, "adresse invalide");
        return -1;
    }
    httpcSetKeepAlive(&ctx, HTTPC_KEEPALIVE_DISABLED);
    httpcAddRequestHeaderField(&ctx, "User-Agent", "recettes3ds/1");
    httpcAddRequestHeaderField(&ctx, "Connection", "close");

    int resultat = -1;
    u32 code = 0;
    if (R_FAILED(httpcBeginRequest(&ctx))
        || R_FAILED(httpcGetResponseStatusCodeTimeout(&ctx, &code, DELAI_REPONSE_NS))) {
        snprintf(erreur, (size_t)taille_erreur, "pas de réponse");
    } else if (code != 200) {
        /* La passerelle explique l'erreur dans le corps : {"erreur": "…"}. */
        char corps[512], explication[200];
        u32 lu = 0;
        httpcDownloadData(&ctx, (u8 *)corps, sizeof(corps), &lu);
        if (synchro_erreur_passerelle(corps, (int)lu, explication, sizeof(explication)))
            snprintf(erreur, (size_t)taille_erreur, "%s (%lu)", explication, (unsigned long)code);
        else if (code == 404)
            snprintf(erreur, (size_t)taille_erreur, "introuvable sur la passerelle (404)");
        else if (code == 502)
            snprintf(erreur, (size_t)taille_erreur, "la passerelle n'arrive pas à joindre Mealie (502)");
        else
            snprintf(erreur, (size_t)taille_erreur, "réponse HTTP %lu", (unsigned long)code);
    } else {
        u32 total = 0, lu = 0;
        Result r;
        do {
            lu = 0;
            r = httpcDownloadData(&ctx, (u8 *)tampon + total, (u32)max - total, &lu);
            total += lu;
        } while (r == (Result)HTTPC_RESULTCODE_DOWNLOADPENDING && total < (u32)max);

        if (r == (Result)HTTPC_RESULTCODE_DOWNLOADPENDING)
            snprintf(erreur, (size_t)taille_erreur, "réponse trop grosse");
        else if (R_FAILED(r))
            snprintf(erreur, (size_t)taille_erreur, "réception interrompue");
        else
            resultat = (int)total;
    }
    httpcCloseContext(&ctx);
    return resultat;
}
