FROM debian:bullseye AS builder

RUN apt-get update && \
    apt-get install -y \
        build-essential \
        libssl-dev

COPY src /root/src

WORKDIR /root/src

RUN ls -lh
RUN ./build.sh

FROM debian:bullseye

RUN apt-get update && \
    apt-get install -y libssl1.1 && \
    apt-get clean

COPY --from=builder /root/src/challenge /opt/challenge

CMD /opt/challenge