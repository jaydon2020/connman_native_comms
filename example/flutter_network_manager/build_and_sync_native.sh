#!/bin/bash
# build_and_sync_native.sh
# Builds the native library and syncs it to the Flutter project

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
FLUTTER_PROJECT="$SCRIPT_DIR"
LIBS_DIR="$FLUTTER_PROJECT/linux/libs"

echo "📦 Building native library..."
cd "$PROJECT_ROOT/native"
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

echo "📋 Syncing libconnman_nc.so to Flutter project..."
mkdir -p "$LIBS_DIR"
cp "$PROJECT_ROOT/build/libconnman_nc.so" "$LIBS_DIR/"
echo "✅ Done! libconnman_nc.so is now at: $LIBS_DIR/libconnman_nc.so"

echo ""
echo "🚀 You can now run:"
echo "   cd $FLUTTER_PROJECT && flutter run -d linux"
