#!/bin/bash

FILE=$(basename $1)
LUA_PATH=/$(head -c10 /dev/urandom | xxd -p)/$FILE

./build.sh
docker run -v $(realpath $1):$LUA_PATH --rm -it smugglers-cove-challenge $LUA_PATH
