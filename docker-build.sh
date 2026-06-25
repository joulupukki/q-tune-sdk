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
# line in its CMakeLists.txt. For that path to resolve inside the container, the
# SDK root is mounted at the SAME absolute path it has on the host — so whether
# your project lives inside this SDK repo (the examples) or anywhere else on disk
# (a project created by tools/new_plugin.py), the build "just works" with Docker.
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

# Mount the SDK at its own host path so an absolute QTUNE_SDK_DIR resolves inside
# the container. The build runs in the project directory (also at its host path).
# When the project lives inside the SDK repo, the SDK mount already contains it,
# so we avoid a redundant (and illegal) overlapping second mount.
MOUNTS=(-v "${SDK_ROOT}:${SDK_ROOT}")
if [[ "${PROJECT_DIR}/" != "${SDK_ROOT}/"* ]]; then
  MOUNTS+=(-v "${PROJECT_DIR}:${PROJECT_DIR}")
fi

echo "Building plugin in: ${PROJECT_DIR}"
echo "Mounting SDK:       ${SDK_ROOT}"
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
  "${MOUNTS[@]}" \
  -w "${PROJECT_DIR}" \
  -e IDF_TARGET=esp32s3 \
  "${IMAGE}" \
  sh -c 'grep -q "CONFIG_IDF_TARGET=\"esp32s3\"" sdkconfig 2>/dev/null || idf.py set-target esp32s3; idf.py build'
