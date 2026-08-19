#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
extra=()
if [[ "${1:-}" == "--device-sdk" ]]; then
  extra+=(-DEHC_TEST_DEVICE_SDK=ON)
fi
cmake -S . -B build -DEHC_BUILD_TESTS=ON "${extra[@]}"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
