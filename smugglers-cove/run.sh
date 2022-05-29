#!/bin/bash

set -e

(cd ./challenge && ./build.sh)
(cd ./server && ./build.sh)
(cd ./launcher && ./build.sh)
docker-compose -f ./docker-compose.yml rm -f
docker-compose -f ./docker-compose.yml up
