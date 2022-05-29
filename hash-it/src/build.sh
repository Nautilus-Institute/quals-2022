#!/bin/bash

# I know I am a terrible person. I will be a less terrible person later.

gcc main.c -O2 -lssl -lcrypto -o challenge
strip challenge