FROM ubuntu:20.04 as builder

# install dependencies
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get -y upgrade
RUN apt-get -y install --no-install-recommends build-essential python3-dev python3-pip
RUN apt-get -y install --no-install-recommends libffi-dev zlib1g-dev \
      liblzma-dev libreadline-dev libssl-dev libsqlite3-dev libgdbm-dev \
      libuuid1 git g++ libbz2-dev
RUN apt-get clean
RUN rm -rf /var/lib/apt/lists/*

RUN mkdir /src
ADD portable-python/ /src/portable-python/
ADD cpython/ /src/cpython/
# Clean up test folders
RUN rm -rf /src/cpython/build/
RUN rm -rf /src/cpython/build_original/

# Build customized Python
WORKDIR /src/
RUN cd ./portable-python && python3 setup.py install
ARG CPYTHON_SRC_ROOT=/src/cpython/
RUN portable-python build 3.12.0

# Compile os.py
RUN cd ./dist/ && tar -xzf cpython-3.12.0-linux-x86_64.tar.gz
RUN ./dist/3.12.0/bin/python -m py_compile ./dist/3.12.0/lib/python3.12/os.py
RUN mkdir -p ./dist/slim/bin
RUN mkdir -p ./dist/slim/lib/python3.12
RUN cp ./dist/3.12.0/bin/python3.12 ./dist/slim/bin/python
RUN strip --strip-all ./dist/slim/bin/python
RUN cp ./dist/3.12.0/lib/python3.12/__pycache__/os.cpython-312.pyc ./dist/slim/lib/python3.12/os.pyc
RUN cp -r ./dist/3.12.0/lib/python3.12/encodings ./dist/slim/lib/python3.12/encodings

# Compile chall.py
ADD chall.py /src/
RUN ./dist/3.12.0/bin/python3.12 -m py_compile chall.py
RUN mv __pycache__/chall.cpython-312.pyc chall.pyc



FROM ubuntu:20.04 as challenge

ARG DEBIAN_FRONTEND=noninteractive

WORKDIR /challenge
COPY --from=builder /src/chall.pyc /src/dist/slim /challenge/
RUN tar -czf adamd_dist.tar.gz *

CMD ["bash"]
