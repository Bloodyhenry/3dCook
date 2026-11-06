/* Détection d'un souffle (voir souffle.h). */
#include "souffle.h"

/* Seuils par sensibilité : 1 = il faut souffler fort et près, 5 = un souffle léger suffit. */
static const float SEUILS[SOUFFLE_SENSIBILITE_MAX] = {0.40f, 0.28f, 0.20f, 0.14f, 0.09f};

float souffle_seuil(int sensibilite)
{
    if (sensibilite < SOUFFLE_SENSIBILITE_MIN) sensibilite = SOUFFLE_SENSIBILITE_MIN;
    if (sensibilite > SOUFFLE_SENSIBILITE_MAX) sensibilite = SOUFFLE_SENSIBILITE_MAX;
    return SEUILS[sensibilite - 1];
}

void souffle_regler(Souffle *s, int sensibilite)
{
    s->seuil = souffle_seuil(sensibilite);
}

void souffle_init(Souffle *s, int sensibilite)
{
    *s = (Souffle){0};
    souffle_regler(s, sensibilite);
    s->arme = true;
}

float souffle_niveau(const int16_t *e, int n)
{
    if (n <= 0) return 0;
    /* On retire la composante continue (décalage du micro) avant de mesurer. */
    int64_t somme = 0;
    for (int i = 0; i < n; i++) somme += e[i];
    int32_t moyenne = (int32_t)(somme / n);
    int64_t ecarts = 0;
    for (int i = 0; i < n; i++) {
        int32_t d = e[i] - moyenne;
        ecarts += d < 0 ? -d : d;
    }
    float niveau = (float)ecarts / (float)n / 32768.0f;
    return niveau > 1.0f ? 1.0f : niveau;
}

void souffle_ignorer(Souffle *s, uint64_t jusqu_a)
{
    s->au_dessus_ms = s->trou_ms = 0;
    if (jusqu_a > s->pause_jusqu_a) s->pause_jusqu_a = jusqu_a;
}

bool souffle_analyser(Souffle *s, const int16_t *e, int n, uint64_t maintenant)
{
    if (n <= 0) return false;
    int duree = (int)((int64_t)n * 1000 / SOUFFLE_FREQUENCE);
    if (duree < 1) duree = 1;
    s->niveau = souffle_niveau(e, n);
    bool fort = s->niveau >= s->seuil;

    if (maintenant < s->pause_jusqu_a) {
        s->au_dessus_ms = s->trou_ms = 0;
        return false;
    }

    if (!fort) {
        s->calme_ms += duree;
        s->trou_ms += duree;
        if (s->trou_ms > SOUFFLE_TROU_MS) s->au_dessus_ms = 0;
        if (s->calme_ms >= SOUFFLE_CALME_MS) s->arme = true;
        return false;
    }

    s->calme_ms = 0;
    s->trou_ms = 0;
    s->au_dessus_ms += duree;
    if (s->arme && s->au_dessus_ms >= SOUFFLE_DUREE_MS) {
        s->arme = false;
        s->au_dessus_ms = 0;
        s->pause_jusqu_a = maintenant + SOUFFLE_PAUSE_MS;
        return true;
    }
    return false;
}
