FROM golang:1.18.0-bullseye as launcher_builder

COPY go.mod go.sum Makefile /src/
RUN cd /src/ && go mod download -x

COPY ./src/ /src/src/
RUN cd /src/ && make

FROM ubuntu:20.04 as launcher

ARG DEBIAN_FRONTEND=noninteractive
#RUN apt-get update -y && apt-get upgrade -y
RUN apt-get update -y && apt-get install -y curl

COPY --from=launcher_builder /src/launcher /launcher

entrypoint ["/launcher"]
