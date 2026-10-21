#!/usr/bin/env bash
# Builds platform/rgds/build/tmc-rgds (aarch64 Linux, links the RG DS's own SDL2) in a native arm64
# Debian trixie container (glibc 2.41, the same as ROCKNIX). Needs Docker (colima on Apple Silicon).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
docker build -q --platform linux/arm64 -t tmc-rgds-build "$ROOT/platform/rgds" >/dev/null
docker run --rm --platform linux/arm64 -v "$ROOT":/work -w /work/platform/rgds tmc-rgds-build \
    make -j"${JOBS:-6}" "$@"
