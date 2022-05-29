# WE ARE NOT USING THIS DOCKER FOR BUILDING DISTRIBUTABLE THINGS
#
# The purpose of this docker image is just to specify an environment for
# building the challenge. The binary in the repository is the definitive
# challenge binary.

FROM debian:bullseye AS builder

RUN apt-get update -y && \
    apt-get install -y build-essential && \
    apt-cache clean

COPY toolchain.tgz /tmp
RUN tar -C / /tmp/toolchain.tgz

RUN mkdir /prf

COPY build.sh /prf
COPY include /prf
COPY solver /prf
COPY src /prf

WORKDIR /prf

RUN /prf/build.sh

FROM debian:bullseye

COPY --from=builder /prf/ctfserver /opt/ctfserver
COPY --from=builder /prf/ctfclient /opt/ctfclient