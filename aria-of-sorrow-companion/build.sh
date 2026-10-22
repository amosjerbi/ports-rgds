#!/usr/bin/env bash
# Builds aria-of-sorrow-companion/build/aos-bottom (aarch64 Linux) in a native arm64 Debian trixie container.
set -euo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
docker build -q --platform linux/arm64 -t aos-companion-build "$D" >/dev/null
docker run --rm --platform linux/arm64 -v "$D":/src -w /src aos-companion-build make "$@"
