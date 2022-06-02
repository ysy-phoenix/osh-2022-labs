.PHONY: clean
CC = gcc
RM = rm
CCFLAGS ?= -Wall -O2 -D_GNU_SOURCE -luring
1: 1.c
	$(CC) 1.c -o 1 -lpthread
2: 2.c
	$(CC) 2.c -o 2 -lpthread
3: 3.c
	$(CC) 3.c -o 3 
4: 4.c
	$(CC) 3.c -o 4 
5: 5.c
	$(CC) 5.c -o 5  ${CCFLAGS}
clean:
	$(RM) 1 2 3 4 5