#!/usr/bin/env bash
# Phase 2 end-to-end test: build IfdefMapper plugin and run it on the dummy project.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLUGIN_SRC="$REPO_ROOT/llvm-pass/IfdefMapper"
PLUGIN_BUILD="$PLUGIN_SRC/build"
ARTIFACTS="$REPO_ROOT/tests/artifacts"
DUMMY="$REPO_ROOT/tests/dummy_project/src"

# ── Locate llvm-config ────────────────────────────────────────────────────────
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

CLANGXX="$("$LLVM_CONFIG" --bindir)/clang++"
LLVM_VER="$("$LLVM_CONFIG" --version)"
echo "[Phase2] LLVM $LLVM_VER  |  clang++: $CLANGXX"

# ── Build plugin ──────────────────────────────────────────────────────────────
echo "[Phase2] Building IfdefMapper plugin..."
bash "$PLUGIN_SRC/build.sh"

PLUGIN="$(find "$PLUGIN_BUILD" -name "IfdefMapper.*" \( -name "*.dylib" -o -name "*.so" \) | head -1)"
echo "[Phase2] Plugin: $PLUGIN"

mkdir -p "$ARTIFACTS"

# ── Run on each source file ───────────────────────────────────────────────────
run_on_file() {
  local src="$1"
  local out="$2"
  echo "[Phase2] Analyzing: $src"
  "$CLANGXX" \
    -Xclang -load -Xclang "$PLUGIN" \
    -Xclang -add-plugin -Xclang ifdef-mapper \
    -Xclang -plugin-arg-ifdef-mapper -Xclang "output=$out" \
    -std=c++17 \
    -I "$DUMMY" \
    -fsyntax-only \
    "$src"
}

run_on_file "$DUMMY/feature.cpp" "$ARTIFACTS/ast_mapping_feature.json"
run_on_file "$DUMMY/main.cpp"    "$ARTIFACTS/ast_mapping_main.json"

# ── Merge into single ast_mapping.json ───────────────────────────────────────
python3 - <<'PYEOF'
import json, pathlib, sys

arts = pathlib.Path("tests/artifacts")
merged = {"tool": "IfdefMapper", "files": {}}

for part in sorted(arts.glob("ast_mapping_*.json")):
    data = json.loads(part.read_text())
    merged["files"].update(data.get("files", {}))

out = arts / "ast_mapping.json"
out.write_text(json.dumps(merged, indent=2))
print(f"[Phase2] Merged AST mapping → {out}")
print(json.dumps(merged, indent=2))
PYEOF
