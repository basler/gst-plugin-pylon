# Compatibility Test Suite

Ensures that changes (e.g. speedup refactors) do not break existing installations.
**Compatibility breaks are forbidden** — users have cameras in the field.

## Strategy

1. **Golden outputs**: Generate reference outputs from a known-good baseline (main).
2. **Compare**: On a branch, run the same tests and diff against golden.
3. **CI**: Run on both main and PR branches; merge only if compat passes.

## Test Surfaces

| Surface | What we verify |
|---------|----------------|
| gst-inspect | Element structure, property names, cam/stream API contract |
| Pipeline | `pylonsrc device-index=0 num-buffers=5 ! fakesink` completes successfully |

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
