#!/usr/bin/env python3
"""pipeline.py — Dead Feature Detector: automated end-to-end pipeline.

Can be used standalone (headless) or imported by app.py (web UI).

Usage:
    python3 pipeline.py --project tests/dummy_project
    python3 pipeline.py --project /path/to/cmake/project --output-dir /tmp/dfd-out
"""

from __future__ import annotations

import json
import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Callable, Dict, List, Optional, Tuple

REPO_ROOT = Path(__file__).parent.resolve()

# ── LLVM tool discovery ────────────────────────────────────────────────────────

def find_llvm_config() -> Optional[str]:
    for candidate in [
        "/opt/homebrew/opt/llvm/bin/llvm-config",
        "/usr/local/opt/llvm/bin/llvm-config",
        "llvm-config",
    ]:
        if shutil.which(candidate) or Path(candidate).exists():
            return candidate
    return None


def llvm_bindir(llvm_config: str) -> str:
    return subprocess.check_output([llvm_config, "--bindir"],
                                   text=True).strip()


def discover_tools(llvm_config: Optional[str] = None) -> Dict[str, str]:
    """Return a dict of tool-name → absolute path for all LLVM/system tools."""
    lc = llvm_config or find_llvm_config()
    if not lc:
        raise RuntimeError(
            "llvm-config not found. Install LLVM: brew install llvm"
        )
    bd = llvm_bindir(lc)
    tools = {
        "llvm_config": lc,
        "clangxx":     str(Path(bd) / "clang++"),
        "opt":         str(Path(bd) / "opt"),
        "llvm_link":   str(Path(bd) / "llvm-link"),
        "python":      sys.executable,
    }
    for name, path in tools.items():
        if name == "llvm_config":
            continue
        if not Path(path).exists():
            raise RuntimeError(f"Tool not found: {path}  (expected from llvm-config --bindir)")
    return tools


def plugin_path(name: str) -> Path:
    """Find built plugin .so in llvm-pass/<name>/build/."""
    for ext in ("so", "dylib"):
        p = REPO_ROOT / "llvm-pass" / name / "build" / f"{name}.{ext}"
        if p.exists():
            return p
    return REPO_ROOT / "llvm-pass" / name / "build" / f"{name}.so"


# ── compile_commands.json parser ───────────────────────────────────────────────

_FLAG_PATTERNS = {
    "defines":  re.compile(r"(?:^|\s)(-D\S+)"),
    "includes": re.compile(r"(?:^|\s)(-I\S+)"),
    "std":      re.compile(r"(?:^|\s)(-std=\S+)"),
}


def parse_entry_flags(entry: dict) -> Tuple[List[str], List[str], List[str]]:
    """Extract (-D, -I, -std=) flags from a compile_commands.json entry."""
    if "arguments" in entry:
        args = list(entry["arguments"])
    else:
        args = shlex.split(entry.get("command", ""), posix=os.name != "nt")

    defines, includes, std = [], [], []
    i = 0
    while i < len(args):
        a = args[i]
        if a == "-D" and i + 1 < len(args):
            defines.append(f"-D{args[i+1]}"); i += 2; continue
        elif a.startswith("-D"):
            defines.append(a)
        elif a == "-I" and i + 1 < len(args):
            includes.append(f"-I{args[i+1]}"); i += 2; continue
        elif a.startswith("-I"):
            includes.append(a)
        elif a.startswith("-std="):
            std = [a]
        i += 1
    return defines, includes, std or ["-std=c++17"]


# ── pipeline class ─────────────────────────────────────────────────────────────

ProgressCB = Callable[[str, str], None]   # (phase_name, message)


class DeadFeaturePipeline:
    """
    Orchestrates all four analysis phases for a given CMake project.

    Parameters
    ----------
    project_path : path to the CMake project root
    output_dir   : where to store all artifacts (created if missing)
    llvm_config  : optional path to llvm-config (auto-detected otherwise)
    progress     : callback(phase_name, message) for progress updates
    """

    def __init__(
        self,
        project_path: str,
        output_dir: str,
        llvm_config: Optional[str] = None,
        progress: Optional[ProgressCB] = None,
    ) -> None:
        self.project_path = Path(project_path).resolve()
        self.output_dir   = Path(output_dir).resolve()
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.ir_dir = self.output_dir / "ir"
        self.ir_dir.mkdir(exist_ok=True)
        self.progress = progress or (lambda phase, msg: print(f"[{phase}] {msg}"))
        self.tools    = discover_tools(llvm_config)

        # Results populated by each phase
        self.config_map_path:   Optional[Path] = None
        self.compile_commands:  Optional[Path] = None
        self.ast_mapping_path:  Optional[Path] = None
        self.reachability_path: Optional[Path] = None
        self.report_path:       Optional[Path] = None
        self.dead_features_path:Optional[Path] = None

    # ── logging helpers ────────────────────────────────────────────────────────

    def _log(self, phase: str, msg: str) -> None:
        self.progress(phase, msg)

    def _run(self, phase: str, cmd: List[str], cwd: Optional[Path] = None,
             env: Optional[dict] = None) -> subprocess.CompletedProcess:
        self._log(phase, f"$ {' '.join(str(c) for c in cmd)}")
        result = subprocess.run(
            [str(c) for c in cmd],
            capture_output=True, text=True,
            cwd=str(cwd) if cwd else None,
            env=env,
        )
        if result.stdout.strip():
            for line in result.stdout.strip().splitlines():
                self._log(phase, line)
        if result.stderr.strip():
            for line in result.stderr.strip().splitlines():
                self._log(phase, line)
        return result

    # ── plugin build ──────────────────────────────────────────────────────────

    def ensure_plugins_built(self) -> None:
        """Build IfdefMapper and DeadFeaturePass if not already compiled."""
        for name in ("IfdefMapper", "DeadFeaturePass"):
            p = plugin_path(name)
            if not p.exists():
                self._log("setup", f"Building {name} plugin...")
                build_sh = REPO_ROOT / "llvm-pass" / name / "build.sh"
                env = {**os.environ,
                       "LLVM_CONFIG": self.tools["llvm_config"]}
                r = self._run("setup", ["bash", str(build_sh)], env=env)
                if r.returncode != 0:
                    raise RuntimeError(f"Failed to build {name} plugin")
            else:
                self._log("setup", f"{name} plugin already built: {p}")

    # ── Phase 1: CMake define extraction ──────────────────────────────────────

    def run_phase1(self) -> Path:
        self._log("phase1", "Reading your project's build settings (CMakeLists.txt)...")
        self._log("phase1", "This finds every compile target and the feature flags it uses.")

        config_map_out = self.output_dir / "build_config_map.json"
        build_dir      = self.output_dir / "cmake-build"

        r = self._run("phase1", [
            self.tools["python"],
            str(REPO_ROOT / "extractor" / "extractor.py"),
            "--project",   str(self.project_path),
            "--build-dir", str(build_dir),
            "--output",    str(config_map_out),
            "--cmake-bin", shutil.which("cmake") or "cmake",
            "--verbose",
        ])
        if r.returncode != 0:
            raise RuntimeError("Phase 1 failed — see log above")

        self.config_map_path = config_map_out
        # compile_commands.json lives in the cmake build dir
        self.compile_commands = build_dir / "compile_commands.json"
        if not self.compile_commands.exists():
            raise RuntimeError(f"compile_commands.json not found at {self.compile_commands}")

        data = json.loads(config_map_out.read_text())
        targets = list(data["targets"].keys())
        self._log("phase1", f"✓ Found {len(targets)} target(s): {', '.join(targets)}")
        return config_map_out

    # ── Phase 2: IfdefMapper ──────────────────────────────────────────────────

    def run_phase2(self) -> Path:
        if not self.compile_commands:
            raise RuntimeError("Run Phase 1 first")

        self._log("phase2", "Scanning source files for #ifdef / #ifndef / #if blocks...")
        self._log("phase2", "We record exactly which lines each conditional guard covers.")

        mapper = plugin_path("IfdefMapper")
        if not mapper.exists():
            raise RuntimeError(f"IfdefMapper plugin not found: {mapper}")

        commands = json.loads(self.compile_commands.read_text())
        merged   = {"tool": "IfdefMapper", "files": {}}
        total    = len(commands)

        for i, entry in enumerate(commands):
            source = entry.get("file", "")
            if not source or not Path(source).exists():
                continue

            defines, includes, std = parse_entry_flags(entry)
            out_file = self.ir_dir / f"ast_{i}.json"

            r = self._run("phase2", [
                self.tools["clangxx"],
                "-Xclang", "-load", "-Xclang", str(mapper),
                "-Xclang", "-add-plugin", "-Xclang", "ifdef-mapper",
                "-Xclang", "-plugin-arg-ifdef-mapper",
                "-Xclang", f"output={out_file}",
                *defines, *includes, *std,
                "-fsyntax-only",
                source,
            ])

            if out_file.exists():
                data = json.loads(out_file.read_text())
                merged["files"].update(data.get("files", {}))

            self._log("phase2", f"  [{i+1}/{total}] {Path(source).name}")

        ast_out = self.output_dir / "ast_mapping.json"
        ast_out.write_text(json.dumps(merged, indent=2))
        self.ast_mapping_path = ast_out

        total_blocks = sum(len(v) for v in merged["files"].values())
        self._log("phase2", f"✓ Mapped {total_blocks} #ifdef block(s) across {len(merged['files'])} file(s)")
        return ast_out

    # ── Phase 3: LLVM IR reachability ─────────────────────────────────────────

    def run_phase3(self) -> Path:
        if not self.compile_commands:
            raise RuntimeError("Run Phase 1 first")

        self._log("phase3", "Compiling your project to LLVM IR (intermediate representation)...")
        self._log("phase3", "Then tracing the call graph to find every line that can actually execute.")

        pass_lib = plugin_path("DeadFeaturePass")
        if not pass_lib.exists():
            raise RuntimeError(f"DeadFeaturePass plugin not found: {pass_lib}")

        commands  = json.loads(self.compile_commands.read_text())
        bc_files: List[Path] = []
        total = len(commands)

        for i, entry in enumerate(commands):
            source = entry.get("file", "")
            if not source or not Path(source).exists():
                continue

            defines, includes, std = parse_entry_flags(entry)
            bc_out = self.ir_dir / f"src_{i}.bc"

            r = self._run("phase3", [
                self.tools["clangxx"],
                "-g", "-emit-llvm", "-c",
                *defines, *includes, *std,
                source, "-o", str(bc_out),
            ])
            if bc_out.exists():
                bc_files.append(bc_out)
            self._log("phase3", f"  [{i+1}/{total}] compiled {Path(source).name}")

        if not bc_files:
            raise RuntimeError("Phase 3: no bitcode files generated")

        # Link into whole-program IR
        wp = self.ir_dir / "whole_program.bc"
        r = self._run("phase3", [
            self.tools["llvm_link"],
            *[str(f) for f in bc_files],
            "-o", str(wp),
        ])
        if r.returncode != 0:
            raise RuntimeError("llvm-link failed")
        self._log("phase3", f"Linked {len(bc_files)} bitcode file(s) → whole_program.bc")

        # Run DeadFeaturePass
        reach_out = self.output_dir / "reachability.json"
        r = self._run("phase3", [
            self.tools["opt"],
            f"--load-pass-plugin={pass_lib}",
            "--passes=dead-feature-pass",
            f"--dead-feature-output={reach_out}",
            str(wp), "--disable-output",
        ])
        if r.returncode != 0:
            raise RuntimeError("DeadFeaturePass failed")

        self.reachability_path = reach_out
        data = json.loads(reach_out.read_text())
        n_fn = len(data.get("reachable_functions", []))
        self._log("phase3", f"✓ Traced {n_fn} reachable function(s)")
        return reach_out

    # ── Phase 4: Correlate & report ───────────────────────────────────────────

    def run_phase4(self) -> Path:
        if not (self.config_map_path and self.ast_mapping_path and self.reachability_path):
            raise RuntimeError("Run Phases 1–3 first")

        self._log("phase4", "Cross-referencing build configs, #ifdef locations, and reachability data...")
        self._log("phase4", "Any #ifdef branch that's never reached in any config = dead feature.")

        report_out   = self.output_dir / "report.md"
        dead_out     = self.output_dir / "dead_features.json"

        r = self._run("phase4", [
            self.tools["python"],
            str(REPO_ROOT / "correlator.py"),
            "--config-map",   str(self.config_map_path),
            "--ast-mapping",  str(self.ast_mapping_path),
            "--reachability", str(self.reachability_path),
            "--report",       str(report_out),
            "--json-out",     str(dead_out),
            "--project-root", str(self.project_path),
        ])
        if r.returncode != 0:
            raise RuntimeError("Phase 4 (correlator) failed")

        self.report_path        = report_out
        self.dead_features_path = dead_out

        data = json.loads(dead_out.read_text())
        s    = data["summary"]
        self._log("phase4",
                  f"✓ Found {s['total']} dead branch(es): "
                  f"{s['high']} HIGH, {s['medium']} MEDIUM — "
                  f"{s['high_loc_removable']} line(s) safely removable")
        return report_out

    # ── Full pipeline ─────────────────────────────────────────────────────────

    def run_all(self) -> dict:
        """Run all four phases end-to-end. Returns final summary dict."""
        self.ensure_plugins_built()
        self.run_phase1()
        self.run_phase2()
        self.run_phase3()
        self.run_phase4()

        dead_data = json.loads(self.dead_features_path.read_text())
        return {
            "config_map":    str(self.config_map_path),
            "ast_mapping":   str(self.ast_mapping_path),
            "reachability":  str(self.reachability_path),
            "report":        str(self.report_path),
            "dead_features": str(self.dead_features_path),
            "summary":       dead_data["summary"],
        }


# ── CLI entry point ────────────────────────────────────────────────────────────

def main() -> int:
    import argparse
    p = argparse.ArgumentParser(
        description="Dead Feature Detector — run all analysis phases on a CMake project.")
    p.add_argument("--project",    required=True, help="Path to CMake project")
    p.add_argument("--output-dir", default=None,  help="Output directory (default: <project>/dfd-output)")
    p.add_argument("--llvm-config",default=None,  help="Path to llvm-config")
    args = p.parse_args()

    out = args.output_dir or str(Path(args.project) / "dfd-output")

    def progress(phase: str, msg: str) -> None:
        print(f"\033[36m[{phase}]\033[0m {msg}")

    try:
        pipeline = DeadFeaturePipeline(
            project_path=args.project,
            output_dir=out,
            llvm_config=args.llvm_config,
            progress=progress,
        )
        result = pipeline.run_all()
        print("\n\033[32m✓ Analysis complete!\033[0m")
        print(f"  Report:  {result['report']}")
        print(f"  Summary: {result['summary']}")
        return 0
    except Exception as e:
        print(f"\n\033[31m✗ Error: {e}\033[0m", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
