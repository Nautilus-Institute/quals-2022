#!/bin/bash

rm ctfserver ctfclient

gcc -g -fstack-protector -fsanitize=address -fPIC -Wall -Os -maes -mrdrnd -m64 -msse4.1 -DDEBUG -o ctfserver ctf_server.c helpers.c   -lssl -lcrypto -ldl -lpthread
gcc -g -fstack-protector -fsanitize=address -fPIC  -Os -maes -mrdrnd -m64 -msse4.1 -DDEBUG -o ctfclient ctf_client.c helpers.c   -lssl -lcrypto -ldl -lpthread
