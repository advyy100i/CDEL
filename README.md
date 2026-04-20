# Assignment 29: Dead Feature Detector

A whole-program LLVM/Clang analysis tool that identifies code regions guarded by preprocessor flags or runtime feature toggles that are **unreachable under any actual build configuration**. Traditional dead-code elimination works per translation unit; this tool combines build-system-level configuration analysis with IR-level reachability to find feature-guarded blocks that are dead **across all real configurations**.

---

## Pipeline Overview

```
CMake Project
      │
      ▼
[Phase 1] extractor.py
  → build_config_map.json   (target → {defines})
      │
      ▼
[Phase 2] Clang Plugin (IfdefMapper)
  → ast_mapping.json         (#ifdef blocks → {file, lines, macro condition})
      │
      ▼
[Phase 3] LLVM LTO Pass (DeadFeaturePass)
  → reachability.json        (functions/blocks reachable from main)
      │
      ▼
[Phase 4] correlator.py
  → report.md                (dead features with confidence scores & LoC savings)
      │
      ▼
[Phase 5] Evaluation on open-source project (zlib / sqlite)
```

---

## Phase Status

| Phase | Component | Status |
|-------|-----------|--------|
| 1 | CMake define extractor (`extractor/extractor.py`) | ✅ Complete |
| 2 | Clang preprocessor/AST plugin (`llvm-pass/IfdefMapper`) | ✅ Complete |
| 3 | LLVM whole-program reachability pass (`llvm-pass/DeadFeaturePass`) | ✅ Complete |
| 4 | Correlation engine & Markdown reporter (`correlator.py`) | ✅ Complete |
| 5 | Evaluation on `zlib` / `sqlite` | 🔲 Planned |

---

## Phase 1: Build Configuration Extractor

### What it does

`extractor/extractor.py` takes a CMake project, forces a CMake configure with
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, parses `compile_commands.json`, and emits
a JSON file mapping each CMake target to the exact `-D` preprocessor flags used
to compile it.

Falls back to a **fake no-op compiler** (`fake-cxx.sh` / `fake-cxx.cmd`) written
into the build directory when no real C++ toolchain is present, allowing
metadata-only extraction without a full toolchain.

### Usage

```bash
python3 extractor/extractor.py \
  --project <path/to/cmake/project> \
  --output  <path/to/output.json> \
  [--build-dir <build-dir>]      # default: <project>/build-extractor
  [--cmake-bin <cmake-path>]     # default: cmake on PATH
  [--verbose]
```

### Test (macOS / Linux)

```bash
bash tests/run_phase1_test.sh
```

### Test (Windows PowerShell)

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\run_phase1_test.ps1
```

### Output format (`build_config_map.json`)

```json
{
  "generated_at": "<ISO-8601 UTC>",
  "project_root": "/abs/path/to/project",
  "build_dir":    "/abs/path/to/build",
  "targets": {
    "dummy_core": {
      "defines": ["CORE_BUILD=1", "EXPERIMENTAL_FEATURE=1"],
      "files": [
        { "source": "/abs/path/feature.cpp", "defines": ["CORE_BUILD=1", "EXPERIMENTAL_FEATURE=1"] }
      ]
    }
  }
}
```

---

## Phase 2: Clang Preprocessor Plugin (IfdefMapper)

`llvm-pass/IfdefMapper/` — a Clang plugin using `PPCallbacks` to record every
`#ifdef` / `#ifndef` / `#if` conditional block with source file, line range,
macro condition, and branch structure (`#else` / `#elif` arms).

### Build

```bash
bash llvm-pass/IfdefMapper/build.sh
```

### Test (macOS / Linux)

```bash
bash tests/run_phase2_test.sh
```

### Run manually on a single file

```bash
/opt/homebrew/opt/llvm/bin/clang++ \
  -Xclang -load -Xclang llvm-pass/IfdefMapper/build/IfdefMapper.so \
  -Xclang -add-plugin -Xclang ifdef-mapper \
  -Xclang -plugin-arg-ifdef-mapper -Xclang output=ast_mapping.json \
  -std=c++17 -fsyntax-only <source.cpp>
```

> **Note:** Use `-add-plugin` (not `-plugin`). In LLVM 22, `-plugin` requires
> `ReplaceAction` type; our plugin uses `CmdlineBeforeMainAction` and must be
> invoked via `-add-plugin`.

### Output format (`ast_mapping.json`)

```json
{
  "tool": "IfdefMapper",
  "files": {
    "/abs/path/feature.cpp": [
      {
        "kind": "ifdef",
        "condition": "EXPERIMENTAL_FEATURE",
        "start_line": 4,
        "end_line": 8,
        "branches": [
          { "kind": "ifdef", "condition": "EXPERIMENTAL_FEATURE", "start_line": 4, "end_line": 7 }
        ]
      }
    ]
  }
}
```

---

## Phase 3: LLVM Whole-Program Reachability Pass (DeadFeaturePass)

`llvm-pass/DeadFeaturePass/` — new-pass-manager LLVM Module pass. Takes whole-program LLVM IR, performs call-graph BFS from `main` + all exported symbols, and maps reachable basic blocks to source line numbers via debug info.

### Build

```bash
bash llvm-pass/DeadFeaturePass/build.sh
```

### Test (macOS / Linux)

```bash
bash tests/run_phase3_test.sh
```

### Run manually

```bash
# 1. Compile sources to LLVM bitcode with debug info
clang++ -g -std=c++17 -DEXPERIMENTAL_FEATURE=1 -emit-llvm -c feature.cpp -o feature.bc
clang++ -g -std=c++17 -emit-llvm -c main.cpp -o main.bc

# 2. Link to whole-program IR
llvm-link feature.bc main.bc -o whole_program.bc

# 3. Run the pass
opt --load-pass-plugin=./DeadFeaturePass.so \
    --passes="dead-feature-pass" \
    --dead-feature-output=reachability.json \
    whole_program.bc --disable-output
```

### Output format (`reachability.json`)

```json
{
  "tool": "DeadFeaturePass",
  "entry_points": ["main", "_Z12feature_namev"],
  "reachable_functions": ["main", "_Z12feature_namev", "_Z13feature_valueb"],
  "unreachable_functions": [],
  "reachable_lines": {
    "/abs/path/feature.cpp": [5, 6, 13, 15, 19]
  },
  "function_details": { ... }
}
```

### Conservative indirect-call handling

If any reachable function makes an indirect call (function pointer), all
address-taken functions in the module are added to the reachable set and BFS
continues from them.

---

## Phase 4: Correlation Engine (`correlator.py`)

`correlator.py` at the repo root joins all three data sources.

```bash
python3 correlator.py \
  --config-map   tests/artifacts/build_config_map.json \
  --ast-mapping  tests/artifacts/ast_mapping.json \
  --reachability tests/artifacts/reachability.json \
  --report       report.md \
  --json-out     tests/artifacts/dead_features.json \
  --project-root .
```

Or: `bash tests/run_phase4_test.sh`

Confidence: `HIGH` = branch statically never compiled; `MEDIUM` = compiled in
some config but has zero IR coverage (runtime-dead candidate).

---

## Phase 5: Evaluation *(planned)*

Run the full pipeline on `zlib` or `sqlite` and produce the final deliverable
report.

---

## Prerequisites

| Tool | Required for | Install |
|------|-------------|---------|
| Python 3.8+ | All phases | System |
| CMake 3.16+ | Phase 1 | `brew install cmake` |
| Ninja | Phase 1 (optional, preferred) | `brew install ninja` |
| LLVM 17+ (Homebrew) | Phases 2–3 | `brew install llvm` |
| clang / clang++ | Phases 2–3 | Included with LLVM |

> **Apple Clang** (from Xcode Command Line Tools) does **not** ship LLVM
> development headers. Phases 2 and 3 require the full Homebrew LLVM package.

---

## Repository Layout

```
CDEL/
├── extractor/
│   └── extractor.py          # Phase 1: CMake define extractor
├── llvm-pass/
│   ├── README.md             # Phase 2 & 3 notes
│   ├── IfdefMapper/          # Phase 2: Clang preprocessor plugin (planned)
│   └── DeadFeaturePass/      # Phase 3: LLVM LTO pass (planned)
├── tests/
│   ├── dummy_project/        # Minimal CMake project with #ifdef guards
│   │   ├── CMakeLists.txt
│   │   └── src/
│   │       ├── main.cpp
│   │       ├── feature.cpp
│   │       └── feature.h
│   ├── run_phase1_test.sh    # macOS/Linux end-to-end Phase 1 test
│   └── run_phase1_test.ps1   # Windows end-to-end Phase 1 test
└── CLAUDE.md
```
