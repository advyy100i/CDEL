#!/usr/bin/env bash
# Phase 1 end-to-end test (macOS / Linux)
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_PATH="$REPO_ROOT/tests/demo_project"
BUILD_PATH="$PROJECT_PATH/build-extractor"
OUTPUT_PATH="$REPO_ROOT/tests/artifacts/build_config_map.json"

PYTHON_EXE="python3"
if [ -f "$REPO_ROOT/.venv/bin/python" ]; then
  PYTHON_EXE="$REPO_ROOT/.venv/bin/python"
fi

CMAKE_ARGS=()
if [ -f "$REPO_ROOT/.venv/bin/cmake" ]; then
  CMAKE_ARGS=(--cmake-bin "$REPO_ROOT/.venv/bin/cmake")
fi

echo "[Phase1] Running extractor on PulsarNet demo project..."
rm -rf "$BUILD_PATH"

"$PYTHON_EXE" "$REPO_ROOT/extractor/extractor.py" \
  --project "$PROJECT_PATH" \
  --build-dir "$BUILD_PATH" \
  --output "$OUTPUT_PATH" \
  ${CMAKE_ARGS[@]+"${CMAKE_ARGS[@]}"} \
  --verbose

echo "[Phase1] Extractor output: $OUTPUT_PATH"
echo "[Phase1] --- Contents ---"
cat "$OUTPUT_PATH"
