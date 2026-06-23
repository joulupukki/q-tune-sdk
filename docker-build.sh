#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Boyd Timothy
#
# Build a Q-Tune plugin inside the pinned ESP-IDF v5.3.2 container — no local
# ESP-IDF install required, only Docker.
#
# Usage:
#   ./docker-build.sh [plugin-project-dir]   # default: current directory
#
# Examples:
#   cd examples/example_tuner && ../../docker-build.sh
#   ./docker-build.sh examples/example_standby
#
# The resulting <name>.so is written to <plugin-project-dir>/build/.

set -euo pipefail

IMAGE="espressif/idf:v5.3.2"

PROJECT_DIR="${1:-$PWD}"
if [ ! -f "${PROJECT_DIR}/CMakeLists.txt" ]; then
  echo "error: no CMakeLists.txt in '${PROJECT_DIR}' — point this at a plugin project dir." >&2
  exit 1
fi
PROJECT_DIR="$(cd "${PROJECT_DIR}" && pwd)"

echo "Building plugin in: ${PROJECT_DIR}"
echo "Using image:        ${IMAGE}"

# The base image's entrypoint sources $IDF_PATH/export.sh before exec'ing the
# command, so idf.py is on PATH. set-target makes the build use esp32s3 even on
# a fresh checkout with no sdkconfig.
exec docker run --rm -t \
  -v "${PROJECT_DIR}":/project \
  -w /project \
  -e IDF_TARGET=esp32s3 \
  "${IMAGE}" \
  sh -c "idf.py set-target esp32s3 && idf.py build"
