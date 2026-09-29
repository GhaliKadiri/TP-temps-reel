#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include "tache.h"

#define LIGNE_MAX 256

/* Lit un entier à partir de *p (espaces initiaux ignorés), vérifie qu'il est
 * suivi d'un espace ou de la fin de ligne et qu'il est dans [min, max].
 * Avance *p après le nombre. Retourne 0 si tout va bien, -1 sinon. */
static int lire_champ_entier(char **p, long min, long max, long *val)
{
    char *fin;
    errno = 0;
    long v = strtol(*p, &fin, 10);
    if (fin == *p || errno == ERANGE || v < min || v > max) return -1;
    if (*fin != '\0' && !isspace((unsigned char)*fin) && *fin != '#') return -1;
    *p = fin;
    *val = v;
    return 0;
}

int lire_taches(const char *chemin, Ensemble *e)
{
    FILE *f = fopen(chemin, "r");
    if (f == NULL) {
        perror(chemin);
        return -1;
    }

    char ligne[LIGNE_MAX];
    int num_ligne = 0;
    e->n = 0;

    while (fgets(ligne, sizeof ligne, f) != NULL) {
        num_ligne++;

        /* ligne plus longue que le tampon : fgets l'aurait coupée en deux */
        if (strchr(ligne, '\n') == NULL && !feof(f)) {
            fprintf(stderr, "%s:%d : ligne trop longue (%d caractères max)\n",
                    chemin, num_ligne, LIGNE_MAX - 2);
            fclose(f);
            return -1;
        }

        /* ignorer les espaces en début de ligne, les lignes vides et les commentaires */
        char *p = ligne;
        while (isspace((unsigned char)*p)) p++;
        if (*p == '\0' || *p == '#') continue;

        if (e->n >= TACHES_MAX) {
            fprintf(stderr, "Erreur : plus de %d tâches\n", TACHES_MAX);
            fclose(f);
            return -1;
        }

        Tache *t = &e->t[e->n];

        /* 1er champ : le nom (jusqu'au premier espace) */
        char *debut_nom = p;
        while (*p != '\0' && !isspace((unsigned char)*p)) p++;
        size_t lg_nom = (size_t)(p - debut_nom);
        if (lg_nom >= NOM_MAX) {
            fprintf(stderr, "%s:%d : nom \"%.*s\" trop long (%d caractères max)\n",
                    chemin, num_ligne, (int)lg_nom, debut_nom, NOM_MAX - 1);
            fclose(f);
            return -1;
        }
        memcpy(t->nom, debut_nom, lg_nom);
        t->nom[lg_nom] = '\0';

        /* 4 champs entiers, lus avec strtol et contrôle de plage
         * (sscanf("%d") a un comportement indéfini en cas de dépassement) */
        long v[4];
        static const long mini[4] = { 1, 1, 1, INT_MIN };
        static const long maxi[4] = { VALEUR_MAX, VALEUR_MAX, VALEUR_MAX, INT_MAX };
        for (int k = 0; k < 4; k++) {
            if (lire_champ_entier(&p, mini[k], maxi[k], &v[k]) != 0) {
                fprintf(stderr, "%s:%d : format attendu \"nom C D T prio\" "
                        "(C, D, T entiers de 1 à %ld, prio entier)\n",
                        chemin, num_ligne, VALEUR_MAX);
                fclose(f);
                return -1;
            }
        }
        t->C = (int)v[0]; t->D = (int)v[1]; t->T = (int)v[2]; t->prio = (int)v[3];
        t->S = 0;   /* toutes les tâches lues sont activées à t = 0 */

        /* après les 5 champs, seul un commentaire est accepté */
        while (isspace((unsigned char)*p)) p++;
        if (*p != '\0' && *p != '#') {
            fprintf(stderr, "%s:%d : champ en trop après \"nom C D T prio\"\n", chemin, num_ligne);
            fclose(f);
            return -1;
        }
        /* contrôles de cohérence du modèle de tâche (C, D, T > 0 : vu ci-dessus) */
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
