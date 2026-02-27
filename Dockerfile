FROM alpine:latest AS builder

RUN apk add --no-cache gcc musl-dev make zlib-dev openssl-dev

WORKDIR /build
COPY . .

RUN make clean && make

FROM alpine:latest

RUN apk add --no-cache zlib openssl

COPY --from=builder /build/bin/fit /usr/local/bin/fit

RUN mkdir -p /data && \
    adduser -D -h /data fit

USER fit
WORKDIR /data

EXPOSE 9418

CMD ["fit", "daemon", "--port", "9418"]
