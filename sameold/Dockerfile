ARG PYTHON_VERSION=3.7

FROM python:${PYTHON_VERSION}

RUN apt-get update && apt-get install -y \
    python3-pip \
    python3-venv \
    python3-dev \
    python3-setuptools \
    python3-wheel

RUN mkdir -p /app
WORKDIR /app

RUN pip install flask gunicorn

COPY . .

EXPOSE 8080

CMD ["gunicorn", "--bind", ":8080", "--workers", "2", "check:app"]
