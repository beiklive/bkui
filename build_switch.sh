#!/usr/bin/env bash
set -e

export http_proxy="http://127.0.0.1:7892"
export https_proxy="http://127.0.0.1:7892"
export DEVKITPRO="/opt/devkitpro"
export DEVKITARM="$DEVKITPRO/devkitA64"
export TMPDIR="$PWD/build/tmp"
export TMP="$TMPDIR"
export TEMP="$TMPDIR"

mkdir -p "$TMPDIR"

if [ -f build/switch/CMakeCache.txt ] && ! grep -q "CMAKE_GENERATOR:INTERNAL=Ninja" build/switch/CMakeCache.txt; then
  rm -rf build/switch
fi

cmake -S . -B build/switch -G "Ninja" \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
  -DBKUI_PLATFORM_SWITCH=ON \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DBKUI_SHARED=OFF

cmake --build build/switch
