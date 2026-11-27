# Compatibility Test Suite

Ensures that changes (e.g. speedup refactors) do not break existing installations.
**Compatibility breaks are forbidden** — users have cameras in the field.

## Strategy

1. **Golden outputs**: Generate reference outputs from a known-good baseline (main).
2. **Compare**: On a branch, run the same tests and diff against golden.
3. **CI**: Run on both main and PR branches; merge only if compat passes.

## Test Surfaces

| Surface | Script / Artifact | What we verify |
|---------|-------------------|----------------|
| Golden `gst-inspect` | `tests/compat/run_compat_tests.sh` | Element structure, property names, cam/stream API contract |
| A/B differential (`v1.1.0` vs `v2.0.0`) | `tests/compat/run_ab_compat.py` | Zero-recompile `v1.1.0` C/Python drop-in, `libgstpylon-1.0` ABI/symbols/struct layout, GObject/ChildProxy/caps/metadata/error A/B diff, `~/.cache/gstpylon` migration, `GstDeviceMonitor` non-interference |
| Intentional delta ledger | `tests/compat/intentional_deltas_v2.json` | Machine-checked allowlist of additive APIs and documented bugfixes; any unlisted diff fails |
| A/B performance benchmark | `tests/compat/bench_ab_v1_vs_v2.py` | Cold/warm startup latency, `gst-inspect` latency, sustained throughput, restart FD/RSS |
| Hardware & NVMM sign-off | `tests/compat/HARDWARE_SIGNOFF.md` | Physical USB3/GigE, line-scan, and Jetson DeepStream NVMM checklist |

### Run A/B differential proof against `v1.1.0`

```bash
PYLON_ROOT=/opt/pylon python3 tests/compat/run_ab_compat.py --v2-build build
```

### Run A/B performance benchmark (`v1.1.0` vs `v2.0.0`)

```bash
PYLON_ROOT=/opt/pylon python3 tests/compat/bench_ab_v1_vs_v2.py --v2-build build
```

## Usage

### Generate golden files (on main branch, before merging changes)

```bash
git checkout main
PYLON_ROOT=/opt/pylon/ uvx meson setup build --prefix=$PWD/install
ninja -C build install
./tests/compat/run_compat_tests.sh --generate
# Commit tests/compat/golden/ if the baseline intentionally changed
```

### Run compatibility check (on feature branch)

```bash
PYLON_ROOT=/opt/pylon/ ninja -C build install
./tests/compat/run_compat_tests.sh
```

### Via meson test

```bash
PYLON_ROOT=/opt/pylon/ ninja -C build install
PYLON_ROOT=/opt/pylon/ ninja -C build test
```

Uses `PYLON_CAMEMU=2`. Generate golden in an environment that matches CI (emulated cameras only). If your system has real cameras, the golden may differ from CI; regenerate golden in CI or a container without physical cameras for a consistent baseline.

## Comparison workflow (main vs branch)

To guarantee no compatibility breaks:

1. On **main**: run `--generate`, commit `tests/compat/golden/`.
2. On **branch**: run the script (no `--generate`). It diffs against golden.
3. If diff is non-empty: either fix the branch to match main, or (if the change is intentional) regenerate golden on main and document.

## Adding to CI

Add after `ninja -C build install`:

```yaml
- name: Compatibility check
  run: |
    export PYLON_CAMEMU=2
    ./tests/compat/run_compat_tests.sh
```
