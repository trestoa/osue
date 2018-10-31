#
# Exercise 1a
# Program: mydiff
# Author: Markus Klein (e11707252@student.tuwien.ac.at)
#
CC = gcc
DEFS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS = -Wall -g -std=c99 -pedantic $(DEFS)

SRC_PATH = src
OBJECTS = main.o mydiff.o

.PHONY: all clean
all: mydiff

mydiff: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: $(SRC_PATH)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

main.o: $(SRC_PATH)/main.c
mydiff.o: $(SRC_PATH)/mydiff.c $(SRC_PATH)/mydiff.h

clean:
	rm -rf *.o mydiff