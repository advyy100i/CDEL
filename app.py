#!/usr/bin/env python3
"""app.py — Dead Feature Detector: interactive web frontend.

Usage:
    python3 app.py              # opens http://localhost:5001
    python3 app.py --port 8080
"""

from __future__ import annotations

import json
import os
import sys
import threading
import time
from pathlib import Path
from typing import Any, Dict, Optional

from flask import Flask, jsonify, render_template, request, Response

REPO_ROOT = Path(__file__).parent.resolve()
DEMO_PROJECT = str(REPO_ROOT / "tests" / "dummy_project")
DEFAULT_OUTPUT = str(REPO_ROOT / "tests" / "artifacts" / "pipeline-out")

app = Flask(__name__, template_folder=str(REPO_ROOT / "templates"))

# ── Global job state ───────────────────────────────────────────────────────────
# Single-job model: one analysis runs at a time.

_job: Dict[str, Any] = {
    "status":   "idle",   # idle | running | done | error
    "phase":    0,        # 1-4
    "phase_statuses": {   # per-phase status
        1: "idle", 2: "idle", 3: "idle", 4: "idle"
    },
    "log":      [],       # list of {phase, msg} dicts
    "summary":  None,
    "report_md": None,
    "dead_features": None,
    "error":    None,
    "project":  None,
    "output_dir": None,
}
_lock = threading.Lock()


def _set(**kwargs: Any) -> None:
    with _lock:
        _job.update(kwargs)


def _progress(phase_name: str, msg: str) -> None:
    with _lock:
        _job["log"].append({"phase": phase_name, "msg": msg})
        # Keep last 500 log lines
        if len(_job["log"]) > 500:
            _job["log"] = _job["log"][-500:]


def _run_pipeline(project: str, output_dir: str) -> None:
    """Background thread: run all phases and update _job state."""
    from pipeline import DeadFeaturePipeline

    def progress(phase: str, msg: str) -> None:
        _progress(phase, msg)

    _set(status="running", phase=1,
         phase_statuses={1: "running", 2: "idle", 3: "idle", 4: "idle"},
         project=project, output_dir=output_dir,
         log=[], summary=None, report_md=None, dead_features=None, error=None)

    try:
        pipeline = DeadFeaturePipeline(
            project_path=project,
            output_dir=output_dir,
            progress=progress,
        )

        # Phase 1
        _set(phase=1, phase_statuses={1: "running", 2: "idle", 3: "idle", 4: "idle"})
        pipeline.ensure_plugins_built()
        pipeline.run_phase1()
        _set(phase_statuses={1: "done", 2: "running", 3: "idle", 4: "idle"})

        # Phase 2
        _set(phase=2)
        pipeline.run_phase2()
        _set(phase_statuses={1: "done", 2: "done", 3: "running", 4: "idle"})

        # Phase 3
        _set(phase=3)
        pipeline.run_phase3()
        _set(phase_statuses={1: "done", 2: "done", 3: "done", 4: "running"})

        # Phase 4
        _set(phase=4)
        pipeline.run_phase4()
        _set(phase_statuses={1: "done", 2: "done", 3: "done", 4: "done"})

        # Read results
        report_md = pipeline.report_path.read_text() if pipeline.report_path else ""
        dead_data = json.loads(pipeline.dead_features_path.read_text()) \
            if pipeline.dead_features_path else {}

        _set(status="done", phase=4,
             summary=dead_data.get("summary"),
             report_md=report_md,
             dead_features=dead_data.get("dead_features", []))

    except Exception as exc:
        _set(status="error", error=str(exc))
        _progress("error", f"✗ {exc}")


# ── Routes ─────────────────────────────────────────────────────────────────────

@app.route("/")
def index():
    return render_template("index.html", demo_project=DEMO_PROJECT)


@app.route("/api/analyze", methods=["POST"])
def api_analyze():
    with _lock:
        if _job["status"] == "running":
            return jsonify({"error": "Analysis already running"}), 409

    body       = request.get_json(silent=True) or {}
    project    = body.get("project", "").strip() or DEMO_PROJECT
    output_dir = body.get("output_dir", "").strip() or DEFAULT_OUTPUT

    if not Path(project).exists():
        return jsonify({"error": f"Project path not found: {project}"}), 400

    t = threading.Thread(target=_run_pipeline, args=(project, output_dir),
                         daemon=True)
    t.start()
    return jsonify({"ok": True, "project": project})


@app.route("/api/status")
def api_status():
    with _lock:
        return jsonify({
            "status":          _job["status"],
            "phase":           _job["phase"],
            "phase_statuses":  _job["phase_statuses"],
            "summary":         _job["summary"],
            "report_md":       _job["report_md"],
            "dead_features":   _job["dead_features"],
            "error":           _job["error"],
            "project":         _job["project"],
            "log_count":       len(_job["log"]),
        })


@app.route("/api/log")
def api_log():
    """Return log lines since a given offset."""
    offset = int(request.args.get("offset", 0))
    with _lock:
        lines = _job["log"][offset:]
    return jsonify({"lines": lines, "total": len(_job["log"]) + offset})


@app.route("/api/reset", methods=["POST"])
def api_reset():
    with _lock:
        if _job["status"] == "running":
            return jsonify({"error": "Cannot reset while running"}), 409
    _set(status="idle", phase=0,
         phase_statuses={1: "idle", 2: "idle", 3: "idle", 4: "idle"},
         log=[], summary=None, report_md=None, dead_features=None, error=None,
         project=None)
    return jsonify({"ok": True})


@app.route("/api/demo_path")
def api_demo_path():
    return jsonify({"path": DEMO_PROJECT})


# ── Entry point ────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    import argparse, webbrowser

    p = argparse.ArgumentParser(description="Dead Feature Detector Web UI")
    p.add_argument("--port", type=int, default=5001)
    p.add_argument("--no-browser", action="store_true")
    args = p.parse_args()

    url = f"http://localhost:{args.port}"
    print(f"\n  💀 Dead Feature Detector")
    print(f"  ─────────────────────────────────────────")
    print(f"  Open in your browser: \033[36m{url}\033[0m")
    print(f"  Demo project:         \033[33m{DEMO_PROJECT}\033[0m")
    print(f"  Press Ctrl+C to stop\n")

    if not args.no_browser:
        threading.Timer(1.2, lambda: webbrowser.open(url)).start()

    app.run(host="0.0.0.0", port=args.port, debug=False, use_reloader=False)
