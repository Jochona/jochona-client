#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/jochona-abr-benchmark.XXXXXX")"
trap 'rm -rf "$BUILD_DIR"' EXIT

"${CXX:-c++}" \
  -std=c++17 \
  -O2 \
  -DNDEBUG \
  -Wall \
  -Wextra \
  -Wpedantic \
  -Werror \
  -I"$ROOT_DIR/benchmarks/abr" \
  "$ROOT_DIR/benchmarks/abr/abr_controller.cpp" \
  "$ROOT_DIR/benchmarks/abr/abr_benchmark.cpp" \
  -o "$BUILD_DIR/abr-benchmark"

"$BUILD_DIR/abr-benchmark"
