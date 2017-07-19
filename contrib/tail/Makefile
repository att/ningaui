# 
# Date: 05 Oct 2005

CC=gcc
CFLAGS=
IFLAGS= -I.

all: tac


clean:
	rm -f *.o tac


# tail with the -r option as default (no forward ability at all) so that it works on linux too
tac:	reverse.o tac.o misc.o read.o
	$(CC) $(IFLAGS) $(CFLAGS) -o tac tac.o reverse.c misc.c read.o 
