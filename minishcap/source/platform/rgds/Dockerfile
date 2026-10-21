# Native arm64 build environment for the RG DS (ROCKNIX, glibc 2.41 == Debian trixie).
FROM --platform=linux/arm64 debian:trixie
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential make python3 libsdl2-dev libasound2-dev ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /src
