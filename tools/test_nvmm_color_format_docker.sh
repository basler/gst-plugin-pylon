#!/usr/bin/env bash
# Compile and run the NVMM color-format mapping test against NVIDIA's real
# nvbufsurface.h inside a container (no Jetson GPU required).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${NVMM_FORMAT_TEST_IMAGE:-ubuntu:22.04}"
# DeepStream public header tree on GitHub (same enums as Jetson DS packages).
NVBUF_URL="${NVBUF_URL:-https://raw.githubusercontent.com/NVIDIA/DeepStream/master/includes/nvbufsurface.h}"

docker run --rm \
  -v "$ROOT:/src:ro" \
  -w /src \
  "$IMAGE" \
  bash -c '
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq g++ pkg-config libgstreamer-plugins-base1.0-dev curl ca-certificates >/dev/null

inc=$(mktemp -d)
curl --fail --location --silent --show-error "'"$NVBUF_URL"'" -o "$inc/nvbufsurface.h"
grep -q NVBUF_COLOR_FORMAT_YUYV_ER "$inc/nvbufsurface.h"
grep -q NVBUF_COLOR_FORMAT_UYVY_ER "$inc/nvbufsurface.h"

g++ -std=c++17 -O0 -g -DNVMM_ENABLED \
  -I"$inc" -Iext/pylon \
  $(pkg-config --cflags gstreamer-1.0 gstreamer-video-1.0) \
  tests/check/generic/nvmmformat.cpp ext/pylon/gstpylonnvmmcolor.cpp \
  $(pkg-config --libs gstreamer-1.0 gstreamer-video-1.0) \
  -o /tmp/generic_nvmmformat

/tmp/generic_nvmmformat
'
