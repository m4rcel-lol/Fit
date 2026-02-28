FROM alpine:latest AS builder

RUN apk add --no-cache gcc musl-dev make zlib-dev openssl-dev sqlite-dev

WORKDIR /build
COPY . .

RUN make clean && make

FROM alpine:latest

RUN apk add --no-cache zlib openssl sqlite-libs

COPY --from=builder /build/bin/fit /usr/local/bin/fit
COPY --from=builder /build/bin/fitweb /usr/local/bin/fitweb

RUN mkdir -p /data && \
    adduser -D -h /data fit

USER fit
WORKDIR /data

EXPOSE 9418 8080

CMD ["/bin/sh", "-c", "fit daemon --port 9418 & fitweb"]
