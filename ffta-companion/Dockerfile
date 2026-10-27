# arm64 Debian trixie (same glibc as ROCKNIX) build environment for ffta-bottom.
FROM --platform=linux/arm64 debian:trixie
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential make libsdl2-dev libsdl2-ttf-dev ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /src
