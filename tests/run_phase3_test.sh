#!/usr/bin/env bash
# Phase 3 end-to-end test:
#  1. Compile dummy project to whole-program LLVM IR (with debug info)
#  2. Build DeadFeaturePass plugin
#  3. Run the pass → reachability.json
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PASS_SRC="$REPO_ROOT/llvm-pass/DeadFeaturePass"
PASS_BUILD="$PASS_SRC/build"
ARTIFACTS="$REPO_ROOT/tests/artifacts"
DUMMY="$REPO_ROOT/tests/dummy_project/src"

# ── Locate LLVM tools ─────────────────────────────────────────────────────────
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
  echo "ERROR: llvm-config not found. Install with: brew install llvm"
  exit 1
fi

LLVM_BINDIR="$("$LLVM_CONFIG" --bindir)"
CLANGXX="$LLVM_BINDIR/clang++"
OPT="$LLVM_BINDIR/opt"
LLVM_LINK="$LLVM_BINDIR/llvm-link"
echo "[Phase3] LLVM $("$LLVM_CONFIG" --version)  |  opt: $OPT"

# ── Build pass plugin ─────────────────────────────────────────────────────────
echo "[Phase3] Building DeadFeaturePass..."
bash "$PASS_SRC/build.sh"
PLUGIN="$(find "$PASS_BUILD" -name "DeadFeaturePass.*" \( -name "*.dylib" -o -name "*.so" \) | head -1)"
echo "[Phase3] Plugin: $PLUGIN"

mkdir -p "$ARTIFACTS"
IR_DIR="$ARTIFACTS/ir"
mkdir -p "$IR_DIR"

# ── Compile sources to LLVM bitcode (with defines matching default CMake config)
# Mirrors Phase 1 result: dummy_core has CORE_BUILD=1, EXPERIMENTAL_FEATURE=1
#                         dummy_app has APP_BUILD=1
echo "[Phase3] Compiling to LLVM IR..."

"$CLANGXX" -g -std=c++17 \
  -DCORE_BUILD=1 -DEXPERIMENTAL_FEATURE=1 \
  -I "$DUMMY" \
  -emit-llvm -c "$DUMMY/feature.cpp" \
  -o "$IR_DIR/feature.bc"

"$CLANGXX" -g -std=c++17 \
  -DAPP_BUILD=1 \
  -I "$DUMMY" \
  -emit-llvm -c "$DUMMY/main.cpp" \
  -o "$IR_DIR/main.bc"

# ── Link into whole-program IR ────────────────────────────────────────────────
echo "[Phase3] Linking to whole-program IR..."
"$LLVM_LINK" "$IR_DIR/feature.bc" "$IR_DIR/main.bc" \
  -o "$IR_DIR/whole_program.bc"

# ── Run DeadFeaturePass ───────────────────────────────────────────────────────
OUTPUT="$ARTIFACTS/reachability.json"
echo "[Phase3] Running DeadFeaturePass..."
"$OPT" \
  --load-pass-plugin="$PLUGIN" \
  --passes="dead-feature-pass" \
  --dead-feature-output="$OUTPUT" \
  "$IR_DIR/whole_program.bc" \
  --disable-output

echo "[Phase3] Reachability output: $OUTPUT"
echo "[Phase3] --- Contents ---"
cat "$OUTPUT"
