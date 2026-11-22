#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
g++ -O3 -std=c++17 -mpopcnt src/cstd.cpp -o build/cstd
