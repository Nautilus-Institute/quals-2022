

CC=/toolchains/x86_64-linux-musl/bin/gcc
TOOLCHAIN_DIR=/toolchains/x86_64-linux-musl/
TOOLCHAIN_LIB=$(TOOLCHAIN_DIR)/lib
TOOLCHAIN_INC=$(TOOLCHAIN_DIR)/include
TOOLCHAIN_BIN=$(TOOLCHAIN_DIR)/bin

SERVER_TARGET = ctfserver
CLIENT_TARGET = ctfclient

CFLAGS=-I${TOOLCHAIN_INC} -fstack-protector  -fPIC -Wall -Os -maes -mrdrnd -m64 -mtune=intel -msse4.1
CFLAGS_CLIENT=-I${TOOLCHAIN_INC} -fPIC -Wall -mss4.1

#DEBUG_FLAGS = -g -DDEBUG
#DEBUG_FLAGS = 
SOURCE=./src/
SOLVER=./solver/
INCLUDE=./include/

#CFLAGS+= ${DEBUG_FLAGS}
CFLAGS_CLIENT+= ${DEBUG_FLAGS}

LD_FLAGS=--static -L${TOOLCHAIN_LIB} -lssl -lcrypto

all: ctfserver ctfclient	


ctfserver: $(SOURCE)ctf_server.c $(SOURCE)helpers.c 
	$(CC) ${CFLAGS} -I$(INCLUDE) -o ${SERVER_TARGET} $(SOURCE)ctf_server.c $(SOURCE)helpers.c ${LD_FLAGS} 

ctfclient: $(SOLVER)ctf_client.c $(SOURCE)helpers.c
	$(CC) ${CFLAGS} -I$(INCLUDE) -o ${CLIENT_TARGET} $(SOLVER)ctf_client.c $(SOURCE)helpers.c ${LD_FLAGS} 


.PHONY: clean

clean:
	-rm -f ctfclient ctfserver
