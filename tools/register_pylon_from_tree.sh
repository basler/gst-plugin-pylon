#!/bin/bash
# Unpack a Conan/SDK pylon tree and register a dummy "pylon" dpkg so
# debian/control Build-Depends and tools/patch_deb_changelog.sh work.
set -euo pipefail

SDK_ARCHIVE="${1:-}"
DEST="${PYLON_ROOT:-/opt/pylon}"
VERSION="${PYLON_PKG_VERSION:-26.06}"

if [[ -z "$SDK_ARCHIVE" ]]; then
  echo "usage: $0 <pylon_sdk.tar.gz>" >&2
  exit 1
fi

if [[ ! -f "$SDK_ARCHIVE" ]]; then
  # download-artifact sometimes nests the file
  found="$(find "$(dirname "$SDK_ARCHIVE")" -name 'pylon_sdk.tar.gz' -type f 2>/dev/null | head -1 || true)"
  if [[ -n "$found" ]]; then
    SDK_ARCHIVE="$found"
  else
    echo "ERROR: SDK archive not found: $SDK_ARCHIVE" >&2
    ls -la "$(dirname "$SDK_ARCHIVE")" >&2 || true
    exit 1
  fi
fi

tmp="$(mktemp -d)"
tar -xzf "$SDK_ARCHIVE" -C "$tmp"

root=""
if [[ -d "$tmp/pylon/include/pylon" ]]; then
  root="$tmp/pylon"
elif [[ -d "$tmp/include/pylon" ]]; then
  root="$tmp"
else
  root="$(find "$tmp" -type d -name pylon -path '*/include/pylon' -printf '%h\n' | head -1 || true)"
  root="${root%/include}"
fi

if [[ -z "$root" || ! -d "$root" ]]; then
  echo "ERROR: could not locate pylon include/ tree in $SDK_ARCHIVE" >&2
  find "$tmp" -maxdepth 4 -type d >&2 || true
  exit 1
fi

mkdir -p "$(dirname "$DEST")"
rm -rf "$DEST"
cp -a "$root" "$DEST"
rm -rf "$tmp"

if [[ -d "$DEST/share/pylon" ]]; then
  shopt -s nullglob
  debs=( "$DEST"/share/pylon/*.deb )
  if (( ${#debs[@]} )); then
    apt-get install -y --fix-broken "${debs[@]}" || true
  fi
  shopt -u nullglob
fi

if ! dpkg -s pylon >/dev/null 2>&1; then
  apt-get update
  apt-get install -y equivs
  ctl="$(mktemp)"
  cat > "$ctl" <<EOF
Section: misc
Priority: optional
Standards-Version: 4.6.2
Package: pylon
Version: ${VERSION}
Architecture: all
Description: CI stub for a pylon SDK tree at ${DEST}
EOF
  (cd /tmp && equivs-build "$ctl")
  deb="$(ls /tmp/pylon_*.deb | head -1)"
  dpkg -i "$deb" || apt-get install -y -f
  rm -f "$ctl"
fi

test -d "$DEST"
echo "Pylon tree installed at $DEST"
dpkg -s pylon | grep -E '^(Package|Version):' || true
