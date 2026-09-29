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

clean:
	rm -f ordo *.o

.PHONY: clean exo1 exo2
