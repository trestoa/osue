#
# Exercise 1b
# Program: HTTP Client and Server
# Author: Markus Klein (e11707252@student.tuwien.ac.at)
#
CC = gcc
DEFS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS = -Wall -g -std=c99 -pedantic $(DEFS)

SRC_PATH = src
COMMON_OBJECTS = http.o
CLIENT_OBJECTS = $(COMMON_OBJECTS) client.o
SERVER_OBJECTS = $(COMMON_OBJECTS)

.PHONY: all clean
all: client

client: $(CLIENT_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

server: $(SERVER_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: $(SRC_PATH)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

client.o: $(SRC_PATH)/client.c $(SRC_PATH)/utils.h $(SRC_PATH)/http.h
http.o: $(SRC_PATH)/http.c $(SRC_PATH)/http.h

clean:
	rm -rf *.o client server