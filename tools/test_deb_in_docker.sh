#!/usr/bin/env bash
# Build gst-plugin-pylon Debian packages inside a clean container.
# Usage:
#   tools/test_deb_in_docker.sh [ubuntu:24.04|ubuntu:22.04|debian:bookworm]
set -euo pipefail

IMAGE="${1:-ubuntu:24.04}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYLON_DEBS_TGZ="${PYLON_DEBS_TGZ:-/home/thies/Downloads/pylon-26.02.1_linux-x86_64_debs.tar.gz}"
OUT_DIR="${OUT_DIR:-$ROOT/../deb-out-$(echo "$IMAGE" | tr ':/' '--')}"

if [[ ! -f "$PYLON_DEBS_TGZ" ]]; then
  echo "ERROR: pylon debs tarball not found: $PYLON_DEBS_TGZ"
  echo "Set PYLON_DEBS_TGZ to a pylon_*_linux-x86_64_debs.tar.gz"
  exit 1
fi

mkdir -p "$OUT_DIR"

echo "=== Building Debian packages in $IMAGE ==="
echo "Source: $ROOT"
echo "Pylon:  $PYLON_DEBS_TGZ"
echo "Output: $OUT_DIR"

docker run --rm \
  --platform linux/amd64 \
  -v "$ROOT:/src:ro" \
  -v "$PYLON_DEBS_TGZ:/pylon-debs.tar.gz:ro" \
  -v "$OUT_DIR:/out" \
  -e DEBIAN_FRONTEND=noninteractive \
  -e PYLON_ROOT=/opt/pylon \
  -e PYLON_CAMEMU=3 \
  -e DEB_BUILD_OPTIONS=noautodbgsym \
  "$IMAGE" \
  bash -c '
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

echo "--- apt update / base tools ---"
apt-get update -qq
apt-get install -y -qq \
  build-essential debhelper dh-python fakeroot \
  meson ninja-build cmake pkg-config \
  python3-dev python3-setuptools python3-gi pybind11-dev \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-tools gstreamer1.0-plugins-base \
  gstreamer1.0-python3-plugin-loader \
  git ca-certificates

echo "--- install pylon ---"
mkdir -p /pylon-installer
tar -xvf /pylon-debs.tar.gz -C /pylon-installer
# Codemeter can fail on broken deps in minimal containers; install pylon with --fix-broken like CI
apt-get install -y /pylon-installer/codemeter_*.deb || true
apt-get install -y --fix-broken /pylon-installer/pylon_*.deb
test -d /opt/pylon

echo "--- prepare source tree ---"
# Work on a writable copy (source mount is read-only)
rm -rf /build
cp -a /src /build
cd /build
# Avoid dirty git describe noise from docker copy metadata if any
git config --global --add safe.directory /build || true

ln -sfn packaging/debian debian
chmod +x packaging/debian/rules tools/patch_deb_changelog.sh
tools/patch_deb_changelog.sh

echo "--- meson version ---"
meson --version
dpkg -s debhelper | grep -E "^Version"

echo "--- dpkg-buildpackage ---"
PYLON_ROOT=/opt/pylon PYLON_CAMEMU=3 dpkg-buildpackage -us -uc -b -rfakeroot

echo "--- collect artifacts ---"
mkdir -p /out
cp -v ../*.deb /out/ || true
cp -v ../*.buildinfo /out/ || true
cp -v ../*.changes /out/ || true

echo "--- verify package contents ---"
set +e
FAIL=0
for deb in /out/*.deb; do
  echo "== $(basename "$deb") =="
  dpkg-deb -I "$deb" | sed -n "1,40p"
  echo "-- files --"
  dpkg-deb -c "$deb" | sed -n "1,80p"
done

# Required payload checks
dpkg-deb -c /out/gst-plugin-pylon_*.deb | grep -E "libgstpylon\\.so|gstreamer-1\\.0/.*gstpylon" || {
  echo "FAIL: plugin .so missing from gst-plugin-pylon"
  FAIL=1
}
dpkg-deb -c /out/gst-plugin-pylon-dev_*.deb | grep -E "gstpylonmeta\\.h|gstpylon-1\\.0\\.pc" || {
  echo "FAIL: headers/pc missing from -dev"
  FAIL=1
}
dpkg-deb -c /out/python3-pygstpylon_*.deb | grep -E "dist-packages/.*pygstpylon" || {
  echo "FAIL: pygstpylon not under dist-packages"
  FAIL=1
}

# Install and smoke-test
apt-get install -y /out/gst-plugin-pylon_*.deb /out/python3-pygstpylon_*.deb
export GST_PLUGIN_PATH=""
export LD_LIBRARY_PATH=/opt/pylon/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
gst-inspect-1.0 pylonsrc | head -40
PYLON_CAMEMU=3 gst-launch-1.0 -q pylonsrc device-serial-number=0815-0000 num-buffers=5 ! fakesink
python3 -c "import pygstpylon; print(\"pygstpylon import ok\", pygstpylon)"

if [[ "$FAIL" -ne 0 ]]; then
  echo "PACKAGE CONTENT CHECKS FAILED"
  exit 1
fi
echo "=== SUCCESS: packages built and smoke-tested in container ==="
'
