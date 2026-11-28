#!/usr/bin/env bash
# Inspect-surface regression test suite for gst-plugin-pylon.
# Run with --generate only when intentionally accepting a property-surface
# change. Run without arguments to compare the current build against golden.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
GOLDEN_DIR="$SCRIPT_DIR/golden"
export LC_ALL=C
# Camera emulator only. The golden must not depend on whatever USB3/GigE
# cameras are attached to the host, so skip every real transport layer.
export PYLON_CAMEMU=2
export PYLONSRC_SKIP_TRANSPORT_LAYERS=usb,gige,cxp,cameralink
unset GST_PYLON_SKIP_TL

generate=false
if [[ "${1:-}" == "--generate" ]]; then
  generate=true
fi

# Normalize gst-inspect output for stable comparison:
# - strip trailing whitespace and blank lines (gst-inspect spacing varies)
# - normalize Filename (path varies) and Version (git hash varies)
# - normalize child-property Default: lines (live camera state can vary)
# - normalize the emulator DeviceVersion (it mirrors the installed pylon SDK build)
# - omit the Element Flags section added by newer gst-inspect versions
# - drop GStreamer-version-specific base-class props / flag bits / action syntax
normalize_inspect() {
  sed 's/[[:space:]]*$//' \
    | sed 's/^[[:space:]]*\t/\t/' \
    | sed 's/ (GstValueList)//g; s/ (GstIntRange)//g; s/ (GstFractionRange)//g' \
    | sed '/^Element Flags:$/,/^$/d' \
    | sed -E 's#Filename[[:space:]]*.*(lib)?gstpylon\.(so|dylib|dll).*#Filename                 libgstpylon.so#' \
    | sed '/^[[:space:]]*Version[[:space:]]/s/[[:space:]]*Version[[:space:]].*$/  Version                  PLACEHOLDER/' \
    | sed '/^[[:space:]]\{20,\}/s/Default: true/Default: <bool>/g; /^[[:space:]]\{20,\}/s/Default: false/Default: <bool>/g' \
    | sed '/^[[:space:]]\{20,\}/s/Default: [0-9][0-9.]*, "[^"]*"/Default: <enum>/g' \
    | sed '/^[[:space:]]\{20,\}/s/Default: [0-9][0-9.e+-]*/Default: <num>/g' \
    | sed -E '/^[[:space:]]{20,}/s/Default: "[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+"/Default: "<sdk-version>"/g' \
    | sed '/^[[:space:]]\{20,\}/s/Default: "(null)"/Default: <str>/g; /^[[:space:]]\{20,\}/s/Default: ""/Default: <str>/g' \
    | sed -E 's/(flags: readable, writable), 0x[0-9a-fA-F]+/\1/' \
    | sed '/^[[:space:]]*automatic-eos[[:space:]]*:/,/^$/d' \
    | awk '
        /^[[:space:]]*"execute-command"/ {
          print "  \"execute-command\" :  <action>"
          skip_action = 1
          next
        }
        skip_action {
          if ($0 ~ /^[[:space:]]*$/) { skip_action = 0; next }
          if ($0 ~ /^[[:space:]]/ && $0 !~ /^[[:space:]]*"/) next
          skip_action = 0
        }
        /^$/ { next }
        { print }
      '
}

# Pad-template Bayer formats differ by GStreamer version (10/12/16le need >= 1.24).
extract_bayer_formats() {
  awk '
    /video\/x-bayer/ { in_bayer=1; next }
    in_bayer && /format:/ {
      sub(/^[[:space:]]*format:[[:space:]]*/, "")
      print
      exit
    }
  ' "$1"
}

extract_inspect_surface() {
  cat
}

run_gst_inspect() {
  gst-inspect-1.0 pylonsrc 2>/dev/null
}

run_pipeline() {
  if [[ "$(uname -s)" == Linux* ]] && command -v timeout >/dev/null 2>&1; then
    timeout 60 gst-launch-1.0 -q pylonsrc device-serial-number=0815-0000 num-buffers=5 ! fakesink 2>&1
  elif [[ "$(uname -s)" == Darwin* ]] && command -v gtimeout >/dev/null 2>&1; then
    gtimeout 60 gst-launch-1.0 -q pylonsrc device-serial-number=0815-0000 num-buffers=5 ! fakesink 2>&1
  else
    # Meson also enforces the overall test timeout.
    gst-launch-1.0 -q pylonsrc device-serial-number=0815-0000 num-buffers=5 ! fakesink 2>&1
  fi
}

require_inspect_property() {
  local file="$1"
  local property="$2"
  if ! grep -Eq "^[[:space:]]+$property[[:space:]]*:" "$file"; then
    echo "FAIL: gst-inspect output is missing required property: $property"
    failed=1
  fi
}

extract_sdk_version() {
  sed -nE 's/^[[:space:]]*Description[[:space:]]+Basler\/Pylon plugin for pylon SDK[[:space:]]+(.+)$/\1/p' "$1" | head -n 1
}

mkdir -p "$GOLDEN_DIR"
cd "$PROJECT_ROOT"

# Use an isolated temporary cache so we get deterministic output without
# touching the user's ~/.cache/gstpylon.
TMP_CACHE="$(mktemp -d)"
trap 'rm -rf "$TMP_CACHE"' EXIT
export XDG_CACHE_HOME="$TMP_CACHE"

source "$SCRIPT_DIR/../check_plugin_path.sh"
check_plugin_path

if $generate; then
  echo "Generating golden files..."
  run_gst_inspect | normalize_inspect | extract_inspect_surface > "$GOLDEN_DIR/gst_inspect_pylonsrc.txt"
  run_pipeline
  echo "Golden files written to $GOLDEN_DIR"
  exit 0
fi

# Compare mode
if [[ ! -f "$GOLDEN_DIR/gst_inspect_pylonsrc.txt" ]]; then
  echo "No golden files found. Run with --generate on main first."
  exit 1
fi

echo "Running inspect-surface regression tests..."

failed=0
skipped=0

# Test 1: gst-inspect surface
actual_inspect=$(mktemp)
run_gst_inspect | normalize_inspect | extract_inspect_surface > "$actual_inspect"
require_inspect_property "$actual_inspect" "device-serial-number"
require_inspect_property "$actual_inspect" "pfs-location"
require_inspect_property "$actual_inspect" "capture-error"
require_inspect_property "$actual_inspect" "max-pending-frames"
require_inspect_property "$actual_inspect" "enable-correction"
require_inspect_property "$actual_inspect" "user-set"
require_inspect_property "$actual_inspect" "cam"
require_inspect_property "$actual_inspect" "stream"

golden_sdk="$(extract_sdk_version "$GOLDEN_DIR/gst_inspect_pylonsrc.txt")"
actual_sdk="$(extract_sdk_version "$actual_inspect")"
golden_bayer="$(extract_bayer_formats "$GOLDEN_DIR/gst_inspect_pylonsrc.txt")"
actual_bayer="$(extract_bayer_formats "$actual_inspect")"
# Compare against a normalized golden so older goldens still match after
# normalize_inspect grows new rules (blank lines, action syntax, etc.).
normalized_golden=$(mktemp)
normalize_inspect < "$GOLDEN_DIR/gst_inspect_pylonsrc.txt" > "$normalized_golden"
if [[ -n "$golden_sdk" && -n "$actual_sdk" && "$golden_sdk" != "$actual_sdk" && "${PYLON_COMPAT_STRICT_GOLDEN:-0}" != "1" ]]; then
  echo "SKIP: golden diff skipped (golden is for pylon SDK $golden_sdk, built against $actual_sdk; set PYLON_COMPAT_STRICT_GOLDEN=1 to enforce)"
  skipped=1
elif [[ -n "$golden_bayer" && -n "$actual_bayer" && "$golden_bayer" != "$actual_bayer" && "${PYLON_COMPAT_STRICT_GOLDEN:-0}" != "1" ]]; then
  echo "SKIP: golden diff skipped (Bayer pad formats differ; golden expects GStreamer formats unavailable in this build; set PYLON_COMPAT_STRICT_GOLDEN=1 to enforce)"
  skipped=1
elif ! diff -u "$normalized_golden" "$actual_inspect"; then
  echo "FAIL: gst-inspect output differs from golden"
  failed=1
else
  echo "PASS: gst-inspect matches golden"
fi
rm -f "$actual_inspect" "$normalized_golden"

# Test 2: pipeline runs to completion
if run_pipeline; then
  echo "PASS: pipeline completes"
else
  echo "FAIL: pipeline did not complete"
  failed=1
fi

if [[ $failed -eq 1 ]]; then
  echo ""
  echo "Inspect-surface regression check FAILED. Review intentional API changes before updating the golden file."
  exit 1
fi

if [[ $skipped -eq 1 ]]; then
  echo ""
  echo "Required-property and pipeline checks passed; golden diff skipped due to SDK or GStreamer pad-format mismatch."
  exit 77
fi

echo ""
echo "All compatibility checks passed."
