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

static void verdict_sim(char *buf, size_t taille, const Ensemble *e, const ResultatSim *r)
{
    if (r->echecs == 0)
        snprintf(buf, taille, "FAISABLE");
    else
        snprintf(buf, taille, "NON (t=%ld, %s)", r->premier_echec,
                 e->t[r->tache_premier_echec].nom);
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
            affichage = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            duree = atol(argv[++i]);
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

    long H = hyperperiode(&e, DUREE_MAX);
    if (duree <= 0) duree = H;
    printf("Hyperpériode (PPCM des périodes) = %ld%s -> durée de simulation = %ld\n",
           H, H == DUREE_MAX ? " (plafonnée)" : "", duree);
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
        simuler(&e, (Politique)p, 1, duree, preemptif ? affichage : 0, verbeux, &sim_p[p]);
        simuler(&e, (Politique)p, 0, duree, preemptif ? 0 : affichage, verbeux, &sim_np[p]);

        if (duree == H && (sim_p[p].reliquat > 0 || sim_np[p].reliquat > 0))
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
        verdict_sim(a, sizeof a, &e, &sim_p[p]);
        verdict_sim(b, sizeof b, &e, &sim_np[p]);
        printf("| %-9s | %-14s | %-19s | %-19s |\n", nom_politique((Politique)p),
               theorie[p] == 1 ? "FAISABLE" : theorie[p] == 0 ? "NON FAISABLE" : "indecis",
               a, b);
    }
    printf("+-----------+----------------+---------------------+---------------------+\n");
    printf("(NON (t=x, tâche) : première échéance ratée à l'instant x)\n");
    return 0;
}
