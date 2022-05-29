FROM ubuntu:22.04 as builder

# install dependencies
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get -y upgrade && \
    apt-get -y install --no-install-recommends g++ make libc6-dev
RUN apt-get clean
RUN rm -rf /var/lib/apt/lists/*

WORKDIR /src
ADD Makefile *.cpp *h /src/

RUN make
RUN mkdir dist && cp cryptochall dist && tar --numeric-owner -c -f cryptochall_dist.tar.gz -v -z -h dist


FROM ubuntu:22.04 as challenge

WORKDIR /perribus/challenge
COPY --from=builder /src/cryptochall /src/cryptochall_dist.tar.gz /perribus/challenge/
COPY flag.txt /perribus/challenge/

RUN adduser --no-create-home --disabled-password --gecos "" user
USER user

CMD ["/perribus/challenge/cryptochall"]