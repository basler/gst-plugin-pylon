#!/usr/bin/env bash
# Functional test suite for pylonsrc using Basler camera emulators.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_PREFIX="${BUILD_PREFIX:-$PROJECT_ROOT/install}"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
PYLON_ROOT="${PYLON_ROOT:-/opt/pylon}"
PYLON_CAMEMU="${PYLON_CAMEMU:-3}"

EMU_SERIAL_0="0815-0000"
EMU_SERIAL_1="0815-0001"

pass=0
fail=0
skip=0

# Runtime search paths for the pylon SDK and libgstpylon.
# Under fakeroot (dpkg-buildpackage) inherited LD_LIBRARY_PATH is only fakeroot
# paths, so DT_RUNPATH alone is not enough for the plugin scanner child.
plugin_file=""
for dir in \
    "$BUILD_PREFIX/lib64/gstreamer-1.0" \
    "$BUILD_PREFIX/lib/gstreamer-1.0" \
    "$BUILD_DIR/ext/pylon"
do
  for name in libgstpylon.so libgstpylon.dylib libgstpylon.dll gstpylon.dll; do
    if [[ -f "$dir/$name" ]]; then
      plugin_file="$dir/$name"
      GST_PLUGIN_PATH="$dir"
      break 2
    fi
  done
done
if [[ -z "$plugin_file" ]]; then
  echo "No pylonsrc plugin found. Build with: ninja -C build"
  exit 1
fi

EXTRA_LIBS=()
if [[ "$GST_PLUGIN_PATH" == *ext/pylon || "$GST_PLUGIN_PATH" == *ext\\pylon ]]; then
  EXTRA_LIBS+=("$BUILD_DIR/gst-libs/gst/pylon")
else
  EXTRA_LIBS+=("$(dirname "$GST_PLUGIN_PATH")")
fi
if [[ -d "$PYLON_ROOT/lib" ]]; then
  EXTRA_LIBS+=("$PYLON_ROOT/lib")
fi
if [[ -d "$PYLON_ROOT/Runtime/x64" ]]; then
  PATH="$PYLON_ROOT/Runtime/x64:$PATH"
fi
DYLD_FRAMEWORK_PATH="${DYLD_FRAMEWORK_PATH:-}"
for fw in "$PYLON_ROOT/Library/Frameworks" "$PYLON_ROOT/Frameworks"; do
  if [[ -d "$fw" ]]; then
    DYLD_FRAMEWORK_PATH="$fw${DYLD_FRAMEWORK_PATH:+:$DYLD_FRAMEWORK_PATH}"
  fi
done

lib_join=""
for p in "${EXTRA_LIBS[@]}"; do
  lib_join="$p${lib_join:+:$lib_join}"
done
LD_LIBRARY_PATH="${lib_join}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
DYLD_LIBRARY_PATH="${lib_join}${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"

export PYLON_ROOT PYLON_CAMEMU GST_PLUGIN_PATH PATH
export LD_LIBRARY_PATH DYLD_LIBRARY_PATH DYLD_FRAMEWORK_PATH
export GST_DEBUG_NO_COLOR=1

run_pass() {
  echo "PASS: $1"
  pass=$((pass + 1))
}

run_fail() {
  echo "FAIL: $1"
  if [[ -n "${2:-}" ]]; then
    echo "       $2"
  fi
  fail=$((fail + 1))
}

run_skip() {
  echo "SKIP: $1"
  skip=$((skip + 1))
}

expect_ok() {
  local name="$1"
  shift
  local output
  if output="$("$@" 2>&1)"; then
    run_pass "$name"
  else
    run_fail "$name" "command exited non-zero: $*"
    echo "$output" | tail -n 80
  fi
}

expect_fail() {
  local name="$1"
  shift
  if "$@" >/dev/null 2>&1; then
    run_fail "$name" "expected failure but command succeeded: $*"
  else
    run_pass "$name"
  fi
}

expect_output() {
  local name="$1"
  local pattern="$2"
  shift 2
  local output
  if ! output="$("$@" 2>&1)"; then
    run_fail "$name" "command exited non-zero: $*"
    echo "$output" | sed -n '1,20p'
    return
  fi
  if grep -Eq "$pattern" <<<"$output"; then
    run_pass "$name"
  else
    run_fail "$name" "output did not match /$pattern/"
    echo "$output" | sed -n '1,20p'
  fi
}

expect_fail_output() {
  local name="$1"
  local pattern="$2"
  shift 2
  local output
  output="$("$@" 2>&1)" || true
  if grep -Eq "$pattern" <<<"$output"; then
    run_pass "$name"
  else
    run_fail "$name" "stderr did not match /$pattern/"
    echo "$output" | sed -n '1,20p'
  fi
}

# GNU timeout is Linux; macOS may have gtimeout from coreutils; do not invoke
# Windows timeout.exe (different CLI, can block).
TIMEOUT_CMD=""
case "$(uname -s)" in
  Linux*)
    if command -v timeout >/dev/null 2>&1; then
      TIMEOUT_CMD=timeout
    fi
    ;;
  Darwin*)
    if command -v gtimeout >/dev/null 2>&1; then
      TIMEOUT_CMD=gtimeout
    fi
    ;;
esac

gst_pipeline() {
  if [[ -n "$TIMEOUT_CMD" ]]; then
    "$TIMEOUT_CMD" 90 gst-launch-1.0 -q "$@" 2>&1
  else
    gst-launch-1.0 -q "$@" 2>&1
  fi
}

echo "camemu functional tests (PYLON_CAMEMU=$PYLON_CAMEMU, PYLON_ROOT=$PYLON_ROOT)"
echo "GST_PLUGIN_PATH=$GST_PLUGIN_PATH"
echo

expect_output "plugin_loads" "Basler/Pylon source element" \
  gst-inspect-1.0 pylonsrc

expect_output "inspect_has_device_serial_property" "device-serial-number" \
  gst-inspect-1.0 pylonsrc

expect_output "inspect_has_child_properties" "cam|stream" \
  gst-inspect-1.0 pylonsrc

expect_ok "capture_by_emulator_serial_0" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" num-buffers=10 ! fakesink

expect_ok "capture_by_emulator_serial_1" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_1" num-buffers=5 ! fakesink

expect_ok "capture_by_device_index_1" \
  gst_pipeline pylonsrc device-index=1 num-buffers=5 ! fakesink

expect_fail "ambiguous_devices_without_selection" \
  gst_pipeline pylonsrc num-buffers=1 ! fakesink

expect_fail_output "ambiguous_devices_lists_emulators" "$EMU_SERIAL_0" \
  gst_pipeline pylonsrc num-buffers=1 ! fakesink

if [[ -n "$TIMEOUT_CMD" ]]; then
  expect_fail_output "wrong_serial_fails_quickly" "No devices found matching" \
    "$TIMEOUT_CMD" 5 gst-launch-1.0 -q pylonsrc device-serial-number=NOSUCHSERIAL999 \
      num-buffers=1 ! fakesink
else
  expect_fail_output "wrong_serial_fails_quickly" "No devices found matching" \
    gst-launch-1.0 -q pylonsrc device-serial-number=NOSUCHSERIAL999 \
      num-buffers=1 ! fakesink
fi

expect_ok "capture_gray8_fixed_caps" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" num-buffers=8 \
    ! "video/x-raw,format=GRAY8,width=640,height=480" ! fakesink

expect_ok "capture_bayer_caps" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" num-buffers=5 \
    ! "video/x-bayer,format=rggb,width=640,height=480" ! fakesink

expect_ok "capture_with_framerate_cap" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" num-buffers=5 \
    ! "video/x-raw,format=GRAY8,width=640,height=480,framerate=30/1" ! fakesink

expect_ok "capture_with_user_set_auto" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" user-set=Auto \
    num-buffers=5 ! fakesink

expect_ok "capture_with_enable_correction" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" enable-correction=true \
    num-buffers=5 ! fakesink

# cam:: before user-set must still apply the final userset (config reload)
expect_ok "cam_property_before_user_set" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" \
    cam::Gain=1 user-set=Auto num-buffers=5 ! fakesink

expect_ok "pipeline_with_queue" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" num-buffers=10 \
    ! queue max-size-buffers=2 ! fakesink

expect_ok "pipeline_with_videoconvert" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" num-buffers=5 \
    ! videoconvert ! video/x-raw,format=RGB ! fakesink

if command -v python3 >/dev/null 2>&1 || [[ -x /usr/bin/python3 ]]; then
  # Prefer a Python that has PyGObject (system packages). A Meson/CI venv
  # often shadows python3 without gi bindings.
  PYTHON_GI=""
  for py in /usr/bin/python3 python3; do
    if command -v "$py" >/dev/null 2>&1 || [[ -x "$py" ]]; then
      if "$py" -c "import gi; gi.require_version('Gst','1.0'); from gi.repository import Gst" 2>/dev/null; then
        PYTHON_GI="$py"
        break
      fi
    fi
  done
  if [[ -n "$PYTHON_GI" ]]; then
    expect_ok "appsink_buffer_count" \
      "$PYTHON_GI" "$SCRIPT_DIR/appsink_buffer_count.py" \
        --serial "$EMU_SERIAL_0" --buffers 12

    # Pipe FD / VmRSS sampling uses /proc (Linux). Camemu itself is
    # cross-platform; skip only this check elsewhere.
    if [[ -d /proc/self/fd ]]; then
      # Restart cycles with 4096x4096 RGB (~50 MiB/frame): pipe FD growth and
      # RSS must stay bounded; abrupt stop leaves a pending grab in the handler.
      # RSS threshold is in frames (~50 MiB at 4096² RGB). Meson sets
      # MALLOC_PERTURB_ which inflates RSS; CI also runs other tests in
      # parallel unless camemu is marked is_parallel=false. Allow a few
      # cached pool pages (real grab-result leaks are ~1 frame/cycle).
      expect_ok "restart_resource_cleanup" \
        "$PYTHON_GI" "$SCRIPT_DIR/restart_resource_leak.py" \
          --serial "$EMU_SERIAL_0" --cycles 10 --max-pipe-growth 8 \
          --max-rss-frames 3 --settle-ms 400
    else
      run_skip "restart_resource_cleanup (/proc FD and VmRSS sampling is Linux-only)"
    fi
  else
    run_skip "appsink_buffer_count (PyGObject not available)"
    run_skip "restart_resource_cleanup (PyGObject not available)"
  fi
else
  run_skip "appsink_buffer_count (python3 not available)"
  run_skip "restart_resource_cleanup (python3 not available)"
fi

expect_ok "sequential_pipeline_runs" \
  bash -c '
    for i in 1 2 3; do
      gst-launch-1.0 -q pylonsrc device-serial-number='"$EMU_SERIAL_0"' num-buffers=3 ! fakesink || exit 1
    done
  '

echo
echo "Results: $pass passed, $fail failed, $skip skipped"
if [[ "$fail" -gt 0 ]]; then
  exit 1
fi
