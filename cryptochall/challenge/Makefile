
CC=g++
CFLAGS= -Wall -fno-rtti
CFLAGS= -Wall
LDFLAGS=
SRCS=$(wildcard *.cpp) $(wildcard *.h)

BIN=cryptochall

build: $(BIN)

.PHONY: all clean

$(BIN): $(SRCS)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	strip --strip-all $(BIN)


clean: 
	rm $(BIN)
