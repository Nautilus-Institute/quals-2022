FROM ubuntu:20.04 as builder

# install dependencies
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get -y upgrade && \
    apt-get -y install --no-install-recommends gcc make libc6-dev g++ libboost-all-dev cmake python3
RUN apt-get clean
RUN rm -rf /var/lib/apt/lists/*

RUN mkdir /src
ADD src/ /src/

# build server
WORKDIR /src/build
RUN rm -f CMakeCache.txt
RUN cmake /src
RUN make
RUN strip --strip-all server
RUN tar --numeric-owner -c -f router_dist.tar.gz -v -z -h web/

# build vm
RUN mkdir /vm
ADD vm/ /vm/
WORKDIR /vm
RUN make


FROM ubuntu:20.04 as challenge

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get -y upgrade
RUN apt-get -y install --no-install-recommends libboost-filesystem1.71.0 supervisor iputils-ping

WORKDIR /challenge
ADD supervisord.conf /challenge/
COPY --from=builder /src/build/server /src/build/router_dist.tar.gz /challenge/
COPY --from=builder /vm/vm /challenge/web/
COPY flag3 /
RUN tar -xvzf router_dist.tar.gz
RUN adduser --no-create-home --disabled-password --gecos "" user

CMD ["supervisord", "-c", "/challenge/supervisord.conf"]
