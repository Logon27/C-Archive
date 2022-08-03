C=gcc
CFLAGS= -std=c11 -Wall -pedantic
OBJECT1= ctar.o
OBJECT2= utar.o

all: ctar utar
ctar: $(OBJECT1)
	$(CC) $(CFLAGS) -o ctar $(OBJECT1)
utar: $(OBJECT2)
	$(CC) $(CFLAGS) -o utar $(OBJECT2)
ctar.o: ctar.c
	$(CC) -c $(CFLAGS) ctar.c
utar.o: utar.c
	$(CC) -c $(CFLAGS) utar.c

.PHONY: clean
clean:
	rm *.o ctar utar
