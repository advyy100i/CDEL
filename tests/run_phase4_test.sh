#!/usr/bin/env bash
# Phase 4 end-to-end test: run the correlator on Phase 1-3 artifacts.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ARTIFACTS="$REPO_ROOT/tests/artifacts"

echo "[Phase4] Running correlator..."
python3 "$REPO_ROOT/correlator.py" \
  --config-map   "$ARTIFACTS/build_config_map.json" \
  --ast-mapping  "$ARTIFACTS/ast_mapping.json" \
  --reachability "$ARTIFACTS/reachability.json" \
  --report       "$REPO_ROOT/report.md" \
  --json-out     "$ARTIFACTS/dead_features.json" \
  --project-root "$REPO_ROOT"

echo ""
echo "[Phase4] --- report.md ---"
cat "$REPO_ROOT/report.md"
