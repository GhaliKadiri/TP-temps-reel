#include <stdio.h>
#include <string.h>
#include "simulateur.h"

/* Une instance (job) d'une tâche : la k-ième activation. */
typedef struct {
    long activation;   /* date d'activation                 */
    long echeance;     /* échéance absolue = activation + D */
    int  reste;        /* temps d'exécution restant         */
    int  en_retard;    /* échéance déjà dépassée ?          */
} Job;

/* File d'attente des jobs d'une tâche (FIFO) : un job en retard continue
 * de s'exécuter, le suivant attend derrière lui. */
#define FILE_MAX 64
typedef struct {
    Job  j[FILE_MAX];
    int  tete, nb;
} FileJobs;

static Job *tete(FileJobs *f)          { return &f->j[f->tete]; }
static Job *job(FileJobs *f, int k)    { return &f->j[(f->tete + k) % FILE_MAX]; }

static void retirer_tete(FileJobs *f)
{
    f->tete = (f->tete + 1) % FILE_MAX;
    f->nb--;
}

/* ------------------------------------------------------------------ */
/* Choix de la tâche à exécuter                                        */
/* ------------------------------------------------------------------ */

/* La tâche a doit-elle passer avant la tâche b ? (indices de tâches)
 * À priorité égale, la tâche en cours garde le processeur (pas de
 * préemption inutile), sinon on suit l'ordre du fichier. */
static int plus_prioritaire(int a, int b, int courant, Politique p,
                            const int rang[], FileJobs files[])
{
    if (p == EDF) {
        long ea = tete(&files[a])->echeance, eb = tete(&files[b])->echeance;
        if (ea != eb) return ea < eb;          /* échéance absolue la plus proche */
    } else {
        if (rang[a] != rang[b]) return rang[a] < rang[b];   /* priorité fixe */
    }
    if (a == courant) return 1;
    if (b == courant) return 0;
    return a < b;
}

/* Parcourt la file d'attente et retourne la tâche la plus prioritaire
 * parmi celles qui ont un job prêt (-1 si aucune : processeur oisif). */
static int elire(int n, int courant, Politique p, const int rang[], FileJobs files[])
{
    int elu = -1;
    for (int i = 0; i < n; i++) {
        if (files[i].nb == 0) continue;
        if (elu == -1 || plus_prioritaire(i, elu, courant, p, rang, files))
            elu = i;
    }
    return elu;
}

/* ------------------------------------------------------------------ */
/* Chronogramme                                                        */
/* ------------------------------------------------------------------ */

static void afficher_chronogramme(const Ensemble *e, int largeur,
                                  char grille[][AFFICHAGE_MAX], const char ratees[])
{
    char regle[AFFICHAGE_MAX + 8];
    memset(regle, ' ', sizeof regle);
    for (int t = 0; t < largeur; t += 5) {
        char num[8];
        int l = snprintf(num, sizeof num, "%d", t);
        if (t + l <= largeur + 3) memcpy(regle + t, num, l);
    }
    regle[largeur + 3] = '\0';

    printf("\nChronogramme (t = 0 .. %d) :\n", largeur);
    printf("%-10s %s\n", "t", regle);
    for (int i = 0; i < e->n; i++)
        printf("%-10s %.*s\n", e->t[i].nom, largeur, grille[i]);
    printf("%-10s %.*s\n", "Depasse", largeur, ratees);
    printf("Légende : # exécution   - prête mais en attente   . inactive"
           "   X échéance ratée à cet instant\n");
}

/* ------------------------------------------------------------------ */
/* Simulation en temps discret                                         */
/* ------------------------------------------------------------------ */

int simuler(const Ensemble *e, Politique p, int preemptif, long duree,
            int affichage, int journal, ResultatSim *res)
{
    int n = e->n;
    static FileJobs files[TACHES_MAX];
    static char grille[TACHES_MAX][AFFICHAGE_MAX];
    char ratees[AFFICHAGE_MAX + 1];
    int rang[TACHES_MAX];

    if (affichage > AFFICHAGE_MAX) affichage = AFFICHAGE_MAX;
    if (affichage > duree) affichage = (int)duree;
    memset(files, 0, sizeof files);
    memset(ratees, ' ', sizeof ratees);

    /* rang de priorité fixe de chaque tâche (0 = la plus prioritaire) */
    if (est_priorite_fixe(p)) {
        Ensemble trie = *e;
        trier_par_priorite(&trie, p);
        for (int k = 0; k < n; k++) rang[trie.t[k].id] = k;
    }

    memset(res, 0, sizeof *res);
    res->premier_echec = -1;
    res->tache_premier_echec = -1;
    for (int i = 0; i < n; i++) res->R_max[i] = -1;

    if (affichage > 0)
        printf("\n--- Simulation %s %s sur [0, %ld[ ---\n", nom_politique(p),
               preemptif ? "préemptive" : "non préemptive", duree);
    if (journal && affichage > 0)
        printf("Journal des événements (jusqu'à t = %d) :\n", affichage);

    int courant = -1;       /* tâche qui possède le processeur */
    int etait_oisif = 0;

    for (long t = 0; t <= duree; t++) {
        int log = journal && t < affichage;

        /* 1) Contrôle des échéances : un job non terminé à son échéance est en retard. */
        for (int i = 0; i < n; i++) {
            for (int k = 0; k < files[i].nb; k++) {
                Job *jb = job(&files[i], k);
                if (jb->echeance == t && jb->reste > 0 && !jb->en_retard) {
                    jb->en_retard = 1;
                    res->echecs++;
                    if (res->premier_echec < 0) {
                        res->premier_echec = t;
                        res->tache_premier_echec = i;
                    }
                    if (t < affichage) ratees[t] = 'X';
                    if (log) printf("  t=%4ld : *** ÉCHÉANCE RATÉE *** %s (activée à %ld, il reste %d unité(s))\n",
                                    t, e->t[i].nom, jb->activation, jb->reste);
                }
            }
        }
        if (t == duree) break;   /* dernier instant : uniquement le contrôle des échéances */

        /* 2) Activations périodiques (toutes les tâches démarrent à t = 0 :
         *    instant critique, c'est le pire cas pour les priorités fixes). */
        int evenement = 0;
        for (int i = 0; i < n; i++) {
            if (t % e->t[i].T != 0) continue;
            evenement = 1;
            if (files[i].nb == FILE_MAX) {
                if (log) printf("  t=%4ld : file de %s saturée, activation perdue\n", t, e->t[i].nom);
                continue;
            }
            Job *jb = job(&files[i], files[i].nb);
            jb->activation = t;
            jb->echeance   = t + e->t[i].D;
            jb->reste      = e->t[i].C;
            jb->en_retard  = 0;
            files[i].nb++;
            if (log) printf("  t=%4ld : activation de %s (échéance absolue %ld)\n",
                            t, e->t[i].nom, jb->echeance);
        }

        /* 3) Ordonnancement : on (ré)élit à chaque événement d'ordonnancement.
         *    - processeur libre (fin d'un job) : toujours
         *    - nouvelle activation : seulement en mode préemptif */
        if (courant == -1 || (preemptif && evenement)) {
            int elu = elire(n, courant, p, rang, files);
            if (elu != courant) {
                if (courant != -1 && files[courant].nb > 0) {
                    res->preemptions++;
                    if (log) printf("  t=%4ld : %s préemptée par %s\n",
                                    t, e->t[courant].nom, e->t[elu].nom);
                }
                if (elu != -1 && log) {
                    if (p == EDF)
                        printf("  t=%4ld : %s élue (échéance absolue la plus proche : %ld)\n",
                               t, e->t[elu].nom, tete(&files[elu])->echeance);
                    else
                        printf("  t=%4ld : %s élue (rang de priorité %d)\n",
                               t, e->t[elu].nom, rang[elu] + 1);
                }
                courant = elu;
            }
        }

        /* 4) Chronogramme de l'unité [t, t+1[ */
        if (t < affichage)
            for (int i = 0; i < n; i++)
                grille[i][t] = (i == courant) ? '#' : (files[i].nb > 0 ? '-' : '.');

        /* 5) Exécution d'une unité de temps */
        if (courant == -1) {
            if (log && !etait_oisif) printf("  t=%4ld : processeur oisif\n", t);
            etait_oisif = 1;
            continue;
        }
        etait_oisif = 0;

        Job *jb = tete(&files[courant]);
        jb->reste--;
        if (jb->reste == 0) {
            long fin = t + 1;
            long r = fin - jb->activation;
            if (r > res->R_max[courant]) res->R_max[courant] = r;
            if (journal && fin < affichage)
                printf("  t=%4ld : fin de %s (temps de réponse %ld%s)\n", fin,
                       e->t[courant].nom, r, jb->en_retard ? ", EN RETARD" : "");
            retirer_tete(&files[courant]);
            courant = -1;   /* fin de job = événement d'ordonnancement */
        }
    }

    for (int i = 0; i < n; i++)
        for (int k = 0; k < files[i].nb; k++)
            res->reliquat += job(&files[i], k)->reste;

    if (affichage > 0) {
        afficher_chronogramme(e, affichage, grille, ratees);

        printf("\nPire temps de réponse observé :");
        for (int i = 0; i < n; i++) {
            if (res->R_max[i] < 0) printf(" %s=?", e->t[i].nom);
            else                   printf(" %s=%ld", e->t[i].nom, res->R_max[i]);
        }
        printf("\nPréemptions : %d\n", res->preemptions);
        if (res->echecs == 0)
            printf("=> Simulation %s %s : aucune échéance ratée sur [0, %ld] => FAISABLE\n",
                   nom_politique(p), preemptif ? "préemptive" : "non préemptive", duree);
        else
            printf("=> Simulation %s %s : %d échéance(s) ratée(s), la 1re à t=%ld (%s) => NON FAISABLE\n",
                   nom_politique(p), preemptif ? "préemptive" : "non préemptive",
                   res->echecs, res->premier_echec, e->t[res->tache_premier_echec].nom);
    }
    return res->echecs;
}
