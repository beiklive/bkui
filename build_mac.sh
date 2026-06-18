#!/usr/bin/env bash
set -e

build_start_epoch=$(date +%s)
build_start_text=$(date "+%Y-%m-%d %H:%M:%S")

if command -v ninja &>/dev/null; then
  GENERATOR="Ninja"
else
  GENERATOR="Unix Makefiles"
fi

if [ -f build/mac/CMakeCache.txt ] && ! grep -q "CMAKE_GENERATOR:INTERNAL=$GENERATOR" build/mac/CMakeCache.txt; then
  rm -rf build/mac
fi

cmake -S . -B build/mac -G "$GENERATOR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBKUI_PLATFORM_SWITCH=OFF \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DBKUI_SHARED=OFF

cmake --build build/mac --parallel

build_end_epoch=$(date +%s)
build_end_text=$(date "+%Y-%m-%d %H:%M:%S")
elapsed=$((build_end_epoch - build_start_epoch))

printf "\nBuild summary\n"
printf "  Start:   %s\n" "$build_start_text"
printf "  End:     %s\n" "$build_end_text"
printf "  Elapsed: %02d:%02d\n" $((elapsed / 60)) $((elapsed % 60))

shopt -s nullglob
artifacts=(build/mac/bin/*)
if [ ${#artifacts[@]} -eq 0 ]; then
  printf "  No artifacts found in build/mac/bin\n"
else
  for path in "${artifacts[@]}"; do
    if [ -f "$path" ] && [ -x "$path" ]; then
      bytes=$(stat -f %z "$path")
      mib=$(awk "BEGIN { printf \"%.2f\", $bytes / 1048576 }")
      printf "  %s: %s bytes (%s MiB)\n" "$path" "$bytes" "$mib"
    fi
  done
fi
