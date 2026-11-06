/* Détection d'une main devant la caméra (voir mouvement.h). */
#include "mouvement.h"

#include <string.h>

#define N (MVT_L * MVT_H)

static const float SEUILS[5] = {0.55f, 0.45f, 0.35f, 0.27f, 0.20f};

float mvt_seuil(int s)
{
    if (s < 1) s = 1;
    if (s > 5) s = 5;
    return SEUILS[s - 1];
}

void mvt_regler(Mouvement *m, int s) { m->seuil = mvt_seuil(s); }

void mvt_init(Mouvement *m, int s)
{
    memset(m, 0, sizeof(*m));
    mvt_regler(m, s);
    m->arme = true;
}

void mvt_reduire(const uint8_t *yuyv, int w, int h, uint8_t *sortie)
{
    int bx = w / MVT_L, by = h / MVT_H;
    for (int y = 0; y < MVT_H; y++)
        for (int x = 0; x < MVT_L; x++) {
            unsigned somme = 0;
            for (int j = 0; j < by; j++)
                for (int i = 0; i < bx; i++)
                    somme += yuyv[((y * by + j) * w + (x * bx + i)) * 2];   /* octet Y */
            sortie[y * MVT_L + x] = (uint8_t)(somme / (unsigned)(bx * by));
        }
}

void mvt_reduire_rgb565(const uint16_t *px, int w, int h, uint8_t *sortie)
{
    int bx = w / MVT_L, by = h / MVT_H;
    for (int y = 0; y < MVT_H; y++)
        for (int x = 0; x < MVT_L; x++) {
            unsigned somme = 0;
            for (int j = 0; j < by; j++)
                for (int i = 0; i < bx; i++) {
                    uint16_t p = px[(y * by + j) * w + (x * bx + i)];
                    unsigned r = (p >> 11) & 31, v = (p >> 5) & 63, b = p & 31;
                    /* Luminance (Rec. 601) : 0,30 R + 0,59 V + 0,11 B, ramenée sur 0..255. */
                    somme += (r * 8 * 77 + v * 4 * 150 + b * 8 * 29) >> 8;
                }
            sortie[y * MVT_L + x] = (uint8_t)(somme / (unsigned)(bx * by));
        }
}

static int moyenne(const uint8_t *a)
{
    unsigned s = 0;
    for (int i = 0; i < N; i++) s += a[i];
    return (int)(s / N);
}

float mvt_mesurer(const uint8_t *a, const uint8_t *b)
{
    int ma = moyenne(a), mb = moyenne(b), changes = 0;
    for (int i = 0; i < N; i++) {
        int d = (a[i] - ma) - (b[i] - mb);
        if (d > MVT_ECART || d < -MVT_ECART) changes++;
    }
    return (float)changes / N;
}

void mvt_ignorer(Mouvement *m, uint64_t jusqu_a)
{
    m->a_precedente = false;
    m->consecutives = 0;
    if (jusqu_a > m->pause_jusqu_a) m->pause_jusqu_a = jusqu_a;
}

bool mvt_analyser(Mouvement *m, const uint8_t *image, uint64_t maintenant)
{
    int duree = m->derniere_ms ? (int)(maintenant - m->derniere_ms) : 66;
    m->derniere_ms = maintenant;
    m->mouvement = m->a_precedente ? mvt_mesurer(image, m->precedente) : 0;
    bool avait = m->a_precedente;
    memcpy(m->precedente, image, N);
    m->a_precedente = true;
    if (!avait || maintenant < m->pause_jusqu_a) {
        m->consecutives = 0;
        return false;
    }
    if (m->mouvement < m->seuil / 2) {
        m->calme_ms += duree;
        if (m->calme_ms >= MVT_CALME_MS) m->arme = true;
    } else {
        m->calme_ms = 0;
    }
    if (m->mouvement < m->seuil) {
        m->consecutives = 0;
        return false;
    }
    if (++m->consecutives >= MVT_IMAGES && m->arme) {
        m->arme = false;
        m->consecutives = 0;
        m->pause_jusqu_a = maintenant + MVT_PAUSE_MS;
        return true;
    }
    return false;
}
