FROM golang:1.18.0-bullseye as server_builder

COPY ./ /src/

RUN cd /src/ && make

FROM ubuntu:20.04 as server

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update -y && apt-get upgrade -y && apt-get install nginx supervisor -y

WORKDIR /server
COPY --from=server_builder /src/server /server/server
COPY supervisord.conf nginx_default /server/
RUN mv /server/nginx_default /etc/nginx/sites-enabled/default && adduser --no-create-home --disabled-password --gecos "" user
COPY static/ /static/

CMD ["supervisord","-c","/server/supervisord.conf"]
