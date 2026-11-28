#!/usr/bin/env bash
# Functional test suite for pylonsrc using Basler camera emulators.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYLON_ROOT="${PYLON_ROOT:-/opt/pylon}"
PYLON_CAMEMU="${PYLON_CAMEMU:-3}"
# Isolate CamEmu tests from any physical USB/GigE/CXP/CameraLink cameras that
# happen to be plugged into the developer machine or CI runner.
PYLONSRC_SKIP_TRANSPORT_LAYERS="${PYLONSRC_SKIP_TRANSPORT_LAYERS:-usb,gige,cxp,cameralink}"

EMU_SERIAL_0="0815-0000"
EMU_SERIAL_1="0815-0001"

pass=0
fail=0
skip=0

export PYLON_ROOT PYLON_CAMEMU PYLONSRC_SKIP_TRANSPORT_LAYERS
source "$SCRIPT_DIR/../check_plugin_path.sh"
check_plugin_path
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
echo "Testing $PYLON_TEST_PLUGIN"
echo

expect_output "plugin_loads" "Basler/Pylon source element" \
  gst-inspect-1.0 pylonsrc

expect_output "inspect_has_device_serial_property" "device-serial-number" \
  gst-inspect-1.0 pylonsrc

expect_output "inspect_has_child_properties" "cam|stream" \
  gst-inspect-1.0 pylonsrc

expect_output "inspect_has_max_pending_frames_property" "max-pending-frames" \
  gst-inspect-1.0 pylonsrc

# Pin system memory. NVMM is first in the pad template, so `pylonsrc ! fakesink`
# would otherwise pick memory:NVMM and call NvBufSurfaceAllocate (fails under
# fakeroot / dpkg-buildpackage, and is too heavy for 8 GB Jetson test runs).
SYS_CAPS="video/x-raw,format=GRAY8,width=640,height=480"

expect_ok "capture_by_emulator_serial_0" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" num-buffers=10 \
    ! "$SYS_CAPS" ! fakesink

expect_ok "capture_by_emulator_serial_1" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_1" num-buffers=5 \
    ! "$SYS_CAPS" ! fakesink

expect_ok "capture_by_device_index_1" \
  gst_pipeline pylonsrc device-index=1 num-buffers=5 \
    ! "$SYS_CAPS" ! fakesink

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

expect_ok "capture_gray16_le_caps" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" num-buffers=5 \
    ! "video/x-raw,format=GRAY16_LE,width=640,height=480" ! fakesink

expect_ok "capture_bayer_caps" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" num-buffers=5 \
    ! "video/x-bayer,format=rggb,width=640,height=480" ! fakesink

expect_ok "capture_with_framerate_cap" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" num-buffers=5 \
    ! "video/x-raw,format=GRAY8,width=640,height=480,framerate=30/1" ! fakesink

expect_ok "capture_with_user_set_auto" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" user-set=Auto \
    num-buffers=5 ! "$SYS_CAPS" ! fakesink

expect_ok "capture_with_enable_correction" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" enable-correction=true \
    num-buffers=5 ! "$SYS_CAPS" ! fakesink

# cam:: before user-set must still apply the final userset (config reload)
expect_ok "cam_property_before_user_set" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" \
    cam::Gain=1 user-set=Auto num-buffers=5 ! "$SYS_CAPS" ! fakesink

expect_ok "pipeline_with_queue" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" num-buffers=10 \
    ! "$SYS_CAPS" ! queue max-size-buffers=2 ! fakesink

expect_ok "pipeline_with_max_pending_frames" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" \
    max-pending-frames=3 num-buffers=10 ! "$SYS_CAPS" ! fakesink

expect_ok "pipeline_with_videoconvert" \
  gst_pipeline pylonsrc device-serial-number="$EMU_SERIAL_0" num-buffers=5 \
    ! "$SYS_CAPS" ! videoconvert ! video/x-raw,format=RGB ! fakesink

if command -v python3 >/dev/null 2>&1 || [[ -x /usr/bin/python3 ]]; then
  # Prefer a Python that has PyGObject (system packages). A Meson/CI venv
  # often shadows python3 without gi bindings.
  # PYLONSRC_TEST_PYTHON lets the sanitizer job substitute a wrapper that
  # preloads the ASan runtime (see tests/sanitizers/run_shell_tests_asan.sh).
  PYTHON_GI=""
  for py in ${PYLONSRC_TEST_PYTHON:+"$PYLONSRC_TEST_PYTHON"} /usr/bin/python3 python3; do
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

    expect_ok "pending_frames_survive_stall" \
      "$PYTHON_GI" "$SCRIPT_DIR/pending_frames_continuity.py" \
        --serial "$EMU_SERIAL_0" --max-pending-frames 6 --stall-frames 4

    # Caps/colorimetry contracts for NVMM full-range follow-ups. Optional
    # Jetson pipeline: RUN_NVMM_FULL_RANGE=1.
    expect_ok "nvmm_full_range_contract" \
      "$PYTHON_GI" "$SCRIPT_DIR/nvmm_full_range_contract.py" \
        --serial "$EMU_SERIAL_0"

    # Pipe FD / VmRSS sampling uses /proc (Linux). Camemu itself is
    # cross-platform; skip only this check elsewhere.
    if [[ -d /proc/self/fd ]]; then
      # Restart cycles with 4096x4096 RGB (~50 MiB/frame): pipe FD growth and
      # RSS must stay bounded; abrupt stop leaves a pending grab in the handler.
      # The RSS budget is per cycle because a real grab-result leak is ~1
      # frame/cycle, while retained heap pages grow far slower. Meson sets
      # MALLOC_PERTURB_ which inflates RSS; CI also runs other tests in
      # parallel unless camemu is marked is_parallel=false.
      expect_ok "restart_resource_cleanup" \
        "$PYTHON_GI" "$SCRIPT_DIR/restart_resource_leak.py" \
          --serial "$EMU_SERIAL_0" --cycles 10 --max-pipe-growth 8 \
          --max-rss-frames-per-cycle 0.5 --settle-ms 400
    else
      run_skip "restart_resource_cleanup (/proc FD and VmRSS sampling is Linux-only)"
    fi
  else
    run_skip "appsink_buffer_count (PyGObject not available)"
    run_skip "pending_frames_survive_stall (PyGObject not available)"
    run_skip "nvmm_full_range_contract (PyGObject not available)"
    run_skip "restart_resource_cleanup (PyGObject not available)"
  fi
else
  run_skip "appsink_buffer_count (python3 not available)"
  run_skip "pending_frames_survive_stall (python3 not available)"
  run_skip "nvmm_full_range_contract (python3 not available)"
  run_skip "restart_resource_cleanup (python3 not available)"
fi

expect_ok "sequential_pipeline_runs" \
  bash -c '
    for i in 1 2 3; do
      gst-launch-1.0 -q pylonsrc device-serial-number='"$EMU_SERIAL_0"' num-buffers=3 ! '"$SYS_CAPS"' ! fakesink || exit 1
    done
  '

expect_ok "multiprocess_concurrent_distinct_cameras" \
  bash -c '
    rm -rf "${TMPDIR:-/tmp}/gstpylon-mp-cache-$$"
    export XDG_CACHE_HOME="${TMPDIR:-/tmp}/gstpylon-mp-cache-$$"
    trap "rm -rf \"$XDG_CACHE_HOME\"" EXIT
    gst-launch-1.0 -q pylonsrc device-serial-number='"$EMU_SERIAL_0"' num-buffers=10 ! '"$SYS_CAPS"' ! fakesink &
    p0=$!
    gst-launch-1.0 -q pylonsrc device-serial-number='"$EMU_SERIAL_1"' num-buffers=10 ! '"$SYS_CAPS"' ! fakesink &
    p1=$!
    gst-launch-1.0 -q pylonsrc device-index=2 num-buffers=10 ! '"$SYS_CAPS"' ! fakesink &
    p2=$!
    wait "$p0" && wait "$p1" && wait "$p2"
  '

echo
echo "Results: $pass passed, $fail failed, $skip skipped"
if [[ "$fail" -gt 0 ]]; then
  exit 1
fi
