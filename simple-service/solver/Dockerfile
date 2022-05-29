FROM ubuntu:20.04 as solver

# install dependencies
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get -y upgrade
RUN apt-get -y install --no-install-recommends python3
RUN apt-get clean
RUN rm -rf /var/lib/apt/lists/*

WORKDIR /solver
ADD solve.py /solver

CMD ["/usr/bin/python3", "solve.py"]
