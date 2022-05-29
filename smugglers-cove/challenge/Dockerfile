FROM ubuntu:20.04 as builder
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get -y upgrade && \
    apt-get -y install --no-install-recommends gcc make libc6-dev git ca-certificates

WORKDIR /src
RUN git clone https://github.com/LuaJIT/LuaJIT --depth=1 /LuaJIT && cd /LuaJIT && make install -j $(nproc) && cp /usr/local/lib/libluajit-5.1.so.2 /src/.

ADD Makefile *.c /src/

RUN make all 

FROM ubuntu:20.04 as challenge
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get upgrade -y && apt-get install curl -y && apt list --installed > /packages.txt
RUN date -u > /packages_updated.txt

WORKDIR /challenge
COPY --from=builder /src/libluajit-5.1.so.2 /usr/local/lib/libluajit-5.1.so.2
COPY --from=builder /src/cove /src/dig_up_the_loot /challenge/
RUN ldconfig && chmod 111 /challenge/dig_up_the_loot
RUN adduser --no-create-home --disabled-password --gecos "" user

USER user

CMD ["/challenge/cove"]
