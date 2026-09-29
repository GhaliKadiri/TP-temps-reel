CC     = cc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -O2
OBJS   = main.o tache.o analyse.o simulateur.o
HDRS   = tache.h analyse.h simulateur.h

ordo: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lm

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

# Démonstrations
exo1: ordo
	./ordo exemples/taches_tp.txt -p tout

exo2: ordo
	./ordo exemples/taches_tp.txt -p edf -v -a 40

# Tests : exemples du cours (non-régression) + 300 jeux aléatoires
test: ordo
	sh tests/test_exemples.sh
	python3 tests/test_aleatoire.py 300

# Regénère les sorties de référence ; la 1re ligne de chaque fichier
# indique la commande qui l'a produit.
RESULTATS = \
	"exo1_taches_tp_preemptif.txt:exemples/taches_tp.txt -v" \
	"exo1_taches_tp_non_preemptif.txt:exemples/taches_tp.txt -n -v -a 40" \
	"exo1_cours_optimalite_rm_vs_dm.txt:exemples/cours_optimalite.txt" \
	"exo1_non_preemptif.txt:exemples/non_preemptif.txt -n -a 24" \
	"exo2_edf_taches_tp.txt:exemples/taches_tp.txt -p edf -v -a 40" \
	"exo2_edf_cours_p134.txt:exemples/cours_edf_p134.txt -p edf -v -a 28"

resultats: ordo
	@mkdir -p resultats
	@for r in $(RESULTATS); do \
		f=$${r%%:*}; args=$${r#*:}; \
		echo "\$$ ./ordo $$args" > resultats/$$f; \
		./ordo $$args >> resultats/$$f; \
		echo "resultats/$$f  <-  ./ordo $$args"; \
	done

clean:
	rm -f ordo *.o

.PHONY: clean exo1 exo2 test resultats
