#!/usr/bin/env bash
# Install official Basler pylon Debian packages from a *_debs.tar.gz archive
# or directory containing pylon_*.deb (+ optional codemeter*.deb).
set -euo pipefail

INPUT="${1:-}"
if [[ -z "$INPUT" ]]; then
  echo "usage: $0 <pylon_debs.tar.gz | directory | pylon_*.deb>" >&2
  exit 1
fi

if [[ ! -e "$INPUT" ]]; then
  found="$(find "$(dirname "$INPUT")" -name '*deb*.tar.gz' -type f 2>/dev/null | head -1 || true)"
  if [[ -n "$found" ]]; then
    INPUT="$found"
  else
    echo "ERROR: pylon Debian archive/path not found: $INPUT" >&2
    ls -la "$(dirname "$INPUT")" >&2 || true
    exit 1
  fi
fi

export DEBIAN_FRONTEND=noninteractive

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

if [[ -f "$INPUT" && "$INPUT" == *.tar.gz ]]; then
  tar -xzf "$INPUT" -C "$tmp"
elif [[ -f "$INPUT" && "$INPUT" == *.deb ]]; then
  cp -a "$INPUT" "$tmp/"
elif [[ -d "$INPUT" ]]; then
  shopt -s nullglob
  archives=("$INPUT"/*.tar.gz)
  raw_debs=("$INPUT"/*.deb)
  shopt -u nullglob
  if (( ${#archives[@]} > 0 )); then
    for archive in "${archives[@]}"; do
      tar -xzf "$archive" -C "$tmp"
    done
  elif (( ${#raw_debs[@]} > 0 )); then
    cp -a "${raw_debs[@]}" "$tmp/"
  else
    echo "ERROR: No .tar.gz or .deb files found in directory: $INPUT" >&2
    ls -la "$INPUT" >&2 || true
    exit 1
  fi
else
  echo "ERROR: Unsupported input for pylon deb installation: $INPUT" >&2
  exit 1
fi

mapfile -t all_debs < <(find "$tmp" -type f -name '*.deb' | sort)
if (( ${#all_debs[@]} == 0 )); then
  echo "ERROR: No .deb packages found after unpacking $INPUT" >&2
  find "$tmp" -maxdepth 3 >&2 || true
  exit 1
fi

codemeter_debs=()
pylon_debs=()
for deb in "${all_debs[@]}"; do
  pkg="$(dpkg-deb -f "$deb" Package 2>/dev/null || true)"
  case "$pkg" in
    codemeter|codemeter-lite)
      codemeter_debs+=("$deb")
      ;;
    pylon|libpylon*|pylon-dev|pylon-sdk*)
      pylon_debs+=("$deb")
      ;;
  esac
done

if (( ${#pylon_debs[@]} == 0 )); then
  echo "ERROR: Could not find 'Package: pylon' in $INPUT. Found packages:" >&2
  for deb in "${all_debs[@]}"; do
    echo "  $(basename "$deb"): $(dpkg-deb -f "$deb" Package Version 2>/dev/null || echo unknown)" >&2
  done
  exit 1
fi

apt-get update -qq

# CodeMeter daemon startup can fail in minimal containers without systemd/USB;
# allow postinst warning there as long as pylon itself installs cleanly.
if (( ${#codemeter_debs[@]} > 0 )); then
  apt-get install -y --no-install-recommends --fix-broken "${codemeter_debs[@]}" || true
fi

apt-get install -y --no-install-recommends --fix-broken "${pylon_debs[@]}"

status="$(dpkg-query -W -f='${Status}' pylon 2>/dev/null || true)"
if [[ "$status" != "install ok installed" ]]; then
  echo "ERROR: pylon dpkg status is '$status', expected 'install ok installed'" >&2
  exit 1
fi

if [[ ! -d /opt/pylon/include/pylon || ! -d /opt/pylon/lib ]]; then
  echo "ERROR: /opt/pylon/include/pylon or /opt/pylon/lib missing after installing pylon deb" >&2
  ls -la /opt/pylon >&2 || true
  exit 1
fi

echo "Official pylon Debian package installed at /opt/pylon:"
dpkg-query -W -f='  Package: ${Package}\n  Version: ${Version}\n  Architecture: ${Architecture}\n' pylon
