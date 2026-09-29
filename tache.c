#include <stdio.h>
#include <string.h>
#include "tache.h"

int lire_taches(const char *chemin, Ensemble *e)
{
    FILE *f = fopen(chemin, "r");
    if (f == NULL) {
        perror(chemin);
        return -1;
    }

    char ligne[256];
    int num_ligne = 0;
    e->n = 0;

    while (fgets(ligne, sizeof ligne, f) != NULL) {
        num_ligne++;

        /* ignorer les espaces en début de ligne, les lignes vides et les commentaires */
        char *p = ligne;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || *p == '\0' || *p == '#') continue;

        if (e->n >= TACHES_MAX) {
            fprintf(stderr, "Erreur : plus de %d tâches\n", TACHES_MAX);
            fclose(f);
            return -1;
        }

        Tache *t = &e->t[e->n];
        if (sscanf(p, "%31s %d %d %d %d", t->nom, &t->C, &t->D, &t->T, &t->prio) != 5) {
            fprintf(stderr, "%s:%d : format attendu \"nom C D T prio\"\n", chemin, num_ligne);
            fclose(f);
            return -1;
        }
        /* contrôles de cohérence du modèle de tâche */
        if (t->C <= 0 || t->D <= 0 || t->T <= 0) {
            fprintf(stderr, "%s:%d : C, D et T doivent être > 0\n", chemin, num_ligne);
            fclose(f);
            return -1;
        }
        if (t->C > t->D) {
            fprintf(stderr, "%s:%d : C > D, la tâche %s ne peut jamais respecter son échéance\n",
                    chemin, num_ligne, t->nom);
            fclose(f);
            return -1;
        }
        if (t->D > t->T) {
            /* les tests du cours (temps de réponse, EDF avec U <= 1) supposent D <= T */
            fprintf(stderr, "%s:%d : D > T non supporté (modèle du cours : D <= T)\n",
                    chemin, num_ligne);
            fclose(f);
            return -1;
        }
        t->id = e->n;
        e->n++;
    }

    fclose(f);
    if (e->n == 0) {
        fprintf(stderr, "%s : aucune tâche trouvée\n", chemin);
        return -1;
    }
    return 0;
}

void afficher_taches(const Ensemble *e)
{
    printf("+------------+------+------+------+----------+\n");
    printf("| %-10s | %4s | %4s | %4s | %8s |\n", "Tache", "C", "D", "T", "Priorité");
    printf("+------------+------+------+------+----------+\n");
    for (int i = 0; i < e->n; i++) {
        const Tache *t = &e->t[i];
        printf("| %-10s | %4d | %4d | %4d | %8d |\n", t->nom, t->C, t->D, t->T, t->prio);
    }
    printf("+------------+------+------+------+----------+\n");
}
