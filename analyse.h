#ifndef ANALYSE_H
#define ANALYSE_H

#include "tache.h"

#define R_INFINI (-1L)   /* temps de réponse non borné (divergence) */

/* Politiques d'ordonnancement.
 * HPF, RM, DM : priorités fixes (Exercice 1)
 * EDF         : priorités dynamiques (Exercice 2) */
typedef enum { HPF, RM, DM, EDF, NB_POLITIQUES } Politique;

const char *nom_politique(Politique p);
int  est_priorite_fixe(Politique p);

/* Trie les tâches de la plus prioritaire à la moins prioritaire :
 * HPF : plus grande importance (prio) d'abord
 * RM  : plus petite période T d'abord
 * DM  : plus petite échéance relative D d'abord
 * Égalité : ordre du fichier. Uniquement pour les priorités fixes. */
void trier_par_priorite(Ensemble *e, Politique p);

/* U = somme des Ci/Ti (valeur approchée, pour l'affichage) */
double charge(const Ensemble *e);

/* U > 1 pour les n premières tâches de t[] ? Test exact en entiers
 * (pas d'erreur d'arrondi quand U vaut exactement 1). */
int charge_superieure_a_1(const Tache t[], int n);

/* Borne de Liu & Layland : n(2^(1/n) - 1) */
double borne_liu_layland(int n);

/* PPCM des périodes (durée de simulation couvrant tous les scénarios).
 * Plafonné à 'plafond' si le PPCM le dépasse (*plafonne passe alors à 1). */
long hyperperiode(const Ensemble *e, long plafond, int *plafonne);

/* Période d'étude (busy period) : plus petit t > 0 tel que
 * t = W(t) = somme des plafond(t/Ti)*Ci. Retourne R_INFINI si U > 1. */
long periode_active(const Ensemble *e, int trace);

/* Pire temps de réponse de la tâche i, ensemble trié par priorité décroissante.
 * Retourne R_INFINI si le calcul diverge. */
long temps_reponse(const Ensemble *e, int i, int trace);

/* Analyse théorique (préemptive) d'une politique.
 * R[id] reçoit le pire temps de réponse calculé de chaque tâche : formule
 * itérative pour les priorités fixes, méthode de Spuri pour EDF
 * (R_INFINI si non calculé).
 * Retourne 1 = faisable, 0 = non faisable, -1 = la théorie ne permet pas de
 * conclure (c'est alors la simulation qui tranche). */
int analyser(const Ensemble *e, Politique p, long R[], int trace);

#endif
