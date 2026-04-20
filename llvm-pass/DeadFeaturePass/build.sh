#!/usr/bin/env bash
# Build DeadFeaturePass LLVM plugin.
# Usage: bash build.sh [--clean]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

LLVM_CONFIG="${LLVM_CONFIG:-}"
if [ -z "$LLVM_CONFIG" ]; then
  for candidate in \
      /opt/homebrew/opt/llvm/bin/llvm-config \
      /usr/local/opt/llvm/bin/llvm-config \
      llvm-config; do
    if command -v "$candidate" &>/dev/null; then
      LLVM_CONFIG="$candidate"
      break
    fi
  done
fi

if [ -z "$LLVM_CONFIG" ]; then
  echo "ERROR: llvm-config not found. Run 'brew install llvm' or set LLVM_CONFIG=<path>."
  exit 1
fi

LLVM_CMAKE_DIR="$("$LLVM_CONFIG" --cmakedir)"
LLVM_BINDIR="$("$LLVM_CONFIG" --bindir)"
echo "[DeadFeaturePass] llvm-config: $LLVM_CONFIG  ($("$LLVM_CONFIG" --version))"
echo "[DeadFeaturePass] LLVM CMake dir: $LLVM_CMAKE_DIR"

if [ "${1:-}" = "--clean" ]; then
  rm -rf "$BUILD_DIR"
fi

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR="$LLVM_CMAKE_DIR" \
  -DCMAKE_CXX_COMPILER="$LLVM_BINDIR/clang++" \
  -G "$(command -v ninja &>/dev/null && echo Ninja || echo 'Unix Makefiles')"

cmake --build "$BUILD_DIR" --parallel

PLUGIN="$(find "$BUILD_DIR" -name "DeadFeaturePass.*" \( -name "*.dylib" -o -name "*.so" \) | head -1)"
if [ -z "$PLUGIN" ]; then
  echo "ERROR: Plugin not found after build."
  exit 1
fi
echo "[DeadFeaturePass] Plugin built: $PLUGIN"
