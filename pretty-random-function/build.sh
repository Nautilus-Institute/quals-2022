#!/bin/bash

rm -f ctfserver
rm -f ctfclient

SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )";

TOOLCHAIN_DIR="/toolchains/x86_64-linux-musl/"
TOOLCHAIN_LIB="$TOOLCHAIN_DIR/lib"
TOOLCHAIN_INC="$TOOLCHAIN_DIR/include"
TOOLCHAIN_BIN="$TOOLCHAIN_DIR/bin"
source /toolchains/x86_64-linux-musl/activate-musl-toolchain.env

INCLUDE_DIR="$SCRIPT_DIR/include"

#CFLAGS="-I${TOOLCHAIN_INC} -fPIC"
# CFLAGS="-I${TOOLCHAIN_INC} -I${INCLUDE_DIR} -fPIC -DDEBUG -Wall"
CFLAGS="-I${TOOLCHAIN_INC} -I${INCLUDE_DIR} -fstack-protector  -fPIC -Wall -Os -maes -mrdrnd -m64 -mtune=intel -msse4.1 -DDEBUG"
CFLAGS_CLIENT="-I${TOOLCHAIN_INC} -I${INCLUDE_DIR} -fPIC -DDEBUG"
LD_FLAGS="--static -L${TOOLCHAIN_LIB} -lssl -lcrypto"

CHALLENGE_CMD="gcc ${CFLAGS} -o ctfserver src/ctf_server.c src/helpers.c ${LD_FLAGS}"
SOLVER_CMD="gcc ${CFLAGS_CLIENT} -o ctfclient solver/ctf_client.c src/helpers.c ${LD_FLAGS}"

echo $CHALLENGE_CMD
$CHALLENGE_CMD

echo $SOLVER_CMD
$SOLVER_CMD
