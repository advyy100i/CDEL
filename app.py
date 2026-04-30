#!/usr/bin/env python3
"""app.py — Dead Feature Detector: interactive web frontend.

Usage:
    python3 app.py              # opens http://localhost:5001
    python3 app.py --port 8080
"""

from __future__ import annotations

import json
import os
import re
import sys
import threading
import time
from pathlib import Path
from typing import Any, Dict, Optional

from flask import Flask, jsonify, render_template, request, Response

REPO_ROOT = Path(__file__).parent.resolve()
DEMO_PROJECT = str(REPO_ROOT / "tests" / "demo_project")
DEFAULT_OUTPUT = str(REPO_ROOT / "tests" / "artifacts" / "pipeline-out")


def _project_output_dir(project: str) -> Path:
    """Return a stable, per-project artifact directory under pipeline-out/."""
    safe = re.sub(r"[^a-zA-Z0-9_.-]", "_", Path(project).name)
    return REPO_ROOT / "tests" / "artifacts" / "pipeline-out" / safe

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


def _demangle_simple(mangled: str) -> str:
    """Minimal C++ name demangler using the length-prefix encoding."""
    if mangled in ("main",):
        return "main()"
    if mangled.startswith("__"):
        return mangled
    # Find first length-prefix in the mangled name
    m = re.match(r'_ZN?K?(\d+)', mangled)
    if m:
        length = int(m.group(1))
        start  = m.end()
        name   = mangled[start: start + length]
        rest   = mangled[start + length:]
        arg_map = {'b': 'bool', 'i': 'int', 'v': None, 'f': 'float',
                   'd': 'double', 'c': 'char', 'l': 'long', 'j': 'unsigned'}
        args: list = []
        for ch in rest:
            if ch not in arg_map:
                break
            val = arg_map[ch]
            if val:
                args.append(val)
        return f"{name}({', '.join(args)})"
    if len(mangled) > 28:
        return mangled[:12] + "…"
    return mangled


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
    output_dir = body.get("output_dir", "").strip() or str(_project_output_dir(project))

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


@app.route("/api/known_projects")
def api_known_projects():
    """Return a list of known/discoverable CMake projects for the path dropdown."""
    projects = []

    # Built-in projects shipped with the tool
    builtins = [
        ("PulsarNet demo  (built-in)", REPO_ROOT / "tests" / "demo_project"),
        ("Dummy project   (built-in)", REPO_ROOT / "tests" / "dummy_project"),
    ]
    for label, p in builtins:
        if (p / "CMakeLists.txt").exists():
            projects.append({"label": label, "path": str(p)})

    # Scan one level above REPO_ROOT for sibling CMake projects
    parent = REPO_ROOT.parent
    try:
        for sibling in sorted(parent.iterdir()):
            if sibling == REPO_ROOT:
                continue
            if (sibling / "CMakeLists.txt").exists():
                projects.append({"label": sibling.name, "path": str(sibling)})
    except PermissionError:
        pass

    return jsonify({"projects": projects})


@app.route("/api/artifacts")
def api_artifacts():
    """Return all four pipeline artifacts.

    Optional query param ?project=<path> reads from that project's
    per-project output dir instead of the currently active job's dir.
    This lets the frontend sync slides when the user switches the dropdown.
    """
    project_q = request.args.get("project", "").strip()
    if project_q:
        output_dir = _project_output_dir(project_q)
    else:
        with _lock:
            output_dir = Path(_job.get("output_dir") or DEFAULT_OUTPUT)

    result: Dict[str, Any] = {}
    for key, fname in [("build_config_map", "build_config_map.json"),
                        ("ast_mapping",      "ast_mapping.json"),
                        ("reachability",     "reachability.json"),
                        ("dead_features",    "dead_features.json")]:
        p = output_dir / fname
        if p.exists():
            try:
                result[key] = json.loads(p.read_text())
            except Exception:
                pass
    return jsonify(result)


@app.route("/api/report")
def api_report():
    """Return the Markdown report for a given project (or the active job)."""
    project_q = request.args.get("project", "").strip()
    if project_q:
        output_dir = _project_output_dir(project_q)
    else:
        with _lock:
            output_dir = Path(_job.get("output_dir") or DEFAULT_OUTPUT)

    p = output_dir / "report.md"
    if p.exists():
        return jsonify({"report_md": p.read_text(errors="replace")})
    return jsonify({"report_md": ""})


@app.route("/api/source")
def api_source():
    """Serve a source file that belongs to the active project."""
    file_path = request.args.get("file", "").strip()
    if not file_path:
        return jsonify({"error": "No file specified"}), 400
    project_q = request.args.get("project", "").strip()
    with _lock:
        project = project_q or _job.get("project") or DEMO_PROJECT
    try:
        resolved = Path(file_path).resolve()
        resolved.relative_to(Path(project).resolve())
    except (ValueError, OSError):
        return jsonify({"error": "Access denied"}), 403
    if not resolved.exists() or not resolved.is_file():
        return jsonify({"error": "File not found"}), 404
    content = resolved.read_text(errors="replace")
    return jsonify({"path": str(resolved), "name": resolved.name,
                    "lines": content.splitlines()})


@app.route("/api/callgraph")
def api_callgraph():
    """Build simplified call graph from Phase 3 data."""
    project_q  = request.args.get("project", "").strip()
    with _lock:
        project    = project_q or _job.get("project") or DEMO_PROJECT
        output_dir = _project_output_dir(project) if project_q else \
                     Path(_job.get("output_dir") or DEFAULT_OUTPUT)

    reach_path = output_dir / "reachability.json"
    if not reach_path.exists():
        return jsonify({"nodes": [], "edges": [], "entry_points": []})

    reach          = json.loads(reach_path.read_text())
    func_details   = reach.get("function_details", {})
    project_root   = str(Path(project).resolve())
    reachable_set  = set(reach.get("reachable_functions", []))
    entry_points   = reach.get("entry_points", [])

    user_funcs: Dict[str, Any] = {}
    stdlib_count = 0
    for mangled, details in func_details.items():
        src = details.get("source_file", "")
        if src and str(Path(src).resolve()).startswith(project_root):
            user_funcs[mangled] = details
        else:
            stdlib_count += 1

    nodes = [{"id": m, "name": _demangle_simple(m),
               "file": Path(d["source_file"]).name,
               "filepath": d.get("source_file", ""),
               "lines": d.get("lines", []),
               "reachable": m in reachable_set,
               "is_entry": m in entry_points,
               "is_group": False}
             for m, d in user_funcs.items()]

    if stdlib_count:
        nodes.append({"id": "__stdlib__", "name": "stdlib / STL",
                      "subtitle": f"{stdlib_count} fns", "file": "system",
                      "filepath": "", "lines": [], "reachable": True,
                      "is_entry": False, "is_group": True})

    name_to_mangled = {_demangle_simple(m).split("(")[0]: m for m in user_funcs}
    edges: list = []
    for mangled, details in user_funcs.items():
        src_file   = details.get("source_file", "")
        func_lines = details.get("lines", [])
        if not src_file or not func_lines:
            continue
        try:
            src_lines = Path(src_file).read_text(errors="replace").splitlines()
        except Exception:
            continue
        start = max(0, min(func_lines) - 1)
        end   = min(len(src_lines), max(func_lines) + 1)
        added_stdlib = False
        for i in range(start, end):
            line = src_lines[i]
            for fname, target in name_to_mangled.items():
                if target != mangled and fname + "(" in line:
                    e = {"source": mangled, "target": target}
                    if e not in edges:
                        edges.append(e)
            if not added_stdlib and any(kw in line for kw in ("std::", "cout", "printf")):
                e = {"source": mangled, "target": "__stdlib__"}
                if e not in edges:
                    edges.append(e)
                added_stdlib = True

    return jsonify({"nodes": nodes, "edges": edges, "entry_points": entry_points})


# ── Entry point ────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    import argparse, webbrowser

    p = argparse.ArgumentParser(description="Dead Feature Detector Web UI")
    p.add_argument("--port",       type=int, default=5001)
    p.add_argument("--no-browser", action="store_true")
    p.add_argument("--output-dir", default=None,
                   help="Pre-populate output dir (results from a completed pipeline run)")
    p.add_argument("--project",    default=None,
                   help="Pre-populate project path shown in the UI")
    args = p.parse_args()

    # If a completed run's output dir is supplied, seed the job state so
    # /api/artifacts and /api/callgraph serve those results immediately.
    if args.output_dir:
        out = str(Path(args.output_dir).resolve())
        proj = str(Path(args.project).resolve()) if args.project else DEMO_PROJECT
        _set(status="done", phase=4,
             phase_statuses={1:"done",2:"done",3:"done",4:"done"},
             output_dir=out, project=proj)
        # load summary + dead features from the artifact if present
        dead_path = Path(out) / "dead_features.json"
        if dead_path.exists():
            try:
                d = json.loads(dead_path.read_text())
                _set(summary=d.get("summary"),
                     dead_features=d.get("dead_features", []))
            except Exception:
                pass
        report_path = Path(out) / "report.md"
        if report_path.exists():
            try:
                _set(report_md=report_path.read_text())
            except Exception:
                pass

    url = f"http://localhost:{args.port}"
    print(f"\n  \033[35m💀 Dead Feature Detector\033[0m")
    print(f"  ─────────────────────────────────────────")
    print(f"  Open in your browser: \033[36m{url}\033[0m")
    if args.output_dir:
        print(f"  Results loaded from:  \033[33m{args.output_dir}\033[0m")
    else:
        print(f"  Demo project:         \033[33m{DEMO_PROJECT}\033[0m")
    print(f"  Press Ctrl+C to stop\n")

    if not args.no_browser:
        threading.Timer(1.2, lambda: webbrowser.open(url)).start()

    app.run(host="0.0.0.0", port=args.port, debug=False, use_reloader=False)
