#
# Exercise 2
# Program: intmul
# Author: Markus Klein (e11707252@student.tuwien.ac.at)
#
CC = gcc
DEFS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS = -Wall -g -std=c99 -pedantic $(DEFS)

OBJECTS = intmul.o

.PHONY: all clean
all: intmul

intmul: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

intmul.o: intmul.c

clean:
	rm -rf *.o intmul