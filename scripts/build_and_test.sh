#!/usr/bin/env bash
# Local Linux developer convenience script.
# Run from repo root.
set -euo pipefail

BUILD_DIR="${ST2110_BUILD_DIR:-build}"
BUILD_TYPE="${ST2110_BUILD_TYPE:-Debug}"

export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:/usr/local/lib64/pkgconfig:/usr/local/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

command -v pkg-config >/dev/null 2>&1 || {
  echo "pkg-config is required for local Linux MTL builds."
  exit 1
}

pkg-config --exists mtl || {
  echo "Media Transport Library pkg-config package 'mtl' was not found."
  echo "Install MTL locally, or set PKG_CONFIG_PATH so pkg-config can find mtl.pc."
  echo "Current PKG_CONFIG_PATH=${PKG_CONFIG_PATH:-}"
  exit 1
}

cmake -S . -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

cmake --build "${BUILD_DIR}"
ctest --test-dir "${BUILD_DIR}" --output-on-failure