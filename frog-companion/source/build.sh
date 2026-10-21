#!/usr/bin/env bash
# Builds build/frog-bottom (aarch64 Linux, uses the device's own SDL2) in a native arm64 Debian trixie container.
# Reuses the image from the Minish Cap port (platform/rgds/Dockerfile): gcc + libsdl2-dev, glibc 2.41 like ROCKNIX.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
docker image inspect tmc-rgds-build >/dev/null 2>&1 || docker build -q --platform linux/arm64 -t tmc-rgds-build - <<'DOCKERFILE'
FROM --platform=linux/arm64 debian:trixie
RUN apt-get update && apt-get install -y --no-install-recommends build-essential make libsdl2-dev ca-certificates && rm -rf /var/lib/apt/lists/*
DOCKERFILE
mkdir -p build
docker run --rm --platform linux/arm64 -v "$PWD":/work -w /work tmc-rgds-build \
  gcc -std=gnu11 -O2 -Wall -Wno-unused-result frog_bottom.c libretro_host.c ra_client.c \
      $(docker run --rm --platform linux/arm64 tmc-rgds-build sdl2-config --cflags) \
      -o build/frog-bottom -lSDL2 -ldl -lm
ls -la build/frog-bottom
