#!/usr/bin/env bash
# Builds cima-companion/build/cima-bottom (aarch64 Linux) in a native arm64 Debian trixie container.
set -euo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
docker build -q --platform linux/arm64 -t cima-companion-build "$D" >/dev/null
docker run --rm --platform linux/arm64 -v "$D":/src -w /src cima-companion-build make "$@"
