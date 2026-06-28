# Stage 1: Build cramfs tools on Ubuntu 16.04 (zlib 1.2.8)
FROM ubuntu:16.04 AS builder
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates git build-essential zlib1g-dev \
    && git clone --depth 1 https://github.com/npitre/cramfs-tools.git /tmp/ct \
    && cd /tmp/ct && make LDFLAGS="-static" \
    && rm -rf /var/lib/apt/lists/*

# Stage 2: Production image with crosstool + cramfs tools
FROM ghcr.io/duhow/wibox-crosstool:latest
COPY --from=builder /tmp/ct/mkcramfs /tmp/ct/cramfsck /usr/local/bin/
RUN chmod +x /usr/local/bin/mkcramfs /usr/local/bin/cramfsck \
    && apt-get update && apt-get install -y --no-install-recommends rsync wget bzip2 \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /build
