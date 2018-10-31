#
# Exercise 1b
# Program: HTTP Client and Server
# Author: Markus Klein (e11707252@student.tuwien.ac.at)
#
CC = gcc
DEFS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS = -Wall -g -std=c99 -pedantic $(DEFS)

SRC_PATH = src
COMMON_OBJECTS = 
CLIENT_OBJECTS = $(COMMON_OBJECTS)
SERVER_OBJECTS = $(COMMON_OBJECTS)

.PHONY: all clean
all: client server

client: $(CLIENT_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

server: $(SERVER_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: $(SRC_PATH)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<


clean:
	rm -rf *.o client server