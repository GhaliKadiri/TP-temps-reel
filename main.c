#include <stdio.h>
#include "tache.h"
#include "analyse.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage : %s <fichier_taches>\n", argv[0]);
        return 1;
    }

    Ensemble e;
    if (lire_taches(argv[1], &e) != 0)
        return 1;

    printf("%d tâche(s) lue(s) depuis %s :\n", e.n, argv[1]);
    afficher_taches(&e);

    analyser(&e, HPF);
    return 0;
}
