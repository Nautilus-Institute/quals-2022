FROM ubuntu:22.04 as solver

# install dependencies
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get -y upgrade
RUN apt-get -y install --no-install-recommends python3 
RUN apt-get -y install python3-pip python3-dev git libssl-dev libffi-dev build-essential
RUN apt-get -y install python-is-python3
RUN python3 -m pip install --upgrade pip
RUN python3 -m pip install --upgrade pwntools
RUN apt-get -y install sagemath
RUN apt-get clean
RUN rm -rf /var/lib/apt/lists/*



WORKDIR /solver
ADD x.py /solver

CMD ["/usr/bin/python3", "x.py"]
