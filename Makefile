CC     = cc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -O2
OBJS   = main.o tache.o analyse.o

ordo: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lm

%.o: %.c tache.h analyse.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f ordo *.o

.PHONY: clean
