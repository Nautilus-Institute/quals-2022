FROM discoteq-challenge as challenge_builder
FROM golang:1.18.0-bullseye as server_builder

COPY go* Makefile /src/
COPY src /src/src

RUN cd /src/ && make

FROM ubuntu:20.04 as server

RUN apt-get update -y && apt-get install nginx supervisor curl -y

COPY --from=server_builder /src/server /server/server
COPY --from=challenge_builder /server/static/ /server/static
COPY supervisord.conf nginx_default /
RUN mv /nginx_default /etc/nginx/sites-enabled/default && adduser --no-create-home --disabled-password --gecos "" user
COPY widgets/ /server/static/widget

CMD ["supervisord","-c","/supervisord.conf"]
