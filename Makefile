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

# Compile une version instrumentée (ASan + UBSan) et l'exécute sur tous les
# exemples avec plusieurs jeux d'options ; échoue au moindre message d'erreur.
SAN_OPTIONS_LISTE = "-v -a 200" "-n -v -a 200" "-p edf -d 20" "-d 7"

sanitize:
	$(CC) $(CFLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer \
		-o ordo_san main.c tache.c analyse.c simulateur.c -lm
	@err=0; for f in exemples/*.txt; do for o in $(SAN_OPTIONS_LISTE); do \
		ASAN_OPTIONS=detect_leaks=0 ./ordo_san $$f $$o > /dev/null 2> ordo_san.err || err=1; \
		if [ -s ordo_san.err ]; then echo "ERREUR : $$f $$o"; cat ordo_san.err; err=1; fi; \
	done; done; rm -f ordo_san.err; \
	if [ $$err -eq 0 ]; then echo "sanitize : aucune erreur mémoire ni comportement indéfini"; fi; \
	exit $$err

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
	rm -rf ordo ordo_san ordo_san.err ordo_san.dSYM *.o

.PHONY: clean exo1 exo2 test sanitize resultats
