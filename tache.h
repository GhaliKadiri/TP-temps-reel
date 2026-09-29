#ifndef TACHE_H
#define TACHE_H

#define NOM_MAX    11   /* 10 caractères + fin de chaîne (largeur des tableaux) */
#define TACHES_MAX 64
#define VALEUR_MAX 1000000L   /* plus grande valeur acceptée pour C, D et T */

/* Une tâche périodique temps réel (modèle du cours : C, D, T, P).
 * Toutes les valeurs sont des entiers (unités de temps, ici la seconde). */
typedef struct {
    char nom[NOM_MAX];
    int  C;     /* durée d'exécution (pire cas)            */
    int  D;     /* échéance relative                       */
    int  T;     /* période                                 */
    int  prio;  /* importance (HPF) : plus grand = plus prioritaire */
    long S;     /* date de 1re activation (cours p.93). Vaut 0 pour les tâches
                 * lues ; seule la méthode de Spuri (EDF) la décale. */
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
