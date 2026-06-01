#!/usr/bin/env python3
"""
baseline_compare.py — Naive dead-feature baseline using grep-style analysis.

Implements two baseline approaches and compares them against the full 4-phase
tool output, producing a side-by-side precision/recall comparison.

Baseline 1 (GREP):  find all #ifdef/#ifndef macros in source files, check if
                    the macro appears *anywhere* in CMakeLists.txt.

Baseline 2 (CMAKE): use Phase 1 output (build_config_map.json) to check
                    per-target defines without IR reachability.

Usage:
    python3 baseline/baseline_compare.py \\
        --project   <cmake_project_dir> \\
        --tool-out  <dead_features.json>   # Phase 4 output
        [--config-map  <build_config_map.json>]  # Phase 1 output (enables Baseline 2)
        [--verbose]

Output:
    Prints a comparison table of findings from each approach, plus
    precision/recall metrics relative to the full tool's output as ground truth.
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path


# ── helpers ───────────────────────────────────────────────────────────────────

def find_source_files(project_dir: Path) -> list[Path]:
    """Recursively find all .cpp, .cc, .cxx, .c, .h, .hpp files."""
    exts = {'.cpp', '.cc', '.cxx', '.c', '.h', '.hpp', '.hh'}
    out = []
    for root, dirs, files in os.walk(project_dir):
        # Skip build directories
        dirs[:] = [d for d in dirs if d not in ('build', 'build-extractor', '.git', '__pycache__')]
        for f in files:
            if Path(f).suffix in exts:
                out.append(Path(root) / f)
    return out


def extract_ifdef_macros_from_source(source_files: list[Path]) -> dict[str, list[tuple[Path, int]]]:
    """
    Returns {macro_name: [(file, line), ...]} for every #ifdef / #ifndef in source.
    """
    pattern = re.compile(r'^\s*#\s*ifn?def\s+(\w+)', re.MULTILINE)
    result: dict[str, list] = {}
    for f in source_files:
        try:
            text = f.read_text(errors='replace')
        except OSError:
            continue
        for m in pattern.finditer(text):
            macro = m.group(1)
            line = text[:m.start()].count('\n') + 1
            result.setdefault(macro, []).append((f, line))
    return result


def extract_macros_from_cmake(project_dir: Path) -> set[str]:
    """
    Baseline 1: grep CMakeLists.txt (recursively) for any macro-like token.
    Finds: -DFOO, -DFOO=1, target_compile_definitions, add_definitions.
    """
    pattern = re.compile(r'-D([A-Z_][A-Z0-9_]+)(?:=\S*)?|target_compile_definitions\([^)]*?\b([A-Z_][A-Z0-9_]+)\b')
    found: set[str] = set()
    for cmake_file in project_dir.rglob('CMakeLists.txt'):
        try:
            text = cmake_file.read_text(errors='replace')
        except OSError:
            continue
        for m in re.finditer(r'-D([A-Z_][A-Z0-9_]+)(?:=[^\s)]*)?', text):
            found.add(m.group(1))
        for m in re.finditer(r'target_compile_definitions\s*\([^)]+\)', text, re.DOTALL):
            block = m.group(0)
            for tok in re.findall(r'\b([A-Z_][A-Z0-9_]{2,})\b', block):
                # Skip CMake keywords
                if tok not in ('PRIVATE', 'PUBLIC', 'INTERFACE', 'ON', 'OFF', 'TRUE', 'FALSE'):
                    found.add(tok)
    return found


def baseline1_grep(source_macros: dict, cmake_macros: set) -> set[str]:
    """
    Returns set of macros that appear in source but NOT found anywhere in CMakeLists.
    High false-positive rate: doesn't distinguish per-target or per-file.
    """
    return {m for m in source_macros if m not in cmake_macros}


def baseline2_cmake(source_macros: dict, config_map: dict) -> set[str]:
    """
    Returns macros that appear in source but are absent from ALL per-target defines
    in build_config_map.json. Equivalent to Phase 1 + Phase 2 without IR.
    """
    all_defined: set[str] = set()
    for target_info in config_map.get('targets', {}).values():
        for define in target_info.get('defines', []):
            name = define.split('=')[0]
            all_defined.add(name)
    return {m for m in source_macros if m not in all_defined}


def load_tool_output(tool_out_path: Path) -> list[dict]:
    """Load dead_features.json from Phase 4."""
    try:
        data = json.loads(tool_out_path.read_text())
        return data.get('dead_features', [])
    except (OSError, json.JSONDecodeError) as e:
        print(f"[baseline] WARNING: cannot read tool output: {e}", file=sys.stderr)
        return []


def compute_metrics(found: set[str], ground_truth_macros: set[str], all_macros: set[str]) -> dict:
    """Compute precision/recall where ground_truth_macros = tool's HIGH confidence findings."""
    tp = len(found & ground_truth_macros)
    fp = len(found - ground_truth_macros)
    fn = len(ground_truth_macros - found)
    tn = len(all_macros - found - ground_truth_macros)
    precision = tp / (tp + fp) if (tp + fp) > 0 else 1.0
    recall    = tp / (tp + fn) if (tp + fn) > 0 else 1.0
    f1        = 2 * precision * recall / (precision + recall) if (precision + recall) > 0 else 0.0
    return {'TP': tp, 'FP': fp, 'FN': fn, 'TN': tn,
            'precision': precision, 'recall': recall, 'F1': f1}


def print_table(rows: list[tuple], headers: list[str]):
    col_w = [max(len(h), max((len(str(r[i])) for r in rows), default=0))
             for i, h in enumerate(headers)]
    sep = '+-' + '-+-'.join('-' * w for w in col_w) + '-+'
    fmt = '| ' + ' | '.join(f'{{:<{w}}}' for w in col_w) + ' |'
    print(sep)
    print(fmt.format(*headers))
    print(sep)
    for row in rows:
        print(fmt.format(*[str(v) for v in row]))
    print(sep)


# ── main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description='Compare naive baseline approaches against the Dead Feature Detector.')
    parser.add_argument('--project',    required=True,  help='CMake project directory')
    parser.add_argument('--tool-out',   required=True,  help='dead_features.json from Phase 4')
    parser.add_argument('--config-map', default=None,   help='build_config_map.json from Phase 1 (enables Baseline 2)')
    parser.add_argument('--verbose',    action='store_true')
    args = parser.parse_args()

    project_dir  = Path(args.project).resolve()
    tool_out     = Path(args.tool_out).resolve()
    config_map   = None

    if not project_dir.is_dir():
        print(f"ERROR: project dir not found: {project_dir}", file=sys.stderr)
        sys.exit(1)

    # ── load data ─────────────────────────────────────────────────────────────
    source_files  = find_source_files(project_dir)
    source_macros = extract_ifdef_macros_from_source(source_files)
    cmake_macros  = extract_macros_from_cmake(project_dir)
    tool_findings = load_tool_output(tool_out)

    if args.config_map:
        try:
            config_map = json.loads(Path(args.config_map).read_text())
        except (OSError, json.JSONDecodeError) as e:
            print(f"[baseline] WARNING: cannot read config map: {e}", file=sys.stderr)

    # Ground truth: macros flagged HIGH confidence by the full tool
    gt_macros = {f['macro'] for f in tool_findings if f.get('confidence') == 'HIGH'}
    all_macros = set(source_macros.keys())

    # ── run baselines ─────────────────────────────────────────────────────────
    b1_dead = baseline1_grep(source_macros, cmake_macros)
    b2_dead = baseline2_cmake(source_macros, config_map) if config_map else None
    tool_dead = {f['macro'] for f in tool_findings}

    # ── report ────────────────────────────────────────────────────────────────
    print('\n' + '='*70)
    print('  DEAD FEATURE DETECTOR — Baseline Comparison')
    print('='*70)
    print(f'\n  Project  : {project_dir}')
    print(f'  Sources  : {len(source_files)} files, {len(source_macros)} unique #ifdef macros')
    print(f'  Ground truth (full tool HIGH findings): {sorted(gt_macros) or "(none)"}')

    print('\n── Findings per approach ─────────────────────────────────────────────')
    rows = []
    b1_m = compute_metrics(b1_dead, gt_macros, all_macros)
    rows.append(('Baseline 1 (grep CMakeLists)',
                 len(b1_dead),
                 f"{b1_m['precision']:.0%}",
                 f"{b1_m['recall']:.0%}",
                 f"{b1_m['F1']:.2f}",
                 f"TP={b1_m['TP']} FP={b1_m['FP']} FN={b1_m['FN']}"))
    if b2_dead is not None:
        b2_m = compute_metrics(b2_dead, gt_macros, all_macros)
        rows.append(('Baseline 2 (CMake-only, no IR)',
                     len(b2_dead),
                     f"{b2_m['precision']:.0%}",
                     f"{b2_m['recall']:.0%}",
                     f"{b2_m['F1']:.2f}",
                     f"TP={b2_m['TP']} FP={b2_m['FP']} FN={b2_m['FN']}"))
    tool_m = compute_metrics(tool_dead, gt_macros, all_macros)
    rows.append(('Full 4-phase tool (our tool)',
                 len(tool_dead),
                 f"{tool_m['precision']:.0%}",
                 f"{tool_m['recall']:.0%}",
                 f"{tool_m['F1']:.2f}",
                 f"TP={tool_m['TP']} FP={tool_m['FP']} FN={tool_m['FN']}"))

    print()
    print_table(rows, ['Approach', 'Found', 'Precision', 'Recall', 'F1', 'Breakdown'])

    if args.verbose:
        print('\n── Baseline 1 findings ───────────────────────────────────────────────')
        for m in sorted(b1_dead):
            tag = '✅ TP' if m in gt_macros else '❌ FP'
            locs = ', '.join(f'{f.name}:{ln}' for f, ln in source_macros[m][:3])
            print(f'  {tag}  {m:<35} ({locs})')

        if b2_dead is not None:
            print('\n── Baseline 2 findings ───────────────────────────────────────────────')
            for m in sorted(b2_dead):
                tag = '✅ TP' if m in gt_macros else '❌ FP'
                locs = ', '.join(f'{f.name}:{ln}' for f, ln in source_macros[m][:3])
                print(f'  {tag}  {m:<35} ({locs})')

        print('\n── Full tool findings ────────────────────────────────────────────────')
        for f in tool_findings:
            print(f"  [{f.get('confidence','?')}]  {f['macro']:<35}"
                  f"  {Path(f.get('source_file', f.get('file', '?'))).name}:{f['start_line']}-{f['end_line']}"
                  f"  ({f.get('loc', f.get('loc_removable', '?'))} LoC)")

    # ── LoC summary ───────────────────────────────────────────────────────────
    total_loc = sum(f.get('loc', f.get('loc_removable', 0)) for f in tool_findings if f.get('confidence') == 'HIGH')
    print(f'\n  Full tool — total HIGH-confidence LoC removable: {total_loc}')
    print(f'  (Baselines 1 & 2 cannot report LoC counts — no line-range information)\n')


if __name__ == '__main__':
    main()
