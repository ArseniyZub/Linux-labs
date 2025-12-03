FROM debian

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
	gcc make libfuse3-dev libreadline-dev

WORKDIR /opt

COPY . .

RUN make deb

CMD ["make", "deb"]