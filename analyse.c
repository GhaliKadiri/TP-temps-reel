#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "analyse.h"

const char *nom_politique(Politique p)
{
    switch (p) {
    case HPF: return "HPF (Highest Priority First)";
    }
    return "?";
}

/* HPF : la plus grande valeur de prio passe en premier.
 * En cas d'égalité, on garde l'ordre du fichier (id). */
static int cmp_hpf(const void *a, const void *b)
{
    const Tache *x = a, *y = b;
    if (x->prio != y->prio) return y->prio - x->prio;
    return x->id - y->id;
}

void trier_par_priorite(Ensemble *e, Politique p)
{
    int (*cmp)(const void *, const void *) = NULL;
    switch (p) {
    case HPF: cmp = cmp_hpf; break;
    }
    qsort(e->t, e->n, sizeof(Tache), cmp);
}

double charge(const Ensemble *e)
{
    double U = 0.0;
    for (int i = 0; i < e->n; i++)
        U += (double)e->t[i].C / e->t[i].T;
    return U;
}

double borne_liu_layland(int n)
{
    return n * (pow(2.0, 1.0 / n) - 1.0);
}

/* plafond de a/b pour des entiers positifs, sans passer par les flottants */
static long plafond(long a, long b)
{
    return (a + b - 1) / b;
}

long temps_reponse(const Ensemble *e, int i, int trace)
{
    /* Si les tâches 0..i chargent le CPU à plus de 100 %,
     * la suite r(n) croît indéfiniment : pas de point fixe. */
    double U = 0.0;
    for (int j = 0; j <= i; j++)
        U += (double)e->t[j].C / e->t[j].T;
    if (U > 1.0)
        return -1;

    /* r(0) = Ci
     * r(n+1) = Ci + somme sur j de hp(i) de plafond(r(n) / Tj) * Cj
     * arrêt quand r(n+1) = r(n) */
    long r = e->t[i].C;
    if (trace) printf("    r(0) = %ld\n", r);

    for (int k = 1; ; k++) {
        long suivant = e->t[i].C;
        for (int j = 0; j < i; j++)   /* hp(i) = tâches placées avant i */
            suivant += plafond(r, e->t[j].T) * e->t[j].C;

        if (trace) printf("    r(%d) = %ld\n", k, suivant);
        if (suivant == r)
            return r;
        r = suivant;
    }
}

int analyser(Ensemble *e, Politique p)
{
    printf("\n===== Analyse de faisabilité : %s =====\n", nom_politique(p));

    trier_par_priorite(e, p);
    printf("\nOrdre de priorité (du plus au moins prioritaire) :");
    for (int i = 0; i < e->n; i++)
        printf(" %s%s", e->t[i].nom, i < e->n - 1 ? " >" : "\n");

    /* ---- Étape 1 : condition de charge (nécessaire) ---- */
    double U = charge(e);
    printf("\n-- Étape 1 : charge processeur --\n");
    printf("U = ");
    for (int i = 0; i < e->n; i++)
        printf("%d/%d%s", e->t[i].C, e->t[i].T, i < e->n - 1 ? " + " : "");
    printf(" = %.3f\n", U);

    if (U > 1.0) {
        printf("U > 1 : le processeur est surchargé, NON FAISABLE quelle que soit la politique.\n");
        return 0;
    }
    printf("U <= 1 : condition nécessaire respectée.\n");
    printf("(Pour info, borne de Liu & Layland pour n=%d : %.3f -- elle ne s'applique\n"
           " qu'à RM avec D = T, elle ne permet donc pas de conclure ici.)\n",
           e->n, borne_liu_layland(e->n));
    printf("On passe au calcul des temps de réponse.\n");

    /* ---- Étape 2 : temps de réponse (nécessaire et suffisante) ---- */
    printf("\n-- Étape 2 : temps de réponse --\n");
    long R[TACHES_MAX];
    for (int i = 0; i < e->n; i++) {
        printf("  %s :\n", e->t[i].nom);
        R[i] = temps_reponse(e, i, 1);
        if (R[i] < 0)
            printf("    diverge (charge des tâches plus prioritaires > 1)\n");
    }

    int faisable = 1;
    printf("\n+------------+------+------+------+----------+------+----------+\n");
    printf("| %-10s | %4s | %4s | %4s | %8s | %4s | %-8s |\n",
           "Tache", "C", "D", "T", "Priorite", "R", "R <= D ?");
    printf("+------------+------+------+------+----------+------+----------+\n");
    for (int i = 0; i < e->n; i++) {
        const Tache *t = &e->t[i];
        int ok = R[i] >= 0 && R[i] <= t->D;
        if (!ok) faisable = 0;
        if (R[i] >= 0)
            printf("| %-10s | %4d | %4d | %4d | %8d | %4ld | %-8s |\n",
                   t->nom, t->C, t->D, t->T, t->prio, R[i], ok ? "oui" : "NON");
        else
            printf("| %-10s | %4d | %4d | %4d | %8d | %4s | %-8s |\n",
                   t->nom, t->C, t->D, t->T, t->prio, "inf", "NON");
    }
    printf("+------------+------+------+------+----------+------+----------+\n");

    if (faisable)
        printf("\n=> Verdict %s : FAISABLE (toutes les tâches ont R <= D)\n", nom_politique(p));
    else {
        printf("\n=> Verdict %s : NON FAISABLE (", nom_politique(p));
        int premier = 1;
        for (int i = 0; i < e->n; i++) {
            if (R[i] >= 0 && R[i] <= e->t[i].D) continue;
            printf("%s%s", premier ? "" : ", ", e->t[i].nom);
            if (R[i] >= 0) printf(" : R=%ld > D=%d", R[i], e->t[i].D);
            else           printf(" : R infini");
            premier = 0;
        }
        printf(")\n");
    }
    return faisable;
}
