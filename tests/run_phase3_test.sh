#!/usr/bin/env bash
# Phase 3 end-to-end test:
#  1. Compile PulsarNet demo project to whole-program LLVM IR (with debug info)
#  2. Build DeadFeaturePass plugin
#  3. Run the pass → reachability.json
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PASS_SRC="$REPO_ROOT/llvm-pass/DeadFeaturePass"
PASS_BUILD="$PASS_SRC/build"
ARTIFACTS="$REPO_ROOT/tests/artifacts"
DEMO_SRC="$REPO_ROOT/tests/demo_project/src"
DEMO_INC="$REPO_ROOT/tests/demo_project/include"

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

# ── Compile sources to LLVM bitcode ──────────────────────────────────────────
# Defines mirror the default CMake configuration:
#   pulsar_core: PULSAR_CORE_BUILD=1, ENABLE_TLS=1, ENABLE_METRICS=1
#   pulsar_server: PULSAR_SERVER_BUILD=1 (+ ENABLE_TLS + ENABLE_METRICS via link)
#   pulsar_client: PULSAR_CLIENT_BUILD=1 (same)
echo "[Phase3] Compiling demo project to LLVM IR..."

compile_bc() {
  local src="$1"; local out="$2"; shift 2
  "$CLANGXX" -g -std=c++17 -I "$DEMO_INC" "$@" \
    -emit-llvm -c "$src" -o "$out"
}

CORE_DEFS=(-DPULSAR_CORE_BUILD=1 -DENABLE_TLS=1 -DENABLE_METRICS=1)
for src in connection protocol metrics crypto scheduler; do
  compile_bc "$DEMO_SRC/${src}.cpp" "$IR_DIR/${src}.bc" "${CORE_DEFS[@]}"
  echo "[Phase3]   compiled ${src}.cpp → ${src}.bc"
done

compile_bc "$DEMO_SRC/server_main.cpp" "$IR_DIR/server_main.bc" \
  -DPULSAR_SERVER_BUILD=1 -DENABLE_TLS=1 -DENABLE_METRICS=1
echo "[Phase3]   compiled server_main.cpp → server_main.bc"

compile_bc "$DEMO_SRC/client_main.cpp" "$IR_DIR/client_main.bc" \
  -DPULSAR_CLIENT_BUILD=1 -DENABLE_TLS=1 -DENABLE_METRICS=1
echo "[Phase3]   compiled client_main.cpp → client_main.bc"

# ── Link one whole-program IR per executable (avoids duplicate main symbol) ──
echo "[Phase3] Linking whole-program IRs (server + client separately)..."
CORE_BCS=("$IR_DIR/connection.bc" "$IR_DIR/protocol.bc" "$IR_DIR/metrics.bc"
           "$IR_DIR/crypto.bc"    "$IR_DIR/scheduler.bc")

"$LLVM_LINK" "${CORE_BCS[@]}" "$IR_DIR/server_main.bc" -o "$IR_DIR/whole_server.bc"
echo "[Phase3]   linked whole_server.bc"
"$LLVM_LINK" "${CORE_BCS[@]}" "$IR_DIR/client_main.bc" -o "$IR_DIR/whole_client.bc"
echo "[Phase3]   linked whole_client.bc"

# ── Run DeadFeaturePass on each, merge into one reachability.json ─────────────
OUTPUT="$ARTIFACTS/reachability.json"
echo "[Phase3] Running DeadFeaturePass on server program..."
"$OPT" \
  --load-pass-plugin="$PLUGIN" \
  --passes="dead-feature-pass" \
  --dead-feature-output="$ARTIFACTS/reachability_server.json" \
  "$IR_DIR/whole_server.bc" --disable-output

echo "[Phase3] Running DeadFeaturePass on client program..."
"$OPT" \
  --load-pass-plugin="$PLUGIN" \
  --passes="dead-feature-pass" \
  --dead-feature-output="$ARTIFACTS/reachability_client.json" \
  "$IR_DIR/whole_client.bc" --disable-output

echo "[Phase3] Merging reachability results..."
python3 - <<'PYEOF'
import json, pathlib

arts = pathlib.Path("tests/artifacts")
merged = {"reachable_functions": [], "reachable_lines": {},
          "function_details": {}, "entry_points": []}

for part in [arts / "reachability_server.json", arts / "reachability_client.json"]:
    if not part.exists(): continue
    data = json.loads(part.read_text())
    seen = set(merged["reachable_functions"])
    for fn in data.get("reachable_functions", []):
        if fn not in seen: merged["reachable_functions"].append(fn); seen.add(fn)
    seen_ep = set(merged["entry_points"])
    for fn in data.get("entry_points", []):
        if fn not in seen_ep: merged["entry_points"].append(fn); seen_ep.add(fn)
    for fp, lines in data.get("reachable_lines", {}).items():
        ex = set(merged["reachable_lines"].get(fp, []))
        merged["reachable_lines"][fp] = sorted(ex | set(lines))
    merged["function_details"].update(data.get("function_details", {}))

out = arts / "reachability.json"
out.write_text(json.dumps(merged, indent=2))
print(f"[Phase3] Merged → {out}  ({len(merged['reachable_functions'])} functions)")
PYEOF

echo "[Phase3] Reachability output: $OUTPUT"
echo "[Phase3] --- Contents ---"
cat "$OUTPUT"
