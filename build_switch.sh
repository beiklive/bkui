#!/usr/bin/env bash
set -e

export http_proxy="http://127.0.0.1:7892"
export https_proxy="http://127.0.0.1:7892"
export DEVKITPRO="/opt/devkitpro"
export DEVKITARM="$DEVKITPRO/devkitA64"

cmake -S . -B build/switch -G "Unix Makefiles" \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
  -DBKUI_SHARED=OFF

cmake --build build/switch
