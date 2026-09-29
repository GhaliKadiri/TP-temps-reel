#ifndef SIMULATEUR_H
#define SIMULATEUR_H

#include "tache.h"
#include "analyse.h"

#define AFFICHAGE_MAX 200   /* largeur maximale du chronogramme */
#define JOBS_MAX      512   /* jobs terminés mémorisés (temps de réponse par activation) */

typedef struct {
    int  tache;        /* indice de la tâche      */
    long activation;
    long fin;          /* temps de réponse = fin - activation */
} JobFini;

typedef struct {
    int  echecs;                 /* nombre d'échéances ratées          */
    long premier_echec;          /* instant du 1er dépassement, -1 si aucun */
    int  tache_premier_echec;    /* id de la tâche concernée           */
    int  preemptions;
    long reliquat;               /* travail encore en attente à la fin de la simulation */
    long R_max[TACHES_MAX];      /* pire temps de réponse observé (par id), -1 si aucun job fini */
    long R_max_act[TACHES_MAX];  /* activation du job qui a ce pire temps de réponse */
    JobFini jobs[JOBS_MAX];      /* les premiers jobs terminés, dans l'ordre de fin */
    int  nb_jobs;
} ResultatSim;

/* Job « surveillé » (méthode de Spuri) : on veut la date de fin du job de la
 * tâche 'tache' activé à 'activation'. La simulation s'arrête dès qu'il est
 * terminé ; fin = -1 s'il ne l'est pas à la fin de la simulation.
 * Pour obtenir le cas le plus défavorable, cette tâche perd toutes les
 * égalités d'échéance (EDF). */
typedef struct {
    int  tache;
    long activation;
    long fin;
} JobSuivi;

/* Simule l'exécution de l'ensemble de tâches sur [0, duree[ avec la politique
 * donnée. La tâche i est activée à S, S + T, S + 2T... (S = 0 pour les tâches
 * lues dans le fichier : activation simultanée à t = 0).
 *   preemptif : 1 = préemptif, 0 = non préemptif
 *   duree_complete : 1 si duree suffit pour conclure (sinon, l'absence
 *               d'échec observé ne prouve pas la faisabilité)
 *   affichage : nombre d'unités de temps montrées dans le chronogramme
 *               et le journal (0 = rien n'est affiché)
 *   journal   : 1 = affiche les événements (activation, file parcourue,
 *               élection, préemption, fin, dépassement) sur la console
 *   suivi     : job à surveiller (NULL si aucun)
 * Retourne le nombre d'échéances ratées. */
int simuler(const Ensemble *e, Politique p, int preemptif, long duree, int duree_complete,
            int affichage, int journal, ResultatSim *res, JobSuivi *suivi);

#endif
