#!/usr/bin/env bash
set -e

build_start_epoch=$(date +%s)
build_start_text=$(date "+%Y-%m-%d %H:%M:%S")

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

build_end_epoch=$(date +%s)
build_end_text=$(date "+%Y-%m-%d %H:%M:%S")
elapsed=$((build_end_epoch - build_start_epoch))

printf "\nBuild summary\n"
printf "  Start:   %s\n" "$build_start_text"
printf "  End:     %s\n" "$build_end_text"
printf "  Elapsed: %02d:%02d\n" $((elapsed / 60)) $((elapsed % 60))

shopt -s nullglob
artifacts=(build/switch/bin/*.elf build/switch/bin/*.nacp build/switch/bin/*.nro)
if [ ${#artifacts[@]} -eq 0 ]; then
  printf "  No Switch artifacts found in build/switch/bin\n"
else
  for path in "${artifacts[@]}"; do
    bytes=$(stat -c %s "$path")
    mib=$(awk "BEGIN { printf \"%.2f\", $bytes / 1048576 }")
    printf "  %s: %s bytes (%s MiB)\n" "$path" "$bytes" "$mib"
  done
fi
