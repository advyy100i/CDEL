#!/usr/bin/env bash
# Phase 2 end-to-end test: build IfdefMapper plugin and run it on the demo project.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLUGIN_SRC="$REPO_ROOT/llvm-pass/IfdefMapper"
PLUGIN_BUILD="$PLUGIN_SRC/build"
ARTIFACTS="$REPO_ROOT/tests/artifacts"
DEMO_SRC="$REPO_ROOT/tests/demo_project/src"
DEMO_INC="$REPO_ROOT/tests/demo_project/include"

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
# Defines mirror the default CMake configuration:
#   pulsar_core: PULSAR_CORE_BUILD=1 ENABLE_TLS=1 ENABLE_METRICS=1
#   pulsar_server: PULSAR_SERVER_BUILD=1
#   pulsar_client: PULSAR_CLIENT_BUILD=1
run_core_file() {
  local src="$1"
  local out="$2"
  echo "[Phase2] Analyzing (core): $src"
  "$CLANGXX" \
    -Xclang -load    -Xclang "$PLUGIN" \
    -Xclang -add-plugin -Xclang ifdef-mapper \
    -Xclang -plugin-arg-ifdef-mapper -Xclang "output=$out" \
    -std=c++17 \
    -I "$DEMO_INC" \
    -DPULSAR_CORE_BUILD=1 \
    -DENABLE_TLS=1 \
    -DENABLE_METRICS=1 \
    -fsyntax-only \
    "$src"
}

run_server_file() {
  local src="$1"
  local out="$2"
  echo "[Phase2] Analyzing (server): $src"
  "$CLANGXX" \
    -Xclang -load    -Xclang "$PLUGIN" \
    -Xclang -add-plugin -Xclang ifdef-mapper \
    -Xclang -plugin-arg-ifdef-mapper -Xclang "output=$out" \
    -std=c++17 \
    -I "$DEMO_INC" \
    -DPULSAR_SERVER_BUILD=1 \
    -DENABLE_TLS=1 \
    -DENABLE_METRICS=1 \
    -fsyntax-only \
    "$src"
}

run_client_file() {
  local src="$1"
  local out="$2"
  echo "[Phase2] Analyzing (client): $src"
  "$CLANGXX" \
    -Xclang -load    -Xclang "$PLUGIN" \
    -Xclang -add-plugin -Xclang ifdef-mapper \
    -Xclang -plugin-arg-ifdef-mapper -Xclang "output=$out" \
    -std=c++17 \
    -I "$DEMO_INC" \
    -DPULSAR_CLIENT_BUILD=1 \
    -DENABLE_TLS=1 \
    -DENABLE_METRICS=1 \
    -fsyntax-only \
    "$src"
}

run_core_file   "$DEMO_SRC/connection.cpp" "$ARTIFACTS/ast_mapping_connection.json"
run_core_file   "$DEMO_SRC/protocol.cpp"   "$ARTIFACTS/ast_mapping_protocol.json"
run_core_file   "$DEMO_SRC/metrics.cpp"    "$ARTIFACTS/ast_mapping_metrics.json"
run_core_file   "$DEMO_SRC/crypto.cpp"     "$ARTIFACTS/ast_mapping_crypto.json"
run_core_file   "$DEMO_SRC/scheduler.cpp"  "$ARTIFACTS/ast_mapping_scheduler.json"
run_server_file "$DEMO_SRC/server_main.cpp" "$ARTIFACTS/ast_mapping_server_main.json"
run_client_file "$DEMO_SRC/client_main.cpp" "$ARTIFACTS/ast_mapping_client_main.json"

# ── Merge all per-file mappings into ast_mapping.json ────────────────────────
python3 - <<'PYEOF'
import json, pathlib

arts = pathlib.Path("tests/artifacts")
merged = {"tool": "IfdefMapper", "files": {}}

for part in sorted(arts.glob("ast_mapping_*.json")):
    data = json.loads(part.read_text())
    merged["files"].update(data.get("files", {}))

out = arts / "ast_mapping.json"
out.write_text(json.dumps(merged, indent=2))
print(f"[Phase2] Merged AST mapping → {out}  ({len(merged['files'])} files)")
print(json.dumps(merged, indent=2))
PYEOF
