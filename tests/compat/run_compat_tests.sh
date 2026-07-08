#!/usr/bin/env bash
# Compatibility test suite for gst-plugin-pylon
# Run with --generate to create golden files (do this on main).
# Run without to compare current build against golden.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
GOLDEN_DIR="$SCRIPT_DIR/golden"
BUILD_PREFIX="${BUILD_PREFIX:-$PROJECT_ROOT/install}"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
PYLON_ROOT="${PYLON_ROOT:-/opt/pylon}"

# Resolve plugin and lib paths
# Prefer install dir; else use build dir (ninja test without install)
if [[ -d "$BUILD_PREFIX/lib64/gstreamer-1.0" ]] || [[ -d "$BUILD_PREFIX/lib/gstreamer-1.0" ]]; then
  if [[ -d "$BUILD_PREFIX/lib64" ]]; then
    LIB_DIR="$BUILD_PREFIX/lib64"
  else
    LIB_DIR="$BUILD_PREFIX/lib"
  fi
  GST_PLUGIN_PATH="$LIB_DIR/gstreamer-1.0"
  LD_LIBRARY_PATH="$LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
elif [[ -f "$BUILD_DIR/ext/pylon/libgstpylon.so" ]]; then
  GST_PLUGIN_PATH="$BUILD_DIR/ext/pylon"
  LD_LIBRARY_PATH="$BUILD_DIR/gst-libs/gst/pylon${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
else
  echo "No plugin found. Build with: ninja -C build && ninja -C build install"
  exit 1
fi

export PYLON_CAMEMU=2
export GST_PLUGIN_PATH
export LD_LIBRARY_PATH

generate=false
if [[ "${1:-}" == "--generate" ]]; then
  generate=true
fi

# Normalize gst-inspect output for stable comparison:
# - strip trailing whitespace
# - collapse multiple blank lines to one
# - normalize Filename (path varies) and Version (git hash varies)
# - normalize child-property Default: lines (live camera state can vary)
normalize_inspect() {
  sed 's/[[:space:]]*$//' \
    | sed 's/ (GstValueList)//g; s/ (GstIntRange)//g; s/ (GstFractionRange)//g' \
    | sed 's|Filename[[:space:]]*.*libgstpylon\.so.*|Filename                 libgstpylon.so|' \
    | sed '/^[[:space:]]*Version[[:space:]]/s/[[:space:]]*Version[[:space:]].*$/  Version                  PLACEHOLDER/' \
    | sed '/^[[:space:]]\{20,\}/s/Default: true/Default: <bool>/g; /^[[:space:]]\{20,\}/s/Default: false/Default: <bool>/g' \
    | sed '/^[[:space:]]\{20,\}/s/Default: [0-9][0-9.]*, "[^"]*"/Default: <enum>/g' \
    | sed '/^[[:space:]]\{20,\}/s/Default: [0-9][0-9.e+-]*/Default: <num>/g' \
    | sed '/^[[:space:]]\{20,\}/s/Default: "(null)"/Default: <str>/g; /^[[:space:]]\{20,\}/s/Default: ""/Default: <str>/g' \
    | cat -s
}

# Keep only the stable pylonsrc element properties. Dynamic cam/stream child
# property trees depend on the emulated camera models visible at inspect time.
extract_static_inspect() {
  awk '
    /^  cam / { exit }
    { print }
  '
}

run_gst_inspect() {
  gst-inspect-1.0 pylonsrc 2>/dev/null
}

run_pipeline() {
  timeout 60 gst-launch-1.0 -q pylonsrc device-serial-number=0815-0000 num-buffers=5 ! fakesink 2>&1
}

mkdir -p "$GOLDEN_DIR"
cd "$PROJECT_ROOT"

# Clear cache so we get deterministic output
export GST_PLUGIN_PATH
rm -rf "${XDG_CACHE_HOME:-$HOME/.cache}/gstpylon"

if $generate; then
  echo "Generating golden files..."
  run_gst_inspect | normalize_inspect | extract_static_inspect > "$GOLDEN_DIR/gst_inspect_pylonsrc.txt"
  run_pipeline
  echo "Golden files written to $GOLDEN_DIR"
  exit 0
fi

# Compare mode
if [[ ! -f "$GOLDEN_DIR/gst_inspect_pylonsrc.txt" ]]; then
  echo "No golden files found. Run with --generate on main first."
  exit 1
fi

echo "Running compatibility tests..."

failed=0

# Test 1: gst-inspect structure
actual_inspect=$(mktemp)
run_gst_inspect | normalize_inspect | extract_static_inspect > "$actual_inspect"
if ! diff -u "$GOLDEN_DIR/gst_inspect_pylonsrc.txt" "$actual_inspect"; then
  echo "FAIL: gst-inspect output differs from golden"
  failed=1
else
  echo "PASS: gst-inspect matches golden"
fi
rm -f "$actual_inspect"

# Test 2: pipeline runs to completion
if run_pipeline; then
  echo "PASS: pipeline completes"
else
  echo "FAIL: pipeline did not complete"
  failed=1
fi

if [[ $failed -eq 1 ]]; then
  echo ""
  echo "Compatibility check FAILED. Do not merge if this breaks existing installations."
  exit 1
fi

echo ""
echo "All compatibility checks passed."
