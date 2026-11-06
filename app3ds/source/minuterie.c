/* Minuteurs de cuisine (voir minuterie.h). */
#include "minuterie.h"

#include <stdio.h>
#include <string.h>

void minuterie_init(Minuterie *t)
{
    memset(t, 0, sizeof(*t));
}

int minuterie_trouver(const Minuterie *t, const char *rid, int def)
{
    for (int i = 0; i < MINUTERIE_MAX; i++)
        if (t->m[i].actif && t->m[i].def == def && strcmp(t->m[i].rid, rid) == 0) return i;
    return -1;
}

int minuterie_lancer(Minuterie *t, const char *rid, int def, const char *libelle,
                     int secondes, int secondes_max, uint64_t maintenant)
{
    int deja = minuterie_trouver(t, rid, def);
    if (deja >= 0) return deja;
    if (secondes <= 0) return -1;
    for (int i = 0; i < MINUTERIE_MAX; i++) {
        Minuteur *m = &t->m[i];
        if (m->actif) continue;
        memset(m, 0, sizeof(*m));
        m->actif = true;
        copier_texte(m->rid, rid, TAILLE_ID);
        m->def = def;
        copier_texte(m->libelle, libelle, TAILLE_LIBELLE);
        m->secondes = secondes;
        m->secondes_max = secondes_max > secondes ? secondes_max : 0;
        m->fin_ms = maintenant + (uint64_t)secondes * 1000u;
        return i;
    }
    return -1;
}

int minuterie_restant(const Minuteur *m, uint64_t maintenant)
{
    if (maintenant >= m->fin_ms) return 0;
    return (int)((m->fin_ms - maintenant + 999u) / 1000u);
}

int minuterie_maj(Minuterie *t, uint64_t maintenant)
{
    int nouveaux = 0;
    for (int i = 0; i < MINUTERIE_MAX; i++) {
        Minuteur *m = &t->m[i];
        if (m->actif && !m->sonne && maintenant >= m->fin_ms) {
            m->sonne = true;
            nouveaux++;
        }
    }
    return nouveaux;
}

bool minuterie_sonne(const Minuterie *t)
{
    for (int i = 0; i < MINUTERIE_MAX; i++)
        if (t->m[i].actif && t->m[i].sonne) return true;
    return false;
}

bool minuterie_active(const Minuterie *t)
{
    for (int i = 0; i < MINUTERIE_MAX; i++)
        if (t->m[i].actif) return true;
    return false;
}

void minuterie_arreter(Minuterie *t, int indice)
{
    if (indice >= 0 && indice < MINUTERIE_MAX) memset(&t->m[indice], 0, sizeof(t->m[indice]));
}

int minuterie_arreter_sonneries(Minuterie *t)
{
    int n = 0;
    for (int i = 0; i < MINUTERIE_MAX; i++)
        if (t->m[i].actif && t->m[i].sonne) {
            minuterie_arreter(t, i);
            n++;
        }
    return n;
}

ResultatToucher minuterie_toucher(Minuterie *t, const char *rid, int def, const char *libelle,
                                  int secondes, int secondes_max, uint64_t maintenant)
{
    int i = minuterie_trouver(t, rid, def);
    if (i < 0)
        return minuterie_lancer(t, rid, def, libelle, secondes, secondes_max, maintenant) >= 0
                   ? TOUCHER_LANCE : TOUCHER_PLEIN;
    Minuteur *m = &t->m[i];
    if (m->sonne || maintenant < m->confirmer_jusqu_a) {
        minuterie_arreter(t, i);
        return TOUCHER_ARRETE;
    }
    m->confirmer_jusqu_a = maintenant + MINUTERIE_CONFIRM_MS;
    return TOUCHER_CONFIRMER;
}

int minuterie_ordre(const Minuterie *t, int *indices, int max)
{
    int n = 0;
    for (int i = 0; i < MINUTERIE_MAX && n < max; i++)
        if (t->m[i].actif) indices[n++] = i;
    /* Tri par insertion (6 éléments au plus) : qui sonne d'abord, puis échéance. */
    for (int a = 1; a < n; a++) {
        int x = indices[a], b = a - 1;
        while (b >= 0) {
            const Minuteur *p = &t->m[indices[b]], *q = &t->m[x];
            bool avant = (q->sonne && !p->sonne) || (q->sonne == p->sonne && q->fin_ms < p->fin_ms);
            if (!avant) break;
            indices[b + 1] = indices[b];
            b--;
        }
        indices[b + 1] = x;
    }
    return n;
}

void minuterie_formater(int secondes, char *sortie, int taille)
{
    if (secondes < 0) secondes = 0;
    int h = secondes / 3600, mn = (secondes / 60) % 60, s = secondes % 60;
    if (h) snprintf(sortie, (size_t)taille, "%d:%02d:%02d", h, mn, s);
    else snprintf(sortie, (size_t)taille, "%d:%02d", mn, s);
}
