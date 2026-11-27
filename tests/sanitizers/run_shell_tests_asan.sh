#!/usr/bin/env bash
# Runs the script-driven tests (meson suite "shell") of an ASan/UBSan build.
#
# Those tests drive the system gst-launch-1.0 / gst-inspect-1.0, which are not
# instrumented, while our plugin inside them is. Two things are needed:
#
#  * the sanitizer runtime must be preloaded into the processes that load the
#    plugin: gst-launch-1.0, gst-inspect-1.0 and python3 (PyGObject tests).
#    Nothing else gets it: preloading into coreutils trips unrelated reports,
#  * leak detection must be off for them. Leaks in GStreamer's own registry
#    scan would drown out ours. Leaks in our code are covered by the
#    in-process tests (`meson test --no-suite shell`), which run with
#    detect_leaks=1.
#
# Memory errors and undefined behaviour in the plugin still abort the test.
#
# usage: run_shell_tests_asan.sh <meson-build-dir> [extra meson test args...]
# env:   CC (default: clang) selects which sanitizer runtime to preload.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <meson-build-dir> [meson test args...]" >&2
  exit 2
fi
build_dir="$1"
shift

cc="${CC:-clang}"
arch="$(uname -m)"
if "$cc" --version 2>/dev/null | grep -qi clang; then
  asan_rt="$("$cc" -print-file-name="libclang_rt.asan-${arch}.so")"
  ubsan_rt="$("$cc" -print-file-name="libclang_rt.ubsan_standalone-${arch}.so")"
else
  asan_rt="$("$cc" -print-file-name=libasan.so)"
  ubsan_rt="$("$cc" -print-file-name=libubsan.so)"
fi
for rt in "$asan_rt" "$ubsan_rt"; do
  if [[ ! -f "$rt" ]]; then
    echo "error: sanitizer runtime not found: $rt (CC=$cc)" >&2
    exit 1
  fi
done

wrap_dir="$(mktemp -d)"
trap 'rm -rf "$wrap_dir"' EXIT

for tool in gst-launch-1.0 gst-inspect-1.0 python3; do
  real="$(command -v "$tool")"
  # A venv python3 usually lacks PyGObject; use the system one.
  if [[ "$tool" == python3 && -x /usr/bin/python3 ]]; then
    real=/usr/bin/python3
  fi
  cat > "$wrap_dir/$tool" <<EOF
#!/bin/sh
exec env LD_PRELOAD="$asan_rt $ubsan_rt" \\
  ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:abort_on_error=1" \\
  "$real" "\$@"
EOF
  chmod +x "$wrap_dir/$tool"
done

export PATH="$wrap_dir:$PATH"
# run_camemu_tests.sh looks for /usr/bin/python3 before PATH.
export PYLONSRC_TEST_PYTHON="$wrap_dir/python3"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:abort_on_error=1:print_stacktrace=1}"

exec meson test -C "$build_dir" --suite shell --print-errorlogs "$@"
