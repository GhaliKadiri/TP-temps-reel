#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "tache.h"
#include "analyse.h"
#include "simulateur.h"

#define DUREE_MAX 1000000L   /* plafond de la durée de simulation */

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage : %s <fichier_taches> [options]\n"
        "  -p hpf|rm|dm|edf|tout  politique(s) à étudier (défaut : tout)\n"
        "  -n                     simulation détaillée en NON préemptif (défaut : préemptif)\n"
        "  -a N                   largeur du chronogramme / du journal (défaut : 60)\n"
        "  -d N                   durée de simulation (défaut : PPCM des périodes)\n"
        "  -v                     détail : itérations des calculs + journal des événements\n"
        "Format du fichier : une tâche par ligne \"nom C D T prio\" ('#' = commentaire)\n",
        prog);
}

static int lire_politique(const char *s, int choisies[])
{
    static const char *noms[NB_POLITIQUES] = { "hpf", "rm", "dm", "edf" };
    for (int p = 0; p < NB_POLITIQUES; p++) choisies[p] = 0;
    if (strcasecmp(s, "tout") == 0) {
        for (int p = 0; p < NB_POLITIQUES; p++) choisies[p] = 1;
        return 0;
    }
    for (int p = 0; p < NB_POLITIQUES; p++)
        if (strcasecmp(s, noms[p]) == 0) { choisies[p] = 1; return 0; }
    return -1;
}

/* Lit un entier dans [min, max] ; retourne -1 si l'argument est invalide. */
static int lire_entier(const char *s, long min, long max, long *val)
{
    char *fin;
    long v = strtol(s, &fin, 10);
    if (*s == '\0' || *fin != '\0' || v < min || v > max) return -1;
    *val = v;
    return 0;
}

static void verdict_sim(char *buf, size_t taille, const Ensemble *e, const ResultatSim *r,
                        int complete)
{
    if (r->echecs == 0)
        snprintf(buf, taille, complete ? "FAISABLE" : "pas d'echec (*)");
    else
        snprintf(buf, taille, "NON (t=%ld, %s)", r->premier_echec,
                 e->t[r->tache_premier_echec].nom);
}

/* EDF (cours p.125-134) : la priorité change d'une activation à l'autre, le
 * pire temps de réponse n'est donc pas forcément obtenu à la 1re activation.
 * On liste le temps de réponse de chaque activation de la période active
 * (ensemble A du cours p.133), puis le pire cas sur toute la simulation. */
static void temps_reponse_par_activation(const Ensemble *e, const ResultatSim *r)
{
    long bp = periode_active(e, 0);
    if (bp == R_INFINI) return;

    printf("\nTemps de réponse de chaque activation a dans la période active [0, %ld[ :\n", bp);
    for (int i = 0; i < e->n; i++) {
        long max_bp = -1;
        printf("  %-10s", e->t[i].nom);
        for (int k = 0; k < r->nb_jobs; k++) {
            const JobFini *j = &r->jobs[k];
            if (j->tache != i || j->activation >= bp) continue;
            long rep = j->fin - j->activation;
            printf(" a=%ld:r=%ld", j->activation, rep);
            if (rep > max_bp) max_bp = rep;
        }
        printf("  -> max = %ld", max_bp);
        if (r->R_max[i] > max_bp)
            printf("  (pire cas plus tard : r=%ld pour a=%ld)", r->R_max[i], r->R_max_act[i]);
        printf("\n");
    }
    if (r->nb_jobs == JOBS_MAX)
        printf("  (liste tronquée à %d jobs)\n", JOBS_MAX);
}

int main(int argc, char *argv[])
{
    if (argc < 2) { usage(argv[0]); return 1; }

    int choisies[NB_POLITIQUES];
    lire_politique("tout", choisies);
    int preemptif = 1, affichage = 60, verbeux = 0;
    long duree = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            if (lire_politique(argv[++i], choisies) != 0) { usage(argv[0]); return 1; }
        } else if (strcmp(argv[i], "-n") == 0) {
            preemptif = 0;
        } else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            long v;
            if (lire_entier(argv[++i], 1, AFFICHAGE_MAX, &v) != 0) {
                fprintf(stderr, "-a : entier attendu entre 1 et %d\n", AFFICHAGE_MAX);
                return 1;
            }
            affichage = (int)v;
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            if (lire_entier(argv[++i], 1, DUREE_MAX, &duree) != 0) {
                fprintf(stderr, "-d : entier attendu entre 1 et %ld\n", DUREE_MAX);
                return 1;
            }
        } else if (strcmp(argv[i], "-v") == 0) {
            verbeux = 1;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    Ensemble e;
    if (lire_taches(argv[1], &e) != 0)
        return 1;

    printf("%d tâche(s) lue(s) depuis %s :\n", e.n, argv[1]);
    afficher_taches(&e);

    int plafonne;
    long H = hyperperiode(&e, DUREE_MAX, &plafonne);
    if (duree <= 0) duree = H;
    int complete = !plafonne && duree >= H;   /* la simulation couvre-t-elle tous les scénarios ? */

    if (plafonne)
        printf("Hyperpériode (PPCM des périodes) > %ld -> simulation limitée à %ld\n",
               DUREE_MAX, duree);
    else
        printf("Hyperpériode (PPCM des périodes) = %ld -> durée de simulation = %ld\n", H, duree);
    if (!complete)
        printf("ATTENTION : la simulation ne couvre pas l'hyperpériode. Une échéance ratée\n"
               "reste une preuve de non-faisabilité, mais l'absence d'échec ne prouve rien.\n");
    printf("Hypothèses : monoprocesseur, tâches indépendantes, toutes activées à t = 0,\n"
           "D <= T, priorité HPF : valeur plus grande = plus prioritaire.\n");

    /* Résultats pour le tableau récapitulatif */
    int  theorie[NB_POLITIQUES];
    ResultatSim sim_p[NB_POLITIQUES], sim_np[NB_POLITIQUES];
    long R[TACHES_MAX];

    for (int p = 0; p < NB_POLITIQUES; p++) {
        if (!choisies[p]) continue;
        theorie[p] = analyser(&e, (Politique)p, R, verbeux);

        /* simulation détaillée dans le mode demandé, l'autre mode en silence */
        simuler(&e, (Politique)p, 1, duree, complete, preemptif ? affichage : 0, verbeux, &sim_p[p]);
        simuler(&e, (Politique)p, 0, duree, complete, preemptif ? 0 : affichage, verbeux, &sim_np[p]);

        if (p == EDF)
            temps_reponse_par_activation(&e, &sim_p[p]);

        if (!plafonne && duree == H && (sim_p[p].reliquat > 0 || sim_np[p].reliquat > 0))
            printf("Attention : du travail reste en attente à t = H, le motif ne se répète pas\n"
                   "à l'identique ; relancer avec -d %ld (2H) pour confirmer.\n", 2 * H);

        /* cohérence théorie / simulation pour les priorités fixes préemptives */
        if (est_priorite_fixe((Politique)p) && preemptif) {
            int coherent = 1;
            for (int i = 0; i < e.n; i++)
                if (R[i] != R_INFINI && R[i] <= e.t[i].D && R[i] != sim_p[p].R_max[i])
                    coherent = 0;
            printf("Contrôle : temps de réponse simulés %s temps de réponse calculés"
                   " (tâches qui respectent leur échéance).\n",
                   coherent ? "==" : "!= (ATTENTION)");
        }
    }

    /* ---- Tableau récapitulatif ---- */
    printf("\n==================== RÉCAPITULATIF ====================\n");
    printf("+-----------+----------------+---------------------+---------------------+\n");
    printf("| %-9s | %-14s | %-19s | %-19s |\n",
           "Politique", "Theorie", "Simu. preemptive", "Simu. non preempt.");
    printf("+-----------+----------------+---------------------+---------------------+\n");
    for (int p = 0; p < NB_POLITIQUES; p++) {
        if (!choisies[p]) continue;
        char a[32], b[32];
        verdict_sim(a, sizeof a, &e, &sim_p[p], complete);
        verdict_sim(b, sizeof b, &e, &sim_np[p], complete);
        printf("| %-9s | %-14s | %-19s | %-19s |\n", nom_politique((Politique)p),
               theorie[p] == 1 ? "FAISABLE" : theorie[p] == 0 ? "NON FAISABLE" : "indecis",
               a, b);
    }
    printf("+-----------+----------------+---------------------+---------------------+\n");
    printf("(NON (t=x, tâche) : première échéance ratée à l'instant x)\n");
    if (!complete)
        printf("(*) durée de simulation < hyperpériode : non concluant\n");
    return 0;
}
