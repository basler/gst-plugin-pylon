#!/usr/bin/env bash
# Build once on Ubuntu 22.04, then install-test the same debs on every target.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYLON_SDK_TGZ="${PYLON_SDK_TGZ:-}"
OUT_DIR="${OUT_DIR:-$ROOT/../deb-out-build-once-amd64}"
if (( $# )); then
  TARGETS=("$@")
else
  TARGETS=(ubuntu:22.04 ubuntu:24.04 debian:bookworm)
fi

if [[ -z "$PYLON_SDK_TGZ" || ! -f "$PYLON_SDK_TGZ" ]]; then
  echo "Set PYLON_SDK_TGZ to a pylon_sdk.tar.gz containing pylon/." >&2
  echo "For an installed compatible SDK:" >&2
  echo "  sudo tar -czf /tmp/pylon_sdk.tar.gz -C /opt pylon" >&2
  exit 1
fi

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

echo "=== Build once: ubuntu:22.04 / amd64 ==="
docker run --rm \
  --platform linux/amd64 \
  -v "$ROOT:/src:ro" \
  -v "$PYLON_SDK_TGZ:/pylon-sdk/pylon_sdk.tar.gz:ro" \
  -v "$OUT_DIR:/out" \
  -e DEBIAN_FRONTEND=noninteractive \
  -e PYLON_ROOT=/opt/pylon \
  -e PYLON_CAMEMU=4 \
  ubuntu:22.04 \
  bash -c '
set -euo pipefail
apt-get update -qq
apt-get install -y -qq \
  build-essential ca-certificates cmake debhelper dh-python fakeroot git \
  gir1.2-gstreamer-1.0 gstreamer1.0-plugins-base gstreamer1.0-tools \
  libgstreamer-plugins-base1.0-dev libgstreamer1.0-dev \
  meson ninja-build pkg-config python3-dev python3-gi python3-setuptools

cp -a /src /build
cd /build
git config --global --add safe.directory /build
ln -sfn packaging/debian debian
chmod +x packaging/debian/rules \
  tools/patch_deb_changelog.sh tools/register_pylon_from_tree.sh
tools/register_pylon_from_tree.sh /pylon-sdk/pylon_sdk.tar.gz
tools/patch_deb_changelog.sh

PYLON_ROOT=/opt/pylon \
PYLON_CAMEMU=4 \
LD_LIBRARY_PATH=/opt/pylon/lib \
dpkg-buildpackage -us -uc -b -rfakeroot

cp -v ../*.deb ../*.buildinfo ../*.changes /out/
'

echo "=== Built artifacts ==="
ls -l "$OUT_DIR"

for image in "${TARGETS[@]}"; do
  echo "=== Install-test unchanged packages: $image / amd64 ==="
  docker run --rm \
    --platform linux/amd64 \
    -v "$ROOT:/src:ro" \
    -v "$PYLON_SDK_TGZ:/pylon-sdk/pylon_sdk.tar.gz:ro" \
    -v "$OUT_DIR:/packages:ro" \
    -e PYLON_CAMEMU=4 \
    "$image" \
    bash /src/tools/test_deb_compatibility.sh \
      /packages /pylon-sdk/pylon_sdk.tar.gz
done

echo "=== SUCCESS: one jammy build passed on all target distributions ==="
