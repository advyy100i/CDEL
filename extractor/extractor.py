#!/usr/bin/env python3
"""Extract per-target preprocessor defines from a CMake project.

This script configures a CMake project with compile_commands enabled,
parses compile_commands.json, and emits a per-target mapping of -D flags.
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import re
import shlex
import subprocess
import sys
from glob import glob
from collections import defaultdict
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Set


LOG = logging.getLogger("extractor")


class ExtractorError(RuntimeError):
    """Raised when extraction fails with a user-facing message."""


@dataclass
class FileCompileInfo:
    source: str
    defines: Set[str] = field(default_factory=set)


@dataclass
class TargetInfo:
    defines: Set[str] = field(default_factory=set)
    files: List[FileCompileInfo] = field(default_factory=list)


def configure_logging(verbose: bool) -> None:
    level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(level=level, format="[%(levelname)s] %(message)s")


def run_command(cmd: Sequence[str], cwd: Optional[Path] = None) -> None:
    LOG.debug("Running command: %s", " ".join(cmd))
    try:
        result = subprocess.run(
            cmd,
            cwd=str(cwd) if cwd else None,
            text=True,
            capture_output=True,
            check=True,
        )
    except FileNotFoundError as exc:
        raise ExtractorError(f"Command not found: {cmd[0]}") from exc
    except subprocess.CalledProcessError as exc:
        stderr = (exc.stderr or "").strip()
        stdout = (exc.stdout or "").strip()
        details = stderr or stdout or "No output captured"
        raise ExtractorError(
            f"Command failed ({' '.join(cmd)}):\n{details}"
        ) from exc

    if result.stdout:
        LOG.debug("stdout:\n%s", result.stdout.strip())
    if result.stderr:
        LOG.debug("stderr:\n%s", result.stderr.strip())


def parse_arguments(entry: Dict[str, object]) -> List[str]:
    if "arguments" in entry and isinstance(entry["arguments"], list):
        return [str(token) for token in entry["arguments"]]

    command = entry.get("command")
    if not isinstance(command, str):
        raise ExtractorError("compile_commands entry missing 'arguments' and 'command'")

    try:
        return shlex.split(command, posix=os.name != "nt")
    except ValueError as exc:
        raise ExtractorError(f"Unable to parse command line: {command}") from exc


def normalize_define(raw: str) -> str:
    token = raw.strip()
    if token.startswith("-D"):
        token = token[2:]
    elif token.startswith("/D"):
        token = token[2:]
    return token.strip()


def extract_defines(args: Sequence[str]) -> Set[str]:
    defines: Set[str] = set()
    i = 0
    while i < len(args):
        arg = args[i]
        if arg in ("-D", "/D"):
            if i + 1 < len(args):
                value = normalize_define(args[i + 1])
                if value:
                    defines.add(value)
                i += 2
                continue
        elif arg.startswith("-D") or arg.startswith("/D"):
            value = normalize_define(arg)
            if value:
                defines.add(value)
        i += 1
    return defines


TARGET_FROM_CMAKE_OBJECT = re.compile(r"CMakeFiles[/\\]([^/\\]+)\.dir[/\\]")


def infer_target_name(entry: Dict[str, object], args: Sequence[str]) -> str:
    output = entry.get("output")
    if isinstance(output, str):
        match = TARGET_FROM_CMAKE_OBJECT.search(output)
        if match:
            return match.group(1)

    for idx, arg in enumerate(args):
        if arg == "-o" and idx + 1 < len(args):
            out_path = args[idx + 1]
            match = TARGET_FROM_CMAKE_OBJECT.search(out_path)
            if match:
                return match.group(1)

    src = entry.get("file")
    if isinstance(src, str):
        stem = Path(src).stem
        return f"unknown::{stem}"

    return "unknown::entry"


def load_compile_commands(path: Path) -> List[Dict[str, object]]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise ExtractorError(f"Unable to read compile_commands: {path}") from exc
    except json.JSONDecodeError as exc:
        raise ExtractorError(f"Invalid JSON in compile_commands: {path}") from exc

    if not isinstance(data, list):
        raise ExtractorError("compile_commands.json root must be a list")

    normalized: List[Dict[str, object]] = []
    for item in data:
        if isinstance(item, dict):
            normalized.append(item)
    if not normalized:
        raise ExtractorError("compile_commands.json has no valid compile entries")
    return normalized


def ensure_cmake_project(project_root: Path) -> None:
    if not project_root.exists() or not project_root.is_dir():
        raise ExtractorError(f"Project path does not exist or is not a directory: {project_root}")
    cmake_lists = project_root / "CMakeLists.txt"
    if not cmake_lists.exists():
        raise ExtractorError(f"CMakeLists.txt not found at: {cmake_lists}")


def detect_cpp_compiler() -> Optional[str]:
    from shutil import which

    for candidate in ("clang++", "g++", "cl", "c++"):
        path = which(candidate)
        if path:
            return path
    return None


def resolve_ninja_binary(cmake_exe: str) -> Optional[str]:
    # Prefer Ninja for reproducibility and to avoid NMake requirements.
    from shutil import which

    located = which("ninja")
    if located:
        return located

    cmake_dir = Path(cmake_exe).parent
    local_ninja = cmake_dir / ("ninja.exe" if os.name == "nt" else "ninja")
    if local_ninja.exists():
        return str(local_ninja)
    return None


def write_fake_compiler(compiler_path: Path) -> None:
    compiler_path.parent.mkdir(parents=True, exist_ok=True)
    if os.name == "nt":
                content = r"""@echo off
setlocal EnableDelayedExpansion
if "%1"=="--version" (
    echo fake-cxx 1.0
    exit /b 0
)
if "%1"=="-v" (
    echo fake-cxx 1.0
    exit /b 0
)

set "out="
set "nextOut=0"
:loop
if "%~1"=="" goto done
if "!nextOut!"=="1" (
    set "out=%~1"
    set "nextOut=0"
    shift
    goto loop
)
if "%~1"=="-o" (
    set "nextOut=1"
    shift
    goto loop
)
if /I "%~1:~0,3"=="/Fo" (
    set "out=%~1:~3%"
)
shift
goto loop

:done
if not "%out%"=="" (
    for %%I in ("%out%") do set "outdir=%%~dpI"
    if not "%outdir%"=="" if not exist "%outdir%" mkdir "%outdir%"
    type nul > "%out%"
)
exit /b 0
"""
    else:
        content = """#!/usr/bin/env sh
if [ "$1" = "--version" ] || [ "$1" = "-v" ]; then
    echo "fake-cxx 1.0"
    exit 0
fi

out=""
prev=""
for arg in "$@"; do
    if [ "$prev" = "-o" ]; then
        out="$arg"
        prev=""
        continue
    fi
    if [ "$arg" = "-o" ]; then
        prev="-o"
        continue
    fi
done

if [ -n "$out" ]; then
    mkdir -p "$(dirname "$out")"
    : > "$out"
fi
exit 0
"""

    compiler_path.write_text(content, encoding="utf-8")
    if os.name != "nt":
        compiler_path.chmod(0o755)


def resolve_cmake_binary(cmake_bin: str) -> str:
    from shutil import which

    if os.path.isabs(cmake_bin) and Path(cmake_bin).exists():
        return cmake_bin

    located = which(cmake_bin)
    if located:
        return located

    candidates: List[str] = []
    env_cmake = os.environ.get("CMAKE_EXE")
    if env_cmake:
        candidates.append(env_cmake)

    if os.name == "nt":
        candidates.extend(
            [
                r"C:\Program Files\CMake\bin\cmake.exe",
                r"C:\Program Files (x86)\CMake\bin\cmake.exe",
            ]
        )
        candidates.extend(
            glob(
                r"C:\Program Files\Microsoft Visual Studio\*\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            )
        )

    for candidate in candidates:
        if Path(candidate).exists():
            return candidate

    raise ExtractorError(
        "Unable to locate CMake executable. Install CMake or pass --cmake-bin <path-to-cmake>."
    )


def configure_cmake(project_root: Path, build_dir: Path, cmake_bin: str) -> Path:
    build_dir.mkdir(parents=True, exist_ok=True)
    cmake_exe = resolve_cmake_binary(cmake_bin)
    cmd = [
        cmake_exe,
        "-S",
        str(project_root),
        "-B",
        str(build_dir),
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    ]

    ninja = resolve_ninja_binary(cmake_exe)
    if ninja:
        cmd.extend(["-G", "Ninja", f"-DCMAKE_MAKE_PROGRAM={ninja}"])

    compiler = detect_cpp_compiler()
    if not compiler:
        fake_name = "fake-cxx.cmd" if os.name == "nt" else "fake-cxx.sh"
        fake_compiler = build_dir / fake_name
        write_fake_compiler(fake_compiler)
        LOG.warning(
            "No C++ compiler detected. Falling back to metadata-only mode using %s",
            fake_compiler,
        )
        cmd.extend(
            [
                f"-DCMAKE_CXX_COMPILER={fake_compiler}",
                "-DCMAKE_CXX_COMPILER_WORKS=1",
                "-DCMAKE_CXX_COMPILER_FORCED=TRUE",
            ]
        )

    run_command(cmd)

    compile_commands = build_dir / "compile_commands.json"
    if not compile_commands.exists():
        raise ExtractorError(
            f"compile_commands.json not generated at expected path: {compile_commands}"
        )
    return compile_commands


def analyze_compile_commands(entries: Iterable[Dict[str, object]]) -> Dict[str, TargetInfo]:
    targets: Dict[str, TargetInfo] = defaultdict(TargetInfo)
    for entry in entries:
        args = parse_arguments(entry)
        defines = extract_defines(args)
        target = infer_target_name(entry, args)

        source = str(entry.get("file", "<unknown-file>"))
        file_info = FileCompileInfo(source=source, defines=set(defines))
        targets[target].files.append(file_info)
        targets[target].defines.update(defines)
    return targets


def write_output(
    output_path: Path,
    project_root: Path,
    build_dir: Path,
    targets: Dict[str, TargetInfo],
) -> None:
    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "project_root": str(project_root.resolve()),
        "build_dir": str(build_dir.resolve()),
        "targets": {},
    }

    for target_name in sorted(targets.keys()):
        info = targets[target_name]
        payload["targets"][target_name] = {
            "defines": sorted(info.defines),
            "files": [
                {
                    "source": item.source,
                    "defines": sorted(item.defines),
                }
                for item in sorted(info.files, key=lambda f: f.source)
            ],
        }

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Extract target-wise preprocessor define sets from CMake compile commands."
    )
    parser.add_argument("--project", required=True, help="Path to the CMake project")
    parser.add_argument(
        "--build-dir",
        default=None,
        help="Build directory to generate compile_commands.json (defaults to <project>/build-extractor)",
    )
    parser.add_argument(
        "--output",
        required=True,
        help="Path to output JSON mapping of targets to defines",
    )
    parser.add_argument("--cmake-bin", default="cmake", help="CMake binary name/path")
    parser.add_argument("--verbose", action="store_true", help="Enable debug logging")
    return parser.parse_args(argv)


def main(argv: Sequence[str]) -> int:
    args = parse_args(argv)
    configure_logging(args.verbose)

    project_root = Path(args.project).resolve()
    build_dir = Path(args.build_dir).resolve() if args.build_dir else project_root / "build-extractor"
    output_path = Path(args.output).resolve()

    try:
        ensure_cmake_project(project_root)
        compile_commands_path = configure_cmake(project_root, build_dir, args.cmake_bin)
        entries = load_compile_commands(compile_commands_path)
        target_info = analyze_compile_commands(entries)
        write_output(output_path, project_root, build_dir, target_info)
    except ExtractorError as exc:
        LOG.error(str(exc))
        return 1
    except Exception as exc:  # pylint: disable=broad-except
        LOG.exception("Unexpected failure: %s", exc)
        return 2

    LOG.info("Extraction completed. Wrote %s targets to %s", len(target_info), output_path)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
