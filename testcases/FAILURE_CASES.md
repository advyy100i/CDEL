# Failure Case Demonstration Guide

Four cases where the tool gives wrong or unhelpful results.
Run these to demonstrate the limitations honestly.

---

## Prerequisites

```bash
# Phase 1 only (needed for all cases)
cd "$(git rev-parse --show-toplevel)"  # run from repo root

# Full pipeline (needed for TC_FN01)
# Requires Homebrew LLVM: brew install llvm
# Build plugins first:
bash llvm-pass/IfdefMapper/build.sh
bash llvm-pass/DeadFeaturePass/build.sh
```

---

## TC_FP01 — False Positive (configure_file macro)

**What to observe:** Tool reports `HAVE_NETWORK_STACK` as HIGH dead — but the code is live.

```bash
# Step 1: Run Phase 1 on the test case
python3 extractor/extractor.py \
  --project  testcases/tc_fp01_configure_file \
  --output   /tmp/tc_fp01_config_map.json \
  --verbose

# Step 2: Inspect — HAVE_NETWORK_STACK is NOT in defines (Phase 1 missed it)
python3 -c "
import json
d = json.load(open('/tmp/tc_fp01_config_map.json'))
for t, info in d['targets'].items():
    print(f'{t}: defines = {info[\"defines\"]}')
"

# Step 3: Compare against baseline
python3 baseline/baseline_compare.py \
  --project    testcases/tc_fp01_configure_file \
  --tool-out   /tmp/tc_fp01_dead_features.json \
  --config-map /tmp/tc_fp01_config_map.json \
  --verbose
```

**Expected output from Step 2:**
```
tc_fp01_app: defines = []      ← HAVE_NETWORK_STACK missing!
```

**Why this is wrong:** `config.h.in` defines `HAVE_NETWORK_STACK 1`, but
`configure_file()` never puts it on the command line. Phase 1 only reads
`-D` flags from `compile_commands.json`.

---

## TC_FN02 — False Negative (value-dependent #if)

**What to observe:** Tool reports 0 findings — but 2 blocks are dead (`LOG_LEVEL=0`).

```bash
# Step 1: Run Phase 1
python3 extractor/extractor.py \
  --project  testcases/tc_fn02_value_condition \
  --output   /tmp/tc_fn02_config_map.json \
  --verbose

# Step 2: Inspect — LOG_LEVEL IS in defines (with value stripped to just name)
python3 -c "
import json
d = json.load(open('/tmp/tc_fn02_config_map.json'))
for t, info in d['targets'].items():
    print(f'{t}: defines = {info[\"defines\"]}')
"

# Step 3: Show what a correct tool would flag
echo ""
echo "Correct answer: both blocks below are DEAD because LOG_LEVEL=0"
grep -n '#if LOG_LEVEL' testcases/tc_fn02_value_condition/src/main.cpp
```

**Expected output from Step 2:**
```
tc_fn02_app: defines = ['LOG_LEVEL']   ← name present, value 0 lost
```

**Why this is wrong:** `-DLOG_LEVEL=0` gets stripped to just `LOG_LEVEL`.
Phase 4 sees the name is "defined" and skips the block. The value `0` that
makes `LOG_LEVEL > 0` false is never evaluated.

---

## TC_FN01 — False Negative (BFS over-approximation)

**What to observe:** `legacy_handler()` is compiled but never called — should be
MEDIUM confidence — but BFS marks it reachable because its address is taken.

```bash
# Requires full pipeline (LLVM must be built)

# Step 1: Compile to bitcode
LLVM=/opt/homebrew/opt/llvm/bin
TC=testcases/tc_fn01_indirect_bfs/src

$LLVM/clang++ -g -emit-llvm -c \
  -DENABLE_LEGACY_PATH=1 \
  $TC/main.cpp -o /tmp/tc_fn01_main.bc

# Step 2: Run DeadFeaturePass — inspect reachable_functions
$LLVM/opt \
  --load-pass-plugin=llvm-pass/DeadFeaturePass/build/DeadFeaturePass.so \
  --passes="dead-feature-pass" \
  --dead-feature-output=/tmp/tc_fn01_reach.json \
  /tmp/tc_fn01_main.bc --disable-output

# Step 3: Show that legacy_handler is incorrectly marked reachable
python3 -c "
import json
d = json.load(open('/tmp/tc_fn01_reach.json'))
print('Reachable functions:', d.get('reachable_functions', []))
print()
print('legacy_handler reachable?',
      any('legacy' in f for f in d.get('reachable_functions', [])))
print('Expected: False  (it is never called)')
"
```

**Expected output from Step 3:**
```
Reachable functions: ['main', 'modern_handler', 'legacy_handler']
                                                  ^^^^^^^^^^^^^^
legacy_handler reachable? True
Expected: False  (it is never called)
```

**Why this is wrong:** `dispatch[0]()` is an indirect call — no static callee.
BFS conservatively adds ALL address-taken functions (`&legacy_handler` is in
the array) to the reachable set. This is sound (never a false positive on
direct calls) but imprecise for function-pointer dispatch.

---

## TC_BL01 — Baseline Sufficient (tool over-engineered for simple cases)

**What to observe:** Baseline 2 (Phase 1 only) matches the full tool exactly.

```bash
# Step 1: Run Phase 1
python3 extractor/extractor.py \
  --project  testcases/tc_bl01_baseline_sufficient \
  --output   /tmp/tc_bl01_config_map.json \
  --verbose

# Step 2: Run Baseline 2 manually — check which macros are absent
python3 -c "
import json, re
from pathlib import Path

config = json.load(open('/tmp/tc_bl01_config_map.json'))
all_defined = {d.split('=')[0]
               for t in config['targets'].values()
               for d in t.get('defines', [])}

src = Path('testcases/tc_bl01_baseline_sufficient/src/main.cpp').read_text()
macros = set(re.findall(r'#ifdef\s+(\w+)', src))

dead = macros - all_defined
print(f'Macros in source  : {sorted(macros)}')
print(f'Macros in CMake   : {sorted(all_defined)}')
print(f'Dead (Baseline 2) : {sorted(dead)}')
print()
print('Full tool would give identical results.')
print('Phase 2+3 (Clang plugin + LLVM pass) add nothing here.')
"
```

**Expected output:**
```
Macros in source  : ['EXPERIMENTAL_UI', 'FUTURE_FEATURE_A', 'FUTURE_FEATURE_B']
Macros in CMake   : []
Dead (Baseline 2) : ['EXPERIMENTAL_UI', 'FUTURE_FEATURE_A', 'FUTURE_FEATURE_B']

Full tool would give identical results.
Phase 2+3 (Clang plugin + LLVM pass) add nothing here.
```

**Conclusion:** For projects where dead macros are simply absent from all
`target_compile_definitions`, a grep/CMake check is sufficient. The full
pipeline's value is in per-file config filtering and MEDIUM-confidence
IR reachability — neither applies here.

---

## Side-by-side summary

| Case | Tool result | Correct result | Failure type |
|------|-------------|----------------|--------------|
| TC_FP01 | 1 HIGH dead | 0 (block is live) | False positive |
| TC_FN01 | 0 findings | 1 MEDIUM dead | False negative |
| TC_FN02 | 0 findings | 2 HIGH dead | False negative |
| TC_BL01 | 3 HIGH dead ✓ | 3 HIGH dead | Correct but over-engineered |
