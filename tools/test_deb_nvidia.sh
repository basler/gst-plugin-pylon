#!/usr/bin/env bash
# Inspect nvidia-profile Debian artifacts: DeepStream Depends and NVMM link.
set -euo pipefail

PACKAGES_DIR="${1:-}"
DS_VERSION="${2:-}"
if [[ -z "$PACKAGES_DIR" || ! "$DS_VERSION" =~ ^(6\.4|7\.0|7\.1)$ ]]; then
  echo "usage: $0 <packages-dir> <6.4|7.0|7.1>" >&2
  exit 1
fi

shopt -s nullglob
plugin_debs=("$PACKAGES_DIR"/gst-plugin-pylon_[0-9]*.deb)
shopt -u nullglob
if (( ${#plugin_debs[@]} != 1 )); then
  echo "Expected exactly one gst-plugin-pylon_*.deb in $PACKAGES_DIR:" >&2
  find "$PACKAGES_DIR" -maxdepth 2 -type f -name '*.deb' -print >&2
  exit 1
fi

deb="${plugin_debs[0]}"
depends="$(dpkg-deb -f "$deb" Depends)"
version="$(dpkg-deb -f "$deb" Version)"
built_against="$(dpkg-deb -f "$deb" Pylon-Built-Against)"
installed_pylon="$(dpkg-query -W -f='${Version}' pylon)"
pkg="deepstream-${DS_VERSION}"

if [[ "$depends" != *"$pkg"* ]]; then
  echo "Depends does not mention $pkg: $depends" >&2
  exit 1
fi
if [[ "$depends" != *"pylon (>= 26.06)"* ]]; then
  echo "Unexpected pylon compatibility: $depends" >&2
  exit 1
fi
if [[ "$version" != *"~"* ]]; then
  echo "NVIDIA package Version should include an L4T suffix, got: $version" >&2
  exit 1
fi
if [[ "$built_against" != "$installed_pylon" ]]; then
  echo "Pylon-Built-Against=$built_against, expected $installed_pylon" >&2
  exit 1
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
dpkg-deb -x "$deb" "$tmp"
so="$(find "$tmp" -name 'libgstpylon.so' | head -1)"
if [[ -z "$so" ]]; then
  echo "libgstpylon.so missing from $deb" >&2
  exit 1
fi

needed="$(readelf -d "$so")"
if ! grep -q 'libnvbufsurface\.so' <<<"$needed"; then
  echo "libgstpylon.so is not linked to libnvbufsurface.so:" >&2
  echo "$needed" >&2
  exit 1
fi
if ! grep -q 'libcudart\.so' <<<"$needed"; then
  echo "libgstpylon.so is not linked to libcudart:" >&2
  echo "$needed" >&2
  exit 1
fi

echo "NVIDIA Debian package OK: $deb"
echo "  Version=$version"
echo "  Pylon-Built-Against=$built_against"
echo "  Depends=$depends"
echo "  NEEDED nvbufsurface+cudart"
