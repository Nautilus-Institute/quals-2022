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

COPY . .

#RUN python manage.py collectstatic --noinput


EXPOSE 8080

# replace APP_NAME with module name
CMD ["python", "post_it_notes.py", "8080"]
