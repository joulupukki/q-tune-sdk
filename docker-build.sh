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
#   ./docker-build.sh examples/example_tuner
#   cd examples/example_standby && ../../docker-build.sh
#
# A plugin project references the SDK's include/ and cmake/ via the QTUNE_SDK_DIR
# line in its CMakeLists.txt (default: two levels up). For that path to resolve
# inside the container, the SDK root must be mounted — so when the project lives
# inside this SDK repo (e.g. the examples, or your own copy of one), this script
# mounts the whole SDK repo and builds in the project subdir. The simplest way to
# build your own plugin with Docker is therefore to keep it inside a checkout of
# this repo (copy an example and edit it).
#
# The resulting <name>.so is written to <plugin-project-dir>/build/.

set -euo pipefail

IMAGE="espressif/idf:v5.3.2"
SDK_ROOT="$(cd "$(dirname "$0")" && pwd)"

PROJECT_DIR="${1:-$PWD}"
if [ ! -f "${PROJECT_DIR}/CMakeLists.txt" ]; then
  echo "error: no CMakeLists.txt in '${PROJECT_DIR}' — point this at a plugin project dir." >&2
  exit 1
fi
PROJECT_DIR="$(cd "${PROJECT_DIR}" && pwd)"

# If the project lives inside the SDK repo, mount the whole repo so the project's
# QTUNE_SDK_DIR (../..) resolves to the mounted SDK root. Otherwise mount just the
# project (the project must then set an absolute QTUNE_SDK_DIR that is also
# reachable, or build natively).
if [[ "${PROJECT_DIR}/" == "${SDK_ROOT}/"* ]]; then
  rel="${PROJECT_DIR#"${SDK_ROOT}/"}"
  MOUNT="${SDK_ROOT}"
  WORKDIR="/work/${rel}"
else
  MOUNT="${PROJECT_DIR}"
  WORKDIR="/work"
fi

echo "Building plugin in: ${PROJECT_DIR}"
echo "Mounting:           ${MOUNT} -> /work"
echo "Using image:        ${IMAGE}"

# The base image's entrypoint sources $IDF_PATH/export.sh before exec'ing the
# command, so idf.py is on PATH.
#
# IMPORTANT: only run `idf.py set-target` when the project is NOT already
# configured for esp32s3. set-target wipes the build configuration, which forces
# a full from-scratch rebuild of LVGL + ESP-IDF every time. Skipping it on
# subsequent builds makes them incremental — editing one plugin source then
# recompiles just that file and relinks the .so.
exec docker run --rm -t \
  -v "${MOUNT}":/work \
  -w "${WORKDIR}" \
  -e IDF_TARGET=esp32s3 \
  "${IMAGE}" \
  sh -c 'grep -q "CONFIG_IDF_TARGET=\"esp32s3\"" sdkconfig 2>/dev/null || idf.py set-target esp32s3; idf.py build'
