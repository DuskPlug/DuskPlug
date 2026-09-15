#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/cpp/build-linux"
STAGING="$ROOT/release-linux"

sudo apt-get update
sudo apt-get install -y \
  build-essential cmake pkg-config \
  libcurl4-openssl-dev \
  libgtk-3-dev libayatana-appindicator3-dev \
  libgeoclue-2-dev libsystemd-dev
sudo apt-get install -y libwebkit2gtk-4.0-dev

cmake -S "$ROOT/cpp" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --target duskplug duskplug_tests
"$BUILD_DIR/duskplug_tests"

rm -rf "$STAGING"
mkdir -p "$STAGING/assets" "$STAGING/docs"
cp "$BUILD_DIR/duskplug" "$STAGING/"
cp "$ROOT/packaging/linux/duskplug.desktop" "$STAGING/"
cp "$ROOT/config.example.json" "$ROOT/README.md" "$STAGING/"
cp -r "$ROOT/assets" "$STAGING/"
cp -r "$ROOT/docs" "$STAGING/"

tar -C "$STAGING" -czf "$ROOT/DuskPlug-Linux-x64.tar.gz" .
echo "Built $ROOT/DuskPlug-Linux-x64.tar.gz"
