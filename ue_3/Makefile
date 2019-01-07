#
# Exercise 3
# Program: 
# Author: Markus Klein (e11707252@student.tuwien.ac.at)
#
CC = gcc
DEFS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS = -Wall -g -std=c99 -pedantic $(DEFS)
LDFLAGS = -pthread -lrt

SRC_PATH = src
COMMON_OBJECTS = common.o
GENERATOR_OBJECTS = $(COMMON_OBJECTS) generator.o
SUPERVISOR_OBJECTS = $(COMMON_OBJECTS) supervisor.o

.PHONY: all clean
all: generator supervisor

generator: $(GENERATOR_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

supervisor: $(SUPERVISOR_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: $(SRC_PATH)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

generator.o: $(SRC_PATH)/generator.c $(SRC_PATH)/common.h
supervisor.o: $(SRC_PATH)/supervisor.c $(SRC_PATH)/common.h
common.o: $(SRC_PATH)/common.c $(SRC_PATH)/common.h

clean:
	rm -rf *.o generator supervisor