#!/bin/bash
set -euxo pipefail
apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq build-essential cmake ninja-build git wget xz-utils python3 pkg-config llvm >/dev/null
cd /work
[ -d src ] || { wget -q https://github.com/azahar-emu/azahar/releases/download/2126.2/azahar-unified-source-2126.2.tar.xz; mkdir src; tar -xf azahar-unified-source-2126.2.tar.xz -C src --strip-components=1; rm azahar-unified-source-2126.2.tar.xz; }
cd src
# enable the GLES path on Linux (upstream only does it for Android)
grep -q RGDS_GLES src/citra_libretro/CMakeLists.txt || cat >> src/citra_libretro/CMakeLists.txt <<'C'
# RGDS_GLES: request an OpenGL ES 3.2 context instead of desktop GL 4.3
target_compile_definitions(citra_libretro_common PRIVATE USING_GLES)
target_compile_definitions(citra_libretro PRIVATE USING_GLES)
C
# Mali-G52 (Panfrost) has no geometry shaders: skip the per-fragment-lighting fix-up stage (A Link Between Worlds)
[ -f /work/azahar-no-geometry-shaders.patch ] && ! grep -q GeometryShadersSupported src/video_core/renderer_opengl/gl_shader_manager.h && \
  patch -p0 < /work/azahar-no-geometry-shaders.patch
cmake -G Ninja -B build -DENABLE_LIBRETRO=ON -DCMAKE_BUILD_TYPE=Release -DENABLE_LTO=OFF -DLIBRETRO_STATIC=ON -DENABLE_OPENGL=ON \
  -DCMAKE_C_FLAGS="-mcpu=cortex-a55" -DCMAKE_CXX_FLAGS="-mcpu=cortex-a55"
cmake --build build --target citra_libretro -j"$(nproc)"
f=$(find build -name 'azahar_libretro.so' | head -1)
llvm-strip -s "$f"; mkdir -p /work/out; cp "$f" /work/out/azahar_libretro.so; ls -la /work/out
