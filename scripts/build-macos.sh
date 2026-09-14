#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/cpp/build-macos"

brew install cmake curl

cmake -S "$ROOT/cpp" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --target DuskPlug duskplug_tests
"$BUILD_DIR/duskplug_tests"

cd "$ROOT"
rm -f DuskPlug-macOS.zip
ditto -c -k --sequesterRsrc --keepParent "$ROOT/DuskPlug.app" DuskPlug-macOS.zip
echo "Built $ROOT/DuskPlug-macOS.zip"
