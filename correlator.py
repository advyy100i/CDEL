#!/usr/bin/env python3
"""correlator.py — Phase 4: Dead Feature Detector correlation engine.

Joins:
  Phase 1  build_config_map.json  — target → {defines}
  Phase 2  ast_mapping.json       — file  → [{#ifdef blocks with line ranges}]
  Phase 3  reachability.json      — file  → {reachable line numbers}

Outputs:
  report.md    — Markdown report of dead features with confidence + LoC savings
  dead_features.json — machine-readable version of the same data
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple


# ── data model ─────────────────────────────────────────────────────────────────

@dataclass
class DeadBranch:
    macro: str                      # guarding macro name
    branch_kind: str                # "ifdef" | "ifndef" | "if" | "else" | "elif"
    condition: str                  # raw condition text from source
    source_file: str
    start_line: int
    end_line: int
    confidence: str                 # "HIGH" | "MEDIUM"
    reason: str                     # human-readable explanation
    loc: int                        # body line count (excludes directive lines)
    configs_with_macro: List[str]   # configs where guarding macro is defined


# ── helpers ────────────────────────────────────────────────────────────────────

def strip_value(define: str) -> str:
    """'EXPERIMENTAL_FEATURE=1' → 'EXPERIMENTAL_FEATURE'."""
    return define.split("=")[0].strip()


def build_config_macro_sets(config_map: dict) -> Dict[str, Set[str]]:
    """Return {target_name: set_of_macro_names}."""
    result: Dict[str, Set[str]] = {}
    for target, info in config_map.get("targets", {}).items():
        result[target] = {strip_value(d) for d in info.get("defines", [])}
    return result


def build_file_config_sets(
    config_map: dict,
    all_config_sets: Dict[str, Set[str]],
) -> Dict[str, Dict[str, Set[str]]]:
    """Return {normalized_file_path: {target_name: macro_set}}.

    Only includes targets whose compile_commands explicitly list the file.
    This prevents configs that don't compile feature.cpp from polluting the
    dead-feature analysis of feature.cpp's #ifdef blocks.
    """
    result: Dict[str, Dict[str, Set[str]]] = {}
    for target, info in config_map.get("targets", {}).items():
        for file_entry in info.get("files", []):
            norm = normalize_path(file_entry["source"])
            if norm not in result:
                result[norm] = {}
            result[norm][target] = all_config_sets[target]
    return result


def normalize_path(p: str) -> str:
    """Resolve '..' components and return a canonical absolute path string."""
    return str(Path(p).resolve())


def branch_body_loc(start: int, end: int) -> int:
    """Lines of code in the branch body.

    Phase 2 plugin sets:
      branch.start_line = the directive line (#ifdef / #else)
      branch.end_line   = LAST body line (the line before the next directive)
                          i.e. already decremented by 1 in IfdefMapper.cpp

    So body = (start+1) .. end  inclusive  →  count = end - start
    """
    return max(0, end - start)


# ── deadness analysis ──────────────────────────────────────────────────────────

def configs_defining(macro: str, config_sets: Dict[str, Set[str]]) -> List[str]:
    """List of target names that define `macro`."""
    return [t for t, macros in config_sets.items() if macro in macros]


def configs_not_defining(macro: str, config_sets: Dict[str, Set[str]]) -> List[str]:
    return [t for t, macros in config_sets.items() if macro not in macros]


def analyze_branch(
    block_kind: str,
    block_condition: str,
    branch: dict,
    reachable_lines_for_file: Set[int],
    config_sets: Dict[str, Set[str]],   # filtered to configs that compile THIS file
) -> Optional[DeadBranch]:
    """
    Return a DeadBranch if this branch is dead, else None.

    Confidence rules:
      HIGH   — the branch can NEVER be compiled in any known build config:
               • ifdef/if branch:  macro never defined in any config
               • ifndef branch:    macro always defined in every config
               • else (after ifdef): parent macro always defined (else never active)
               • else (after ifndef): parent macro never defined (else never active)

      MEDIUM — the branch CAN be compiled in some config, but has zero IR
               coverage in the reachability data for those configs.
               (Could be runtime-dead or debug-info gap; needs manual review.)
    """
    kind      = branch["kind"]      # "ifdef" | "ifndef" | "if" | "elif" | "else"
    condition = branch.get("condition", "").strip()
    start     = branch["start_line"]
    end       = branch["end_line"]

    if start >= end:
        return None  # degenerate / empty branch, skip

    # Body line range (excluding the directive line at start)
    body_lines = set(range(start + 1, end + 1))
    covered    = body_lines & reachable_lines_for_file
    macro      = condition if condition else block_condition

    # ── determine "active condition" and check configs ─────────────────────────

    if kind in ("ifdef", "if"):
        active_macro  = macro
        configs_on    = configs_defining(active_macro, config_sets)
        configs_off   = configs_not_defining(active_macro, config_sets)

        if not configs_on:
            # Macro is never defined → statically dead
            return DeadBranch(
                macro=active_macro, branch_kind=kind, condition=condition,
                source_file="", start_line=start, end_line=end,
                confidence="HIGH",
                reason=f"`{active_macro}` is not defined in any known build configuration.",
                loc=branch_body_loc(start, end),
                configs_with_macro=[],
            )
        if not covered:
            return DeadBranch(
                macro=active_macro, branch_kind=kind, condition=condition,
                source_file="", start_line=start, end_line=end,
                confidence="MEDIUM",
                reason=(f"`{active_macro}` is defined in {len(configs_on)} config(s) "
                        f"({', '.join(configs_on)}) but these lines have no IR "
                        f"coverage in the reachability data."),
                loc=branch_body_loc(start, end),
                configs_with_macro=configs_on,
            )

    elif kind == "ifndef":
        active_macro  = macro
        configs_off   = configs_not_defining(active_macro, config_sets)
        configs_on    = configs_defining(active_macro, config_sets)

        if not configs_off:
            # Macro is always defined → ifndef branch is statically dead
            return DeadBranch(
                macro=active_macro, branch_kind=kind, condition=condition,
                source_file="", start_line=start, end_line=end,
                confidence="HIGH",
                reason=f"`{active_macro}` is defined in ALL known build configurations; "
                       f"the `#ifndef` branch is never compiled.",
                loc=branch_body_loc(start, end),
                configs_with_macro=list(config_sets.keys()),
            )
        if not covered:
            return DeadBranch(
                macro=active_macro, branch_kind=kind, condition=condition,
                source_file="", start_line=start, end_line=end,
                confidence="MEDIUM",
                reason=(f"`{active_macro}` is absent in {len(configs_off)} config(s) "
                        f"({', '.join(configs_off)}) but the branch has no IR coverage."),
                loc=branch_body_loc(start, end),
                configs_with_macro=configs_off,
            )

    elif kind in ("else", "elif"):
        # Else/elif is active when the PARENT condition is NOT satisfied.
        parent_macro = block_condition.strip()
        if block_kind in ("ifdef", "if"):
            # parent was #ifdef FOO → else active when FOO is NOT defined
            active_when_macro_absent = True
            configs_active = configs_not_defining(parent_macro, config_sets)
        elif block_kind == "ifndef":
            # parent was #ifndef FOO → else active when FOO IS defined
            active_when_macro_absent = False
            configs_active = configs_defining(parent_macro, config_sets)
        else:
            configs_active = list(config_sets.keys())  # conservative

        if not configs_active:
            reason = (
                f"The `#else` branch (after `#{block_kind} {parent_macro}`) is never "
                f"active: `{parent_macro}` is {'never' if not active_when_macro_absent else 'always'} "  # noqa
                f"defined in all known configurations."
            )
            return DeadBranch(
                macro=parent_macro, branch_kind=kind, condition=condition,
                source_file="", start_line=start, end_line=end,
                confidence="HIGH",
                reason=reason,
                loc=branch_body_loc(start, end),
                configs_with_macro=configs_defining(parent_macro, config_sets),
            )
        if not covered:
            return DeadBranch(
                macro=parent_macro, branch_kind=kind, condition=condition,
                source_file="", start_line=start, end_line=end,
                confidence="MEDIUM",
                reason=(f"The `#{kind}` branch (after `#{block_kind} {parent_macro}`) "
                        f"has no IR coverage in {len(configs_active)} applicable config(s)."),
                loc=branch_body_loc(start, end),
                configs_with_macro=configs_active,
            )

    return None


def correlate(
    config_map:    dict,
    ast_mapping:   dict,
    reachability:  dict,
) -> List[DeadBranch]:

    all_config_sets  = build_config_macro_sets(config_map)
    file_config_sets = build_file_config_sets(config_map, all_config_sets)

    # Normalize reachable_lines keys
    reach_norm: Dict[str, Set[int]] = {
        normalize_path(k): set(v)
        for k, v in reachability.get("reachable_lines", {}).items()
    }

    dead: List[DeadBranch] = []

    for raw_file, blocks in ast_mapping.get("files", {}).items():
        norm_file = normalize_path(raw_file)
        reachable = reach_norm.get(norm_file, set())

        # Use per-file config sets when available; fall back to all configs.
        file_cfgs = file_config_sets.get(norm_file, all_config_sets)

        for block in blocks:
            block_kind = block["kind"]
            block_cond = block["condition"].strip()

            for branch in block.get("branches", []):
                result = analyze_branch(
                    block_kind, block_cond,
                    branch, reachable, file_cfgs,
                )
                if result is not None:
                    result.source_file = raw_file
                    dead.append(result)

    # Sort: HIGH first, then by file + line
    dead.sort(key=lambda d: (0 if d.confidence == "HIGH" else 1,
                              d.source_file, d.start_line))
    return dead


# ── report generation ──────────────────────────────────────────────────────────

def rel_file(path: str, project_root: Optional[str]) -> str:
    if project_root:
        try:
            return str(Path(path).relative_to(project_root))
        except ValueError:
            pass
    return os.path.basename(path)


def write_markdown(
    dead:         List[DeadBranch],
    config_map:   dict,
    out_path:     str,
    project_root: Optional[str] = None,
) -> None:
    now    = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    root   = config_map.get("project_root", "")
    high   = [d for d in dead if d.confidence == "HIGH"]
    medium = [d for d in dead if d.confidence == "MEDIUM"]
    total_loc = sum(d.loc for d in dead)
    high_loc  = sum(d.loc for d in high)

    lines: List[str] = []
    A = lines.append

    A("# Dead Feature Analysis Report")
    A("")
    A(f"**Generated:** {now}  ")
    A(f"**Project:** `{root}`  ")
    A(f"**Build configs analysed:** {len(config_map.get('targets', {}))}  ")
    A("")
    A("## Summary")
    A("")
    A(f"| Metric | Value |")
    A(f"|--------|-------|")
    A(f"| Total dead branches found | {len(dead)} |")
    A(f"| HIGH confidence (statically dead) | {len(high)} |")
    A(f"| MEDIUM confidence (coverage-based) | {len(medium)} |")
    A(f"| Estimated removable lines (HIGH) | **{high_loc}** |")
    A(f"| Estimated removable lines (total) | {total_loc} |")
    A("")

    # ── build config table ────────────────────────────────────────────────────
    A("## Build Configurations (Phase 1)")
    A("")
    A("| Target | Active Defines |")
    A("|--------|----------------|")
    for target, info in config_map.get("targets", {}).items():
        defines = ", ".join(f"`{d}`" for d in sorted(info.get("defines", [])))
        A(f"| `{target}` | {defines} |")
    A("")

    # ── HIGH confidence ───────────────────────────────────────────────────────
    A("## HIGH Confidence — Statically Dead Features")
    A("")
    A("These branches are **never compiled** under any known build configuration.")
    A("Safe to remove without any build-config flag changes.")
    A("")
    if high:
        A("| Macro | Kind | File | Lines | LoC Saved | Reason |")
        A("|-------|------|------|-------|-----------|--------|")
        for d in high:
            f = rel_file(d.source_file, project_root or root)
            A(f"| `{d.macro}` | `#{d.branch_kind}` | `{f}` "
              f"| {d.start_line}–{d.end_line} | {d.loc} | {d.reason} |")
        A("")
        A("### Details")
        A("")
        for d in high:
            f = rel_file(d.source_file, project_root or root)
            A(f"#### `{d.macro}` — `#{d.branch_kind}` @ `{f}:{d.start_line}`")
            A("")
            A(f"- **File:** `{d.source_file}`")
            A(f"- **Lines:** {d.start_line}–{d.end_line}  ({d.loc} body lines)")
            A(f"- **Branch type:** `#{d.branch_kind}`")
            if d.condition:
                A(f"- **Condition:** `{d.condition}`")
            A(f"- **Reason:** {d.reason}")
            A(f"- **Estimated saving:** {d.loc} lines")
            A("")
    else:
        A("*No statically dead features found.*")
        A("")

    # ── MEDIUM confidence ─────────────────────────────────────────────────────
    A("## MEDIUM Confidence — Coverage-Based Dead Features")
    A("")
    A("These branches _could_ be compiled in some configuration but have zero "
      "IR coverage in the reachability data.  May be runtime-dead, inlined "
      "away, or a debug-info gap.  Requires manual review before removal.")
    A("")
    if medium:
        A("| Macro | Kind | File | Lines | LoC | Reason |")
        A("|-------|------|------|-------|-----|--------|")
        for d in medium:
            f = rel_file(d.source_file, project_root or root)
            A(f"| `{d.macro}` | `#{d.branch_kind}` | `{f}` "
              f"| {d.start_line}–{d.end_line} | {d.loc} | {d.reason} |")
    else:
        A("*No medium-confidence dead features found.*")
    A("")

    # ── footer ────────────────────────────────────────────────────────────────
    A("---")
    A("*Generated by Dead Feature Detector — "
      "[Assignment 29](../README.md)*")

    Path(out_path).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[correlator] Markdown report → {out_path}")


def write_json_output(dead: List[DeadBranch], out_path: str) -> None:
    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "summary": {
            "total": len(dead),
            "high":   sum(1 for d in dead if d.confidence == "HIGH"),
            "medium": sum(1 for d in dead if d.confidence == "MEDIUM"),
            "total_loc_removable": sum(d.loc for d in dead),
            "high_loc_removable":  sum(d.loc for d in dead if d.confidence == "HIGH"),
        },
        "dead_features": [
            {
                "macro":              d.macro,
                "branch_kind":        d.branch_kind,
                "condition":          d.condition,
                "source_file":        d.source_file,
                "start_line":         d.start_line,
                "end_line":           d.end_line,
                "confidence":         d.confidence,
                "reason":             d.reason,
                "loc":                d.loc,
                "configs_with_macro": d.configs_with_macro,
            }
            for d in dead
        ],
    }
    Path(out_path).write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(f"[correlator] JSON output      → {out_path}")


# ── CLI ────────────────────────────────────────────────────────────────────────

def parse_args(argv: list) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Dead Feature Detector — Phase 4 correlation engine."
    )
    p.add_argument("--config-map",   required=True,
                   help="Phase 1 output: build_config_map.json")
    p.add_argument("--ast-mapping",  required=True,
                   help="Phase 2 output: ast_mapping.json")
    p.add_argument("--reachability", required=True,
                   help="Phase 3 output: reachability.json")
    p.add_argument("--report",       default="report.md",
                   help="Output Markdown report path (default: report.md)")
    p.add_argument("--json-out",     default="dead_features.json",
                   help="Output JSON path (default: dead_features.json)")
    p.add_argument("--project-root", default=None,
                   help="Project root for relative paths in report")
    return p.parse_args(argv)


def main(argv: list) -> int:
    args = parse_args(argv)

    def load(path: str) -> dict:
        try:
            return json.loads(Path(path).read_text(encoding="utf-8"))
        except FileNotFoundError:
            print(f"ERROR: file not found: {path}", file=sys.stderr)
            sys.exit(1)
        except json.JSONDecodeError as e:
            print(f"ERROR: invalid JSON in {path}: {e}", file=sys.stderr)
            sys.exit(1)

    config_map   = load(args.config_map)
    ast_mapping  = load(args.ast_mapping)
    reachability = load(args.reachability)

    dead = correlate(config_map, ast_mapping, reachability)

    print(f"[correlator] Found {len(dead)} dead branch(es) "
          f"({sum(1 for d in dead if d.confidence=='HIGH')} HIGH, "
          f"{sum(1 for d in dead if d.confidence=='MEDIUM')} MEDIUM)")
    print(f"[correlator] Estimated removable lines (HIGH): "
          f"{sum(d.loc for d in dead if d.confidence=='HIGH')}")

    write_markdown(dead, config_map, args.report, args.project_root)
    write_json_output(dead, args.json_out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
