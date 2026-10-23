#!/usr/bin/env bash
# Builds fusion-companion/build/mf-bottom (aarch64 Linux) in a native arm64 Debian trixie container.
set -euo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
docker build -q --platform linux/arm64 -t mf-companion-build "$D" >/dev/null
docker run --rm --platform linux/arm64 -v "$D":/src -w /src mf-companion-build make "$@"
