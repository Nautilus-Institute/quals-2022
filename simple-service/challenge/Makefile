CC=gcc
CFLAGS=-Wall
LDFLAGS=
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

BIN=simple-service

build: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
