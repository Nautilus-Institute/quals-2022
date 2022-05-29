#!/bin/bash

set -e
(cd discoteq && ./build.sh)
(cd server && ./build.sh)
(cd launcher && ./build.sh)
docker-compose -f ./docker-compose.yml.local rm -f
docker-compose -f ./docker-compose.yml.local up --remove-orphans
