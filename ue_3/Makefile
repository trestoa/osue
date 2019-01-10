#
# Exercise 3
# Program: 3-coloring
# Author: Markus Klein (e11707252@student.tuwien.ac.at)
#
CC = gcc
DEFS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS = -Wall -g -std=c99 -pedantic $(DEFS)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	LDFLAGS = -pthread
else
	LDFLAGS = -pthread -lrt
endif


SRC_PATH = src
GENERATOR_OBJECTS = $(COMMON_OBJECTS) generator.o
SUPERVISOR_OBJECTS = $(COMMON_OBJECTS) supervisor.o

.PHONY: all clean
all: generator supervisor

generator: $(GENERATOR_OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

supervisor: $(SUPERVISOR_OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: $(SRC_PATH)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

generator.o: $(SRC_PATH)/generator.c $(SRC_PATH)/common.h
supervisor.o: $(SRC_PATH)/supervisor.c $(SRC_PATH)/common.h

clean:
	rm -rf *.o generator supervisor