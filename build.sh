#!/usr/bin/env bash
# Top-level build script: compiles both LLVM plugins (IfdefMapper + DeadFeaturePass).
#
# Usage:
#   bash build.sh                                    # auto-detect llvm-config
#   LLVM_CONFIG=/opt/homebrew/opt/llvm/bin/llvm-config bash build.sh
#   bash build.sh --clean                            # clean rebuild
#
# Requires: Homebrew LLVM (brew install llvm)
#   Apple Clang (Xcode CLT) does NOT include LLVM dev headers.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CLEAN_FLAG="${1:-}"

# ── Check LLVM ────────────────────────────────────────────────────────────────
if [ -z "${LLVM_CONFIG:-}" ]; then
  for candidate in \
      /opt/homebrew/opt/llvm/bin/llvm-config \
      /usr/local/opt/llvm/bin/llvm-config \
      llvm-config; do
    if command -v "$candidate" &>/dev/null; then
      export LLVM_CONFIG="$candidate"
      break
    fi
  done
fi

if [ -z "${LLVM_CONFIG:-}" ]; then
  echo "ERROR: llvm-config not found."
  echo "       Install Homebrew LLVM: brew install llvm"
  echo "       Then re-run: LLVM_CONFIG=/opt/homebrew/opt/llvm/bin/llvm-config bash build.sh"
  exit 1
fi

echo ""
echo "  Dead Feature Detector — Plugin Build"
echo "  ──────────────────────────────────────────"
echo "  llvm-config : $LLVM_CONFIG"
echo "  LLVM version: $("$LLVM_CONFIG" --version)"
echo ""

# ── Build IfdefMapper (Phase 2 Clang plugin) ──────────────────────────────────
echo "[build.sh] === Phase 2: IfdefMapper ==="
if [ "$CLEAN_FLAG" = "--clean" ]; then
  bash "$SCRIPT_DIR/llvm-pass/IfdefMapper/build.sh" --clean
else
  bash "$SCRIPT_DIR/llvm-pass/IfdefMapper/build.sh"
fi
echo ""

# ── Build DeadFeaturePass (Phase 3 LLVM Module pass) ─────────────────────────
echo "[build.sh] === Phase 3: DeadFeaturePass ==="
if [ "$CLEAN_FLAG" = "--clean" ]; then
  bash "$SCRIPT_DIR/llvm-pass/DeadFeaturePass/build.sh" --clean
else
  bash "$SCRIPT_DIR/llvm-pass/DeadFeaturePass/build.sh"
fi
echo ""

echo "  Build complete. Both plugins are ready."
echo "  Run the pipeline with: bash run.sh"
echo ""
