FROM simple-service-challenge

EXPOSE 31337

# install dependencies
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get -y upgrade
RUN apt-get -y install socat
RUN apt-get clean
RUN rm -rf /var/lib/apt/lists/*

ENV TIMEOUT=60
CMD ["/bin/sh", "-c", "/usr/bin/socat", "tcp4-listen:31337", "exec:/challenge/simple-service"]
