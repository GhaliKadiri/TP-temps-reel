#ifndef ANALYSE_H
#define ANALYSE_H

#include "tache.h"

/* Politiques d'ordonnancement à priorités fixes. */
typedef enum {
    HPF     /* Highest Priority First : priorité = importance donnée */
} Politique;

const char *nom_politique(Politique p);

/* Trie les tâches de la plus prioritaire à la moins prioritaire
 * selon la politique choisie. */
void trier_par_priorite(Ensemble *e, Politique p);

/* Étape 1 du cours : U = somme des Ci/Ti. */
double charge(const Ensemble *e);

/* Borne de Liu & Layland : n(2^(1/n) - 1). */
double borne_liu_layland(int n);

/* Étape 2 du cours : pire temps de réponse de la tâche i
 * (l'ensemble doit déjà être trié par priorité décroissante).
 * Retourne -1 si le calcul diverge (charge des tâches 0..i > 1).
 * Si trace != 0, affiche chaque itération. */
long temps_reponse(const Ensemble *e, int i, int trace);

/* Analyse complète : affiche U, les Ri et le verdict.
 * Retourne 1 si l'ensemble est faisable, 0 sinon. */
int analyser(Ensemble *e, Politique p);

#endif
