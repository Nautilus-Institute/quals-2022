FROM ubuntu:20.04 as builder

# install dependencies
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get -y upgrade && \
    apt-get -y install --no-install-recommends gcc make libc6-dev
RUN apt-get clean
RUN rm -rf /var/lib/apt/lists/*

WORKDIR /src
ADD Makefile *.c /src/

RUN make
RUN mkdir dist && cp simple-service dist && tar --numeric-owner -c -f simple-service_dist.tar.gz -v -z -h dist


FROM ubuntu:20.04 as challenge

WORKDIR /challenge
COPY --from=builder /src/simple-service /src/simple-service_dist.tar.gz /challenge/
COPY flag /challenge/

RUN adduser --no-create-home --disabled-password --gecos "" user
USER user

CMD ["/challenge/simple-service"]
