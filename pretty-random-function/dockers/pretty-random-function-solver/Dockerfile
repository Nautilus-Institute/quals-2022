# WE ARE NOT USING THIS DOCKER FOR BUILDING DISTRIBUTABLE THINGS
#
# The purpose of this docker image is just to specify an environment for
# building the challenge. The binary in the repository is the definitive
# challenge binary.

FROM debian:bullseye AS builder

COPY ctfclient /opt/ctfclient

RUN adduser --no-create-home --disabled-password --gecos "" user
USER user

CMD /opt/ctfclient