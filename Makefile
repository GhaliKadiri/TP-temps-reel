CC     = cc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -O2
OBJS   = main.o tache.o

ordo: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c tache.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f ordo *.o

.PHONY: clean
