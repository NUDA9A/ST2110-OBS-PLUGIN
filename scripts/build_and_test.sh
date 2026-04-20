#!/usr/bin/env bash
# Run from repo root.
set -euo pipefail

cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build -V