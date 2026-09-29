#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "analyse.h"

const char *nom_politique(Politique p)
{
    switch (p) {
    case HPF: return "HPF";
    case RM:  return "RM";
    case DM:  return "DM";
    case EDF: return "EDF";
    default:  return "?";
    }
}

static const char *description_politique(Politique p)
{
    switch (p) {
    case HPF: return "Highest Priority First : priorité = importance donnée";
    case RM:  return "Rate Monotonic : plus petite période = plus prioritaire";
    case DM:  return "Deadline Monotonic : plus petite échéance relative = plus prioritaire";
    case EDF: return "Earliest Deadline First : plus proche échéance absolue = plus prioritaire";
    default:  return "?";
    }
}

int est_priorite_fixe(Politique p)
{
    return p == HPF || p == RM || p == DM;
}

/* ------------------------------------------------------------------ */
/* Attribution des priorités fixes : une fonction de comparaison par   */
/* politique, utilisée par qsort. En cas d'égalité, ordre du fichier.  */
/* ------------------------------------------------------------------ */

static int cmp_hpf(const void *a, const void *b)
{
    const Tache *x = a, *y = b;
    if (x->prio != y->prio) return y->prio - x->prio;   /* grande prio d'abord */
    return x->id - y->id;
}

static int cmp_rm(const void *a, const void *b)
{
    const Tache *x = a, *y = b;
    if (x->T != y->T) return x->T - y->T;               /* petite période d'abord */
    return x->id - y->id;
}

static int cmp_dm(const void *a, const void *b)
{
    const Tache *x = a, *y = b;
    if (x->D != y->D) return x->D - y->D;               /* petite échéance d'abord */
    return x->id - y->id;
}

void trier_par_priorite(Ensemble *e, Politique p)
{
    int (*cmp)(const void *, const void *);
    switch (p) {
    case HPF: cmp = cmp_hpf; break;
    case RM:  cmp = cmp_rm;  break;
    case DM:  cmp = cmp_dm;  break;
    default:  return;        /* EDF : pas de priorité fixe */
    }
    qsort(e->t, e->n, sizeof(Tache), cmp);
}

/* ------------------------------------------------------------------ */
/* Outils de calcul                                                    */
/* ------------------------------------------------------------------ */

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

static long pgcd(long a, long b)
{
    while (b != 0) { long r = a % b; a = b; b = r; }
    return a;
}

long hyperperiode(const Ensemble *e, long plafond_max)
{
    long h = 1;
    for (int i = 0; i < e->n; i++) {
        h = h / pgcd(h, e->t[i].T) * e->t[i].T;
        if (h > plafond_max) return plafond_max;
    }
    return h;
}

long periode_active(const Ensemble *e, int trace)
{
    if (charge(e) > 1.0)
        return R_INFINI;

    /* cours p.129 : on part de t = 1 et on itère t = W(t) jusqu'à stabilité */
    long t = 1;
    for (;;) {
        long w = 0;
        for (int i = 0; i < e->n; i++)
            w += plafond(t, e->t[i].T) * e->t[i].C;
        if (trace) printf("    W(%ld) = %ld\n", t, w);
        if (w == t)
            return t;
        t = w;
    }
}

long temps_reponse(const Ensemble *e, int i, int trace)
{
    /* Si les tâches 0..i chargent le CPU à plus de 100 %,
     * la suite r(n) croît indéfiniment : pas de point fixe. */
    double U = 0.0;
    for (int j = 0; j <= i; j++)
        U += (double)e->t[j].C / e->t[j].T;
    if (U > 1.0)
        return R_INFINI;

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

/* ------------------------------------------------------------------ */
/* Analyse théorique                                                   */
/* ------------------------------------------------------------------ */

static void afficher_charge(const Ensemble *e, double U)
{
    printf("-- Étape 1 : charge processeur --\n");
    printf("U = ");
    for (int i = 0; i < e->n; i++)
        printf("%d/%d%s", e->t[i].C, e->t[i].T, i < e->n - 1 ? " + " : "");
    printf(" = %.3f\n", U);
}

static int d_egal_t(const Ensemble *e)
{
    for (int i = 0; i < e->n; i++)
        if (e->t[i].D != e->t[i].T) return 0;
    return 1;
}

static int analyser_priorite_fixe(const Ensemble *orig, Politique p, long R[], int trace)
{
    Ensemble e = *orig;               /* on trie une copie, l'original reste dans l'ordre du fichier */
    trier_par_priorite(&e, p);

    printf("Ordre de priorité :");
    for (int i = 0; i < e.n; i++)
        printf(" %s%s", e.t[i].nom, i < e.n - 1 ? " >" : "\n\n");

    /* ---- Étape 1 : condition de charge (nécessaire) ---- */
    double U = charge(&e);
    afficher_charge(&e, U);
    if (U > 1.0) {
        printf("U > 1 : processeur surchargé => NON FAISABLE quelle que soit la politique.\n");
        for (int i = 0; i < e.n; i++) R[e.t[i].id] = R_INFINI;
        return 0;
    }

    double Ub = borne_liu_layland(e.n);
    if (p == RM && d_egal_t(&e)) {
        if (U <= Ub)
            printf("U <= U_RM = %.3f : condition suffisante de Liu & Layland => faisable.\n", Ub);
        else
            printf("U_RM = %.3f < U <= 1 : la charge ne permet pas de conclure.\n", Ub);
    } else {
        printf("U <= 1 : condition nécessaire respectée. (La borne de Liu & Layland %.3f\n"
               "ne s'applique qu'à RM avec D = T : elle n'est pas utilisée ici.)\n", Ub);
    }

    /* ---- Étape 2 : temps de réponse (nécessaire et suffisante) ---- */
    printf("\n-- Étape 2 : temps de réponse (condition nécessaire et suffisante) --\n");
    for (int i = 0; i < e.n; i++) {
        if (trace) printf("  %s :\n", e.t[i].nom);
        R[e.t[i].id] = temps_reponse(&e, i, trace);
    }

    int faisable = 1;
    printf("+------------+------+------+------+------+------+----------+\n");
    printf("| %-10s | %4s | %4s | %4s | %4s | %4s | %-8s |\n",
           "Tache", "C", "D", "T", "Prio", "R", "R <= D ?");
    printf("+------------+------+------+------+------+------+----------+\n");
    for (int i = 0; i < e.n; i++) {
        const Tache *t = &e.t[i];
        long r = R[t->id];
        int ok = r != R_INFINI && r <= t->D;
        if (!ok) faisable = 0;
        char rs[16];
        if (r == R_INFINI) snprintf(rs, sizeof rs, "inf");
        else               snprintf(rs, sizeof rs, "%ld", r);
        printf("| %-10s | %4d | %4d | %4d | %4d | %4s | %-8s |\n",
               t->nom, t->C, t->D, t->T, t->prio, rs, ok ? "oui" : "NON");
    }
    printf("+------------+------+------+------+------+------+----------+\n");
    return faisable;
}

static int analyser_edf(const Ensemble *e, int trace)
{
    double U = charge(e);
    afficher_charge(e, U);
    if (U > 1.0) {
        printf("U > 1 : processeur surchargé => NON FAISABLE (même EDF, pourtant optimal).\n");
        return 0;
    }

    /* Période d'étude (cours p.127-131) : le pire cas se trouve dedans. */
    printf("\n-- Période d'étude (busy period) : t = W(t) --\n");
    long bp = periode_active(e, trace);
    printf("Période active = %ld\n\n", bp);

    if (d_egal_t(e)) {
        printf("D = T pour toutes les tâches : EDF est optimal et la condition\n"
               "U <= 1 est nécessaire ET suffisante => FAISABLE.\n");
        return 1;
    }
    printf("Il existe des tâches avec D < T : U <= 1 n'est que nécessaire.\n"
           "=> On ne peut pas conclure par la charge, la simulation tranche.\n");
    return -1;
}

int analyser(const Ensemble *e, Politique p, long R[], int trace)
{
    printf("\n==================== %s ====================\n", nom_politique(p));
    printf("(%s)\n\n", description_politique(p));

    int res = est_priorite_fixe(p) ? analyser_priorite_fixe(e, p, R, trace)
                                   : analyser_edf(e, trace);

    printf("\n=> Analyse théorique %s (préemptif) : %s\n", nom_politique(p),
           res == 1 ? "FAISABLE" : res == 0 ? "NON FAISABLE" : "INDÉCIS (voir simulation)");
    return res;
}
