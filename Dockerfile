ARG BASE_IMAGE=wibox-build:latest
ARG CRAMFS_TOOLS_COMMIT=a95eecbf899479f8b3d65fb2822c5b7cbcdacfdd

# Stage 1: Build cramfs tools on Ubuntu 16.04 (zlib 1.2.8)
FROM ubuntu:16.04 AS builder
ARG CRAMFS_TOOLS_COMMIT
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates git build-essential zlib1g-dev \
    && git clone https://github.com/npitre/cramfs-tools.git /tmp/ct \
    && cd /tmp/ct \
    && git checkout "${CRAMFS_TOOLS_COMMIT}" \
    && make LDFLAGS="-static" \
    && rm -rf /var/lib/apt/lists/*

# Stage 2: Production build image: ARM toolchain, PJProject and cramfs tools.
FROM ${BASE_IMAGE}
COPY --from=builder /tmp/ct/mkcramfs /tmp/ct/cramfsck /usr/local/bin/
ENV PATH="/opt/4.6.1/usr/bin:${PATH}"
RUN sed -i \
      -e 's|^# deb http://snapshot.debian.org/archive/debian/20260610T000000Z bullseye main|deb http://snapshot.debian.org/archive/debian/20260610T000000Z bullseye main|' \
      -e 's|^deb http://deb.debian.org/debian bullseye main|# deb http://deb.debian.org/debian bullseye main|' \
      -e 's|^# deb http://snapshot.debian.org/archive/debian-security/20260610T000000Z bullseye-security main|deb http://snapshot.debian.org/archive/debian-security/20260610T000000Z bullseye-security main|' \
      -e 's|^deb http://deb.debian.org/debian-security bullseye-security main|# deb http://deb.debian.org/debian-security bullseye-security main|' \
      -e 's|^# deb http://snapshot.debian.org/archive/debian/20260610T000000Z bullseye-updates main|deb http://snapshot.debian.org/archive/debian/20260610T000000Z bullseye-updates main|' \
      -e 's|^deb http://deb.debian.org/debian bullseye-updates main|# deb http://deb.debian.org/debian bullseye-updates main|' \
      /etc/apt/sources.list \
    && chmod +x /usr/local/bin/mkcramfs /usr/local/bin/cramfsck \
    && apt-get -o Acquire::Check-Valid-Until=false update \
    && apt-get install -y --no-install-recommends rsync wget bzip2 sshpass \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /build
