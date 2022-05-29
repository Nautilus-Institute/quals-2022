#!/bin/bash

set -x
docker run -p 8080:80 \
    --name discoteq-server \
    -v $(realpath ../launcher/launchersock):/launchersock \
    --rm -it discoteq-server
