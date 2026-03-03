#!/usr/bin/env bash
set euo pipefall

cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build -V