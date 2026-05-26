# Test Cases — Dead Feature Detector

30 isolated CMake test projects covering every detection pattern.

Run any test case:
```bash
bash run.sh testcases/<tc_dir>
```

Run all and compare against baseline:
```bash
for tc in testcases/tc*/; do
    echo "=== $tc ==="
    NO_UI=1 bash run.sh "$tc" /tmp/dfd-tc-out
    python3 baseline/baseline_compare.py \
        --project "$tc" \
        --tool-out /tmp/dfd-tc-out/dead_features.json \
        --config-map /tmp/dfd-tc-out/build_config_map.json
done
```

See [EVALUATION.md](../EVALUATION.md) for detailed expected output and analysis.

---

## Category A — Macro completely absent from CMake

| ID | Directory | Dead macro | Expected |
|----|-----------|-----------|----------|
| TC01 | `tc01_never_defined` | `PROTOTYPE_API` | 1 HIGH |
| TC06 | `tc06_platform_guard` | `WINDOWS_PLATFORM` | 1 HIGH |
| TC07 | `tc07_cuda_backend` | `ENABLE_CUDA` | 1 HIGH |
| TC08 | `tc08_deprecated_api` | `DEPRECATED_V1_API` | 1 HIGH |
| TC09 | `tc09_legacy_serializer` | `LEGACY_BINARY_FORMAT` | 1 HIGH |
| TC10 | `tc10_mobile_build` | `MOBILE_BUILD` | 1 HIGH |

## Category B — CMake option present but never wired to a target

| ID | Directory | Dead macro | Expected |
|----|-----------|-----------|----------|
| TC03 | `tc03_always_off_flag` | `ENABLE_LEGACY_ALGO` | 1 HIGH |
| TC11 | `tc11_telemetry_off` | `ENABLE_TELEMETRY` | 1 HIGH |
| TC12 | `tc12_hotpatch_off` | `ENABLE_HOT_PATCH` | 1 HIGH |
| TC13 | `tc13_orm_disabled` | `ENABLE_ORM` | 1 HIGH |
| TC14 | `tc14_ipv6_disabled` | `ENABLE_IPV6` | 1 HIGH |
| TC15 | `tc15_avx512_disabled` | `ENABLE_AVX512` | 1 HIGH |

## Category C — Always-ON macro → `#else` branch dead

| ID | Directory | Live macro (always ON) | Dead branch | Expected |
|----|-----------|----------------------|-------------|----------|
| TC02 | `tc02_else_branch_dead` | `NEW_PARSER` | `#else` old parser | 1 HIGH |
| TC16 | `tc16_debug_branch_dead` | `RELEASE_BUILD` | `#else` debug abort | 1 HIGH |
| TC17 | `tc17_insecure_path_dead` | `SECURITY_HARDENED` | `#else` strcpy path | 1 HIGH |
| TC18 | `tc18_old_protocol_dead` | `PROTOCOL_V2` | `#else` V1 framing | 1 HIGH |
| TC19 | `tc19_bounds_check_dead` | `BOUNDS_CHECKING` | `#else` unchecked | 1 HIGH |
| TC20 | `tc20_unicode_ascii_dead` | `UNICODE_SUPPORT` | `#else` ASCII-only | 1 HIGH |

## Category D — Nested dead blocks

| ID | Directory | Pattern | Expected |
|----|-----------|---------|----------|
| TC04 | `tc04_nested_dead` | `HARDWARE_ACCEL` → `GPU_BACKEND`, `FPGA_BACKEND` | 3 HIGH |
| TC21 | `tc21_triple_nested` | `EXPERIMENTAL` → GPU → TENSOR (3-level) | 3 HIGH |
| TC22 | `tc22_offline_nested` | `OFFLINE_MODE` → CACHE → COMPRESS (3-level) | 3 HIGH |

## Category E — `#elif` chains

| ID | Directory | Pattern | Expected |
|----|-----------|---------|----------|
| TC23 | `tc23_elif_chain` | DEBUG (live) / TRACE (dead) / VERBOSE (dead) | 2 HIGH |

## Category F — `#ifndef` pattern

| ID | Directory | Pattern | Expected |
|----|-----------|---------|----------|
| TC27 | `tc27_ifndef_pattern` | `DISABLE_COMPAT` always ON → `#ifndef` block dead | 1 HIGH |

## Category G — Negative / control tests (0 expected findings)

| ID | Directory | Pattern | Expected |
|----|-----------|---------|----------|
| TC05 | `tc05_all_live` | 3 macros all defined | **0** |
| TC24 | `tc24_large_positive` | 8 macros all defined | **0** |

## Category H — Mixed and advanced patterns

| ID | Directory | Pattern | Expected |
|----|-----------|---------|----------|
| TC25 | `tc25_mixed_live_dead` | 2 live + 2 dead blocks in same file | 2 HIGH |
| TC26 | `tc26_multi_target_partial` | `SERVER_PUSH` in server target only; shared file | 1 HIGH |
| TC28 | `tc28_vendor_extension` | `VENDOR_ACME_SDK` replaced by OSS lib | 1 HIGH |
| TC29 | `tc29_regression_guard` | `REGRESSION_TESTS` + `INTERNAL_TESTING` in prod | 2 HIGH |
| TC30 | `tc30_complex_interactions` | TLS+METRICS live, RDMA+ZERO_COPY+pool-else dead | 3 HIGH |
