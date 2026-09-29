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

/* Durée de simulation qui suffit pour conclure à la faisabilité (tâches
 * activées ensemble à t = 0, D <= T) ; -1 si aucune durée ne suffit :
 *   - priorités fixes, préemptif : instant critique (Liu & Layland), le 1er job
 *     de chaque tâche a le pire temps de réponse -> il suffit d'aller jusqu'à max D ;
 *   - EDF préemptif : si une échéance est ratée, la première l'est dans la 1re
 *     période active L (Baruah, Rosier & Howell 1990 ; Spuri 1996) -> L ;
 *   - non préemptif : pas de résultat aussi simple, on garde l'hyperpériode H
 *     (si tout est fini à H, le déroulement se répète à l'identique). */
static long duree_necessaire(const Ensemble *e, Politique p, int preemptif, long H, int plafonne)
{
    if (preemptif && est_priorite_fixe(p)) {
        long dmax = 0;
        for (int i = 0; i < e->n; i++)
            if (e->t[i].D > dmax) dmax = e->t[i].D;
        return dmax;
    }
    if (preemptif && p == EDF) {
        long L = periode_active(e, 0);   /* R_INFINI (-1) si U > 1 */
        if (L != R_INFINI) return L;
    }
    return plafonne ? -1 : H;
}

/* EDF : temps de réponse des activations du déroulement synchrone (celui de
 * la simulation préemptive) situées dans [0, min(L, durée)[. Ce n'est PAS
 * l'ensemble A de Spuri (voir analyse.c) : le pire cas peut apparaître plus
 * tard dans ce déroulement, dans une configuration décalée. */
static void reponses_deroulement_synchrone(const Ensemble *e, const ResultatSim *r, long duree)
{
    long L = periode_active(e, 0);
    if (L == R_INFINI) return;
    long borne = L < duree ? L : duree;

    printf("\nDéroulement synchrone (simulation EDF préemptive) : temps de réponse\n"
           "des activations dans [0, %ld[%s :\n", borne,
           borne == L ? " (1re période active)" : "");
    int incomplet = 0;
    for (int i = 0; i < e->n; i++) {
        long max_bp = -1, nb = 0;
        printf("  %-10s", e->t[i].nom);
        for (int k = 0; k < r->nb_jobs; k++) {
            const JobFini *j = &r->jobs[k];
            if (j->tache != i || j->activation >= borne) continue;
            long rep = j->fin - j->activation;
            printf(" a=%ld:r=%ld", j->activation, rep);
            if (rep > max_bp) max_bp = rep;
            nb++;
        }
        /* activations attendues dans [0, borne[ : 0, T, 2T... */
        if (nb < (borne + e->t[i].T - 1) / e->t[i].T) incomplet = 1;
        printf("  -> max = %ld", max_bp);
        if (r->R_max[i] > max_bp)
            printf("  (plus tard : r=%ld pour a=%ld)", r->R_max[i], r->R_max_act[i]);
        printf("\n");
    }
    if (incomplet)
        printf("  (liste incomplète : jobs non terminés ou non mémorisés)\n");
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

    if (plafonne)
        printf("Hyperpériode (PPCM des périodes) > %ld -> simulation limitée à %ld\n",
               DUREE_MAX, duree);
    else
        printf("Hyperpériode (PPCM des périodes) = %ld -> durée de simulation = %ld\n", H, duree);
    if (plafonne || duree < H)
        printf("Remarque : la simulation ne couvre pas l'hyperpériode. Une échéance ratée\n"
               "reste une preuve de non-faisabilité ; l'absence d'échec ne prouve la\n"
               "faisabilité que si la durée atteint max D (priorités fixes, préemptif) ou\n"
               "la période active (EDF, préemptif). Sinon : NON CONCLUANT, noté (*).\n");
    printf("Hypothèses : monoprocesseur, tâches indépendantes, toutes activées à t = 0,\n"
           "D <= T, priorité HPF : valeur plus grande = plus prioritaire.\n");

    /* Résultats pour le tableau récapitulatif */
    int  theorie[NB_POLITIQUES];
    int  concluant_p[NB_POLITIQUES], concluant_np[NB_POLITIQUES];
    static ResultatSim sim_p[NB_POLITIQUES], sim_np[NB_POLITIQUES];
    long R[TACHES_MAX];
    int  une_non_concluante = 0;

    for (int p = 0; p < NB_POLITIQUES; p++) {
        if (!choisies[p]) continue;
        theorie[p] = analyser(&e, (Politique)p, R, verbeux);

        /* la durée suffit-elle pour conclure, dans chaque mode ? */
        long nec_p  = duree_necessaire(&e, (Politique)p, 1, H, plafonne);
        long nec_np = duree_necessaire(&e, (Politique)p, 0, H, plafonne);
        concluant_p[p]  = nec_p  >= 0 && duree >= nec_p;
        concluant_np[p] = nec_np >= 0 && duree >= nec_np;

        /* simulation détaillée dans le mode demandé, l'autre mode en silence */
        simuler(&e, (Politique)p, 1, duree, concluant_p[p], preemptif ? affichage : 0,
                verbeux, &sim_p[p], NULL);
        simuler(&e, (Politique)p, 0, duree, concluant_np[p], preemptif ? 0 : affichage,
                verbeux, &sim_np[p], NULL);
        if ((!concluant_p[p] && sim_p[p].echecs == 0) || (!concluant_np[p] && sim_np[p].echecs == 0))
            une_non_concluante = 1;

        if (p == EDF)
            reponses_deroulement_synchrone(&e, &sim_p[p], duree);

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

        /* EDF : R de Spuri et pire cas observé sur toute l'hyperpériode.
         * Spuri couvre tous les scénarios (dont le synchrone) avec des égalités
         * d'échéance défavorables : on doit avoir R >= observé. L'égalité est
         * atteinte quand le pire scénario apparaît dans le déroulement synchrone. */
        if (p == EDF && theorie[p] != 0 && R[0] != R_INFINI) {
            if (plafonne || duree < H || sim_p[p].echecs > 0) {
                printf("Contrôle Spuri : non effectué (il faut une simulation sans échec\n"
                       "sur toute l'hyperpériode).\n");
            } else {
                int egal = 1, inferieur = 0;
                for (int i = 0; i < e.n; i++) {
                    if (R[i] != sim_p[p].R_max[i]) egal = 0;
                    if (R[i] <  sim_p[p].R_max[i]) inferieur = 1;
                }
                printf("Contrôle Spuri : R (Spuri) %s pire temps de réponse observé sur\n"
                       "l'hyperpériode%s.\n",
                       egal ? "==" : inferieur ? "< (ATTENTION)" : ">=",
                       egal || inferieur ? ""
                       : "\n(Spuri couvre des scénarios décalés et tranche les égalités d'échéance\n"
                         "au pire : il peut majorer strictement le déroulement synchrone)");
            }
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
        verdict_sim(a, sizeof a, &e, &sim_p[p], concluant_p[p]);
        verdict_sim(b, sizeof b, &e, &sim_np[p], concluant_np[p]);
        printf("| %-9s | %-14s | %-19s | %-19s |\n", nom_politique((Politique)p),
               theorie[p] == 1 ? "FAISABLE" : theorie[p] == 0 ? "NON FAISABLE" : "indecis",
               a, b);
    }
    printf("+-----------+----------------+---------------------+---------------------+\n");
    printf("(NON (t=x, tâche) : première échéance ratée à l'instant x)\n");
    if (une_non_concluante)
        printf("(*) aucun échec observé, mais la durée simulée est trop courte pour conclure\n");
    return 0;
}
