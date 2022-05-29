#!/bin/bash

CODE_DIR=./code
CODE_DIR=$(realpath $CODE_DIR)

set -x
./build.sh
docker run -v $CODE_DIR:/code -v /var/run/docker.sock:/var/run/docker.sock -v $(pwd)/launchersock:/launchersock --rm -it smugglers-cove-launcher -volume $CODE_DIR
