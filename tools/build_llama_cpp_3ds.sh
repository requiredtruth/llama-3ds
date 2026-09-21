#!/usr/bin/env bash
set -euo pipefail

LLAMA_CPP_COMMIT="6f41ac59e0a49a00483a316a22ada6b04edd2950"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="$ROOT/.deps"
SRC="$DEPS/llama.cpp"
BUILD="$DEPS/llama-build-3ds"

: "${DEVKITPRO:?DEVKITPRO must point to devkitPro}"

mkdir -p "$DEPS"
if [[ ! -d "$SRC/.git" ]]; then
  git clone --filter=blob:none https://github.com/ggml-org/llama.cpp.git "$SRC"
fi
git -C "$SRC" fetch --depth=1 origin "$LLAMA_CPP_COMMIT"
git -C "$SRC" checkout --detach "$LLAMA_CPP_COMMIT"
git -C "$SRC" reset --hard "$LLAMA_CPP_COMMIT"
git -C "$SRC" apply "$ROOT/patches/llama.cpp-3ds-static-backend.patch"

rm -rf "$BUILD"
cmake -S "$SRC" -B "$BUILD"   -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/3DS.cmake"   -DCMAKE_BUILD_TYPE=MinSizeRel   -DBUILD_SHARED_LIBS=OFF   -DGGML_STATIC=ON   -DGGML_NATIVE=OFF   -DGGML_OPENMP=OFF   -DGGML_LLAMAFILE=OFF   -DGGML_BLAS=OFF   -DGGML_ACCELERATE=OFF   -DGGML_CPU_REPACK=OFF   -DGGML_CPU_KLEIDIAI=OFF   -DGGML_CPU_ARM_ARCH=armv6k   -DLLAMA_BUILD_COMMON=OFF   -DLLAMA_BUILD_TESTS=OFF   -DLLAMA_BUILD_TOOLS=OFF   -DLLAMA_BUILD_EXAMPLES=OFF   -DLLAMA_BUILD_SERVER=OFF   -DLLAMA_BUILD_APP=OFF   -DLLAMA_TOOLS_INSTALL=OFF   -DLLAMA_TESTS_INSTALL=OFF   -DLLAMA_OPENSSL=OFF   -DLLAMA_SUBPROCESS=OFF

cmake --build "$BUILD" --target llama -j2
echo "llama.cpp 3DS static library build succeeded at $LLAMA_CPP_COMMIT"
