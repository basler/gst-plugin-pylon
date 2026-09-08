#!/usr/bin/env bash
# Install and smoke-test one architecture's build-once Debian packages.
set -euo pipefail

PACKAGES_DIR="${1:-/packages}"
PYLON_SDK_ARCHIVE="${2:-/pylon-sdk/pylon_sdk.tar.gz}"
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export DEBIAN_FRONTEND=noninteractive
export PYLON_ROOT=/opt/pylon
export PYLON_CAMEMU="${PYLON_CAMEMU:-3}"

apt-get update -qq
apt-get install -y -qq \
  ca-certificates \
  gstreamer1.0-plugins-base \
  gstreamer1.0-tools \
  python3 \
  python3-gi \
  gir1.2-gstreamer-1.0

"$SOURCE_DIR/tools/register_pylon_from_tree.sh" "$PYLON_SDK_ARCHIVE"

shopt -s nullglob
plugin_debs=("$PACKAGES_DIR"/gst-plugin-pylon_[0-9]*.deb)
dev_debs=("$PACKAGES_DIR"/gst-plugin-pylon-dev_[0-9]*.deb)
python_debs=("$PACKAGES_DIR"/python3-pygstpylon_[0-9]*.deb)
shopt -u nullglob

if (( ${#plugin_debs[@]} != 1 ||
      ${#dev_debs[@]} != 1 ||
      ${#python_debs[@]} != 1 )); then
  echo "Expected exactly one runtime, development, and Python package:" >&2
  find "$PACKAGES_DIR" -maxdepth 2 -type f -name '*.deb' -print >&2
  exit 1
fi

depends="$(dpkg-deb -f "${plugin_debs[0]}" Depends)"
if [[ "$depends" != *"pylon (>= 26.06)"* ]]; then
  echo "Unexpected pylon compatibility: $depends" >&2
  exit 1
fi
if [[ "$depends" == *"pylon (<<"* ]]; then
  echo "Do not cap pylon by suite date or SDK number in Depends: $depends" >&2
  exit 1
fi

python_depends="$(dpkg-deb -f "${python_debs[0]}" Depends)"
if grep -Eq 'python3(:any)? \(<<' <<<"$python_depends"; then
  echo "Python package is tied to the build interpreter: $python_depends" >&2
  exit 1
fi
if ! dpkg-deb -c "${python_debs[0]}" | grep -q '/pygstpylon\.abi3\.so$'; then
  echo "python3-pygstpylon does not contain pygstpylon.abi3.so" >&2
  exit 1
fi
if dpkg-deb -c "${python_debs[0]}" | grep -q '/pygstpylon\.cpython-'; then
  echo "python3-pygstpylon contains a CPython-minor-specific module" >&2
  exit 1
fi

apt-get install -y \
  "${plugin_debs[0]}" \
  "${dev_debs[0]}" \
  "${python_debs[0]}"

export LD_LIBRARY_PATH="/opt/pylon/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
gst-inspect-1.0 pylonsrc >/dev/null
gst-launch-1.0 -q \
  pylonsrc device-serial-number=0815-0000 num-buffers=5 ! fakesink

python3 - <<'PY'
import platform

import gi

gi.require_version("Gst", "1.0")
from gi.repository import Gst
import pygstpylon

Gst.init(None)
pipeline = Gst.parse_launch(
    "pylonsrc device-serial-number=0815-0000 num-buffers=1 "
    "! appsink name=sink sync=false"
)
sink = pipeline.get_by_name("sink")
pipeline.set_state(Gst.State.PLAYING)
try:
    sample = sink.emit("try-pull-sample", 15 * Gst.SECOND)
    assert sample is not None
    meta = pygstpylon.gst_buffer_get_pylon_meta(hash(sample.get_buffer()))
    assert meta is not None
    assert meta.stride > 0
    assert isinstance(meta.chunks, dict)
    print(
        f"pygstpylon abi3 metadata OK: Python={platform.python_version()}, "
        f"module={pygstpylon.__version__}, "
        f"stride={meta.stride}"
    )
finally:
    pipeline.set_state(Gst.State.NULL)
    pipeline.get_state(10 * Gst.SECOND)
PY

echo "Debian package compatibility test passed"
