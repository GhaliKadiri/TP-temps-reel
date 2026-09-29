#ifndef TACHE_H
#define TACHE_H

#define NOM_MAX    32
#define TACHES_MAX 64

/* Une tâche périodique temps réel (modèle du cours : C, D, T, P).
 * Toutes les valeurs sont des entiers (unités de temps, ici la seconde). */
typedef struct {
    char nom[NOM_MAX];
    int  C;     /* durée d'exécution (pire cas)            */
    int  D;     /* échéance relative                       */
    int  T;     /* période                                 */
    int  prio;  /* importance (HPF) : plus grand = plus prioritaire */
    int  id;    /* rang dans le fichier, départage les égalités de priorité */
} Tache;

/* Un ensemble de n tâches quelconque (Exercice 1, question 2). */
typedef struct {
    Tache t[TACHES_MAX];
    int   n;
} Ensemble;

/* Lit un fichier texte, une tâche par ligne : "nom C D T prio".
 * Les lignes vides et celles qui commencent par '#' sont ignorées.
 * Retourne 0 si tout va bien, -1 en cas d'erreur (message sur stderr). */
int  lire_taches(const char *chemin, Ensemble *e);

void afficher_taches(const Ensemble *e);

#endif
