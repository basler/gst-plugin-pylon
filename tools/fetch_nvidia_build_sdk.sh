#!/usr/bin/env bash
# Download the real NVIDIA Jetson build inputs used by nvidia-profile CI.
# The files remain NVIDIA-owned build inputs and must not be release artifacts.
set -euo pipefail

DEEPSTREAM_VERSION="${1:-}"
CUDA_VERSION="${2:-}"
L4T_VERSION="${3:-}"
OUT_DIR="${4:-nvidia-sdk-downloads}"

if [[ -z "$DEEPSTREAM_VERSION" || -z "$CUDA_VERSION" || -z "$L4T_VERSION" ]]; then
  echo "usage: $0 <deepstream-version> <cuda-version> <l4t-version> [output-directory]" >&2
  exit 1
fi

case "$DEEPSTREAM_VERSION:$CUDA_VERSION:$L4T_VERSION" in
  6.4:12.2.2:36.2.0|7.0:12.2.2:36.3.0|7.1:12.6.3:36.4.3) ;;
  *)
    echo "Unsupported Jetson SDK triple: DeepStream $DEEPSTREAM_VERSION / CUDA $CUDA_VERSION / L4T $L4T_VERSION" >&2
    exit 1
    ;;
esac

mkdir -p "$OUT_DIR"

DS_FILE="$OUT_DIR/deepstream_sdk_v${DEEPSTREAM_VERSION}.0_jetson.tbz2"
DS_URL="https://api.ngc.nvidia.com/v2/resources/nvidia/deepstream/versions/${DEEPSTREAM_VERSION}/files/$(basename "$DS_FILE")"

if [[ ! -s "$DS_FILE" ]]; then
  curl --fail --location --retry 5 --retry-all-errors \
    "$DS_URL" --output "$DS_FILE"
fi

CUDA_METADATA="$OUT_DIR/redistrib_${CUDA_VERSION}.json"
CUDA_BASE="https://developer.download.nvidia.com/compute/cuda/redist"
curl --fail --location --retry 5 --retry-all-errors \
  "$CUDA_BASE/$(basename "$CUDA_METADATA")" --output "$CUDA_METADATA"

readarray -t CUDA_PACKAGES < <(
  python3 - "$CUDA_METADATA" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    data = json.load(stream)

# cuda_runtime.h pulls in crt/host_config.h from the nvcc redist, not cudart.
for name in ("cuda_cudart", "cuda_nvcc", "cuda_cccl"):
    item = data.get(name) or {}
    plat = next((p for p in ("linux-aarch64", "linux-sbsa") if p in item), None)
    if plat is None:
        sys.exit(f"No linux-aarch64/linux-sbsa package for {name}")
    info = item[plat]
    print(f"{info['relative_path']}\t{info['sha256']}")
PY
)

CUDA_FILES=()
for spec in "${CUDA_PACKAGES[@]}"; do
  CUDA_RELATIVE_PATH="${spec%%$'\t'*}"
  CUDA_SHA256="${spec#*$'\t'}"
  CUDA_FILE="$OUT_DIR/$(basename "$CUDA_RELATIVE_PATH")"
  if [[ ! -s "$CUDA_FILE" ]] ||
     ! echo "$CUDA_SHA256  $CUDA_FILE" | sha256sum --check --status; then
    rm -f "$CUDA_FILE"
    curl --fail --location --retry 5 --retry-all-errors \
      "$CUDA_BASE/$CUDA_RELATIVE_PATH" --output "$CUDA_FILE"
  fi
  echo "$CUDA_SHA256  $CUDA_FILE" | sha256sum --check
  CUDA_FILES+=("$CUDA_FILE")
done

# libnvbufsurface.so lives in L4T multimedia-utils, not in the DeepStream tarball.
L4T_DIST="r$(echo "$L4T_VERSION" | cut -d. -f1-2)"
L4T_REPO="https://repo.download.nvidia.com/jetson"
MM_FILE=""
for feed in t234 common; do
  packages="$OUT_DIR/Packages.${feed}.${L4T_DIST}"
  if curl --fail --location --retry 5 --retry-all-errors \
       "$L4T_REPO/$feed/dists/$L4T_DIST/main/binary-arm64/Packages.gz" \
       --output "${packages}.gz"; then
    gzip -df "${packages}.gz"
    relative="$(python3 - "$packages" "$L4T_VERSION" <<'PY'
import sys

wanted = "nvidia-l4t-multimedia-utils"
prefix = sys.argv[2]
pkg = ver = filename = None
matches = []
with open(sys.argv[1], encoding="utf-8", errors="replace") as stream:
    for raw in stream:
        line = raw.strip()
        if not line:
            if pkg == wanted and ver and filename and ver.startswith(prefix):
                matches.append((ver, filename))
            pkg = ver = filename = None
            continue
        key, _, value = line.partition(":")
        value = value.strip()
        if key == "Package":
            pkg = value
        elif key == "Version":
            ver = value
        elif key == "Filename":
            filename = value
if pkg == wanted and ver and filename and ver.startswith(prefix):
    matches.append((ver, filename))
if not matches:
    sys.exit(1)
matches.sort()
print(matches[-1][1])
PY
)" || relative=""
    if [[ -n "$relative" ]]; then
      MM_FILE="$OUT_DIR/$(basename "$relative")"
      curl --fail --location --retry 5 --retry-all-errors \
        "$L4T_REPO/$feed/$relative" --output "$MM_FILE"
      break
    fi
  fi
done

if [[ -z "$MM_FILE" || ! -s "$MM_FILE" ]]; then
  echo "Could not download nvidia-l4t-multimedia-utils for L4T $L4T_VERSION" >&2
  exit 1
fi

if [[ "$(dpkg-deb -f "$MM_FILE" Package)" != nvidia-l4t-multimedia-utils ]]; then
  echo "Downloaded file is not nvidia-l4t-multimedia-utils:" >&2
  dpkg-deb -I "$MM_FILE" >&2 || true
  exit 1
fi
printf 'DeepStream=%s\nCUDA=%s\nL4T_multimedia_utils=%s\n' \
  "$DS_FILE" "${CUDA_FILES[*]}" "$MM_FILE"
