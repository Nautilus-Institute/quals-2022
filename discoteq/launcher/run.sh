#!/bin/bash

set -x
docker run -v /var/run/docker.sock:/var/run/docker.sock -v $(pwd)/launchersock:/launchersock --rm -it discoteq-launcher
