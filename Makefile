CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic

.PHONY: all, run, test, archive, clean

all: proj2

proj2: proj2.o
	$(CC) $(CFLAGS) $^ -o $@ -pthread -g

proj2.o: proj2.c
	$(CC) $(CFLAGS) -c $^

run:
	make all
	./proj2 3 5 100 100

archive:
	zip proj2.zip Makefile *.h *.c
clean:
	rm -f *.o *.gch proj2