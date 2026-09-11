#!/usr/bin/env bash
# Install downloaded NVIDIA build inputs into an ephemeral CI container.
set -euo pipefail

DEEPSTREAM_VERSION="${1:-}"
CUDA_VERSION="${2:-}"
L4T_VERSION="${3:-}"
DOWNLOAD_DIR="${4:-/nvidia-sdk}"

if [[ -z "$DEEPSTREAM_VERSION" || -z "$CUDA_VERSION" || -z "$L4T_VERSION" ]]; then
  echo "usage: $0 <deepstream-version> <cuda-version> <l4t-version> [download-directory]" >&2
  exit 1
fi

fail_tree() {
  echo "$1" >&2
  ls -la "$DOWNLOAD_DIR" >&2 || true
  set +o pipefail
  tar -tjf "$DS_ARCHIVE" 2>/dev/null | head -40 >&2 || true
  set -o pipefail
  exit 1
}

DS_ARCHIVE="$DOWNLOAD_DIR/deepstream_sdk_v${DEEPSTREAM_VERSION}.0_jetson.tbz2"
mapfile -t CUDA_ARCHIVES < <(find "$DOWNLOAD_DIR" -maxdepth 1 -type f \
  -name "cuda_*-linux-*-archive.tar.*" | sort)
MM_DEB="$(find "$DOWNLOAD_DIR" -maxdepth 1 -type f \
  -name "nvidia-l4t-multimedia-utils_*.deb" -print -quit)"

if [[ ! -f "$DS_ARCHIVE" || ${#CUDA_ARCHIVES[@]} -eq 0 || -z "$MM_DEB" ]]; then
  fail_tree "Missing NVIDIA SDK archives in $DOWNLOAD_DIR"
fi

apt-get update -qq
apt-get install -y -qq bzip2 equivs xz-utils

tar -xjf "$DS_ARCHIVE" -C /
DS_ROOT="/opt/nvidia/deepstream/deepstream-${DEEPSTREAM_VERSION}"
if [[ ! -d "$DS_ROOT" ]]; then
  DS_ROOT="$(find /opt/nvidia/deepstream -mindepth 1 -maxdepth 1 -type d \
    -name 'deepstream-*' -print -quit || true)"
fi
[[ -n "$DS_ROOT" && -d "$DS_ROOT" ]] || fail_tree "DeepStream tree not found after extract"
[[ -f "$DS_ROOT/sources/includes/nvbufsurface.h" ]] || \
  fail_tree "nvbufsurface.h missing under $DS_ROOT"

ln -sfn "$(basename "$DS_ROOT")" /opt/nvidia/deepstream/deepstream
mkdir -p "$DS_ROOT/lib" /usr/lib/aarch64-linux-gnu/nvidia

# DeepStream's tarball does not ship the L4T nvbufsurface implementation.
# Extract the matching multimedia-utils package and put the linker-visible
# name where Meson and the plugin rpath expect it.
mm_tmp="$(mktemp -d)"
dpkg-deb -x "$MM_DEB" "$mm_tmp"
mapfile -t NVBUF_LIBS < <(find "$mm_tmp" -name 'libnvbufsurface.so*' -type f)
if (( ${#NVBUF_LIBS[@]} == 0 )); then
  echo "libnvbufsurface.so not in $MM_DEB" >&2
  find "$mm_tmp" -name '*nvbufsurface*' >&2 || true
  rm -rf "$mm_tmp"
  exit 1
fi
for lib in "${NVBUF_LIBS[@]}"; do
  install -m 0644 "$lib" /usr/lib/aarch64-linux-gnu/nvidia/
  install -m 0644 "$lib" "$DS_ROOT/lib/"
done
rm -rf "$mm_tmp"
if [[ ! -e "$DS_ROOT/lib/libnvbufsurface.so" ]]; then
  versioned="$(find "$DS_ROOT/lib" -maxdepth 1 -name 'libnvbufsurface.so.*' -print -quit)"
  ln -sfn "$(basename "$versioned")" "$DS_ROOT/lib/libnvbufsurface.so"
  ln -sfn "$(basename "$versioned")" /usr/lib/aarch64-linux-gnu/nvidia/libnvbufsurface.so
fi
test -f "$DS_ROOT/lib/libnvbufsurface.so"

mkdir -p /usr/local/cuda
for archive in "${CUDA_ARCHIVES[@]}"; do
  tar -xf "$archive" -C /usr/local/cuda --strip-components=1
done
if [[ -d /usr/local/cuda/lib && ! -d /usr/local/cuda/lib64 ]]; then
  ln -s lib /usr/local/cuda/lib64
fi
test -f /usr/local/cuda/include/cuda_runtime.h
test -f /usr/local/cuda/include/crt/host_config.h
find /usr/local/cuda -name 'libcudart.so*' -print -quit | grep -q .

# The tar installations provide the actual SDK files but not dpkg metadata.
# Register only the DeepStream package name required by debian/control.
PKG="deepstream-${DEEPSTREAM_VERSION}"
if ! dpkg-query -W -f='${Status}' "$PKG" 2>/dev/null | grep -q 'install ok installed'; then
  ctl="$(mktemp)"
  cat >"$ctl" <<EOF
Section: misc
Priority: optional
Standards-Version: 4.6.2
Package: ${PKG}
Version: ${DEEPSTREAM_VERSION}.0-ci
Architecture: all
Description: CI registration for NVIDIA DeepStream SDK installed from NGC
EOF
  (cd /tmp && equivs-build "$ctl")
  dpkg -i "/tmp/${PKG}_${DEEPSTREAM_VERSION}.0-ci_all.deb"
  rm -f "$ctl"
fi

cat >/etc/nv_tegra_release <<EOF
# R${L4T_VERSION%%.*} (release), REVISION: ${L4T_VERSION#*.}, GCID: 00000000, BOARD: CI, EABI: aarch64, DATE: Mon Jan  1 00:00:00 UTC 2024
EOF

ldconfig
dpkg-query -W "$PKG"
echo "Installed DeepStream $DEEPSTREAM_VERSION, CUDA $CUDA_VERSION, L4T $L4T_VERSION build inputs"
