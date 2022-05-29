FROM ubuntu:20.04 as builder

WORKDIR /build/
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update -y && apt-get install -y wget curl git zip xz-utils make nodejs npm cmake clang cmake ninja-build pkg-config libgtk-3-dev liblzma-dev && npm i -g terser html-minifier && wget -q https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_2.10.4-stable.tar.xz && tar xf flutter_linux_2.10.4-stable.tar.xz
RUN git config --global --add safe.directory /build/flutter

ENV PATH="/build/flutter/bin:${PATH}"

ADD pubspec.lock pubspec.yaml Makefile /app/
ADD lib /app/lib
ADD web /app/web
ADD linux /app/linux
ADD assets /app/assets
ADD html_src /app/html_src
ADD rfw-1.0.2-web-patch /app/rfw-1.0.2-web-patch
RUN ls /app/ && cd /app/ && make all

FROM ubuntu:20.04 as challenge
RUN apt-get update -y && apt-get install -y xvfb libegl1 libgtk-3-0
COPY --from=builder /app/build/linux/x64/release/bundle/ /discoteq
COPY --from=builder /app/build/web /server/static
COPY --from=builder /app/discoteq_desktop.zip /server/static/.
COPY run.sh /.
COPY responses.txt /.

CMD ["/bin/bash"]
