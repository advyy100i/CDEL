#!/usr/bin/env bash
set -euo pipefail

# Runs the full Dead Feature Detector pipeline, then opens the interactive
# web UI so results can be explored visually.
#
# Usage:
#   bash run.sh                                       # demo project, port 5001
#   bash run.sh /path/to/cmake/project
#   bash run.sh /path/to/cmake/project /path/to/out
#   bash run.sh /path/to/cmake/project /path/to/out 8080
#   LLVM_CONFIG=/path/to/llvm-config bash run.sh
#   NO_UI=1 bash run.sh                               # pipeline only, no web UI

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_PATH="${1:-$SCRIPT_DIR/tests/dummy_project}"
OUTPUT_DIR="${2:-$SCRIPT_DIR/tests/artifacts/pipeline-out}"
PORT="${3:-5001}"

# ── Validate project path ────────────────────────────────────────────────────
if [ ! -d "$PROJECT_PATH" ]; then
  echo "ERROR: project path not found: $PROJECT_PATH"
  exit 1
fi

# ── Resolve Python executable ────────────────────────────────────────────────
PYTHON_EXE="python3"
if [ -x "$SCRIPT_DIR/.venv/bin/python" ]; then
  PYTHON_EXE="$SCRIPT_DIR/.venv/bin/python"
fi

# ── Check / install Flask ────────────────────────────────────────────────────
if ! "$PYTHON_EXE" -c "import flask" 2>/dev/null; then
  echo "[run.sh] Flask not found — installing from requirements.txt…"
  "$PYTHON_EXE" -m pip install -q -r "$SCRIPT_DIR/requirements.txt"
fi

echo ""
echo "  💀 Dead Feature Detector"
echo "  ─────────────────────────────────────────────"
echo "  Project    : $PROJECT_PATH"
echo "  Output dir : $OUTPUT_DIR"
echo "  Python     : $PYTHON_EXE"
if [ -n "${LLVM_CONFIG:-}" ]; then
  echo "  LLVM_CONFIG: $LLVM_CONFIG"
fi
echo ""

# ── Run pipeline ─────────────────────────────────────────────────────────────
PIPELINE_CMD=(
  "$PYTHON_EXE" "$SCRIPT_DIR/pipeline.py"
  --project    "$PROJECT_PATH"
  --output-dir "$OUTPUT_DIR"
)
if [ -n "${LLVM_CONFIG:-}" ]; then
  PIPELINE_CMD+=(--llvm-config "$LLVM_CONFIG")
fi

echo "[run.sh] Starting pipeline…"
"${PIPELINE_CMD[@]}"
echo "[run.sh] Pipeline complete."
echo ""

# ── Launch web UI ─────────────────────────────────────────────────────────────
if [ "${NO_UI:-0}" = "1" ]; then
  echo "[run.sh] NO_UI=1 — skipping web UI."
  exit 0
fi

echo "[run.sh] Starting web UI on http://localhost:$PORT"
echo "[run.sh] Press Ctrl+C to stop."
echo ""

exec "$PYTHON_EXE" "$SCRIPT_DIR/app.py" \
  --port "$PORT" \
  --output-dir "$OUTPUT_DIR" \
  --project    "$PROJECT_PATH"
