#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Boyd Timothy
#
# Build a Q-Tune plugin inside the pinned ESP-IDF v5.3.2 container — no local
# ESP-IDF, Python, or pyelftools required, only Docker.
#
# Usage:
#   ./docker-build.sh [plugin-project-dir]   # default: current directory
#
# Examples:
#   ./docker-build.sh plugins/tuner/gauge
#   cd plugins/standby/bouncer && ../../../docker-build.sh
#
# How the SDK is found inside the container: the SDK and the project are mounted
# at FIXED container paths (/qtune-sdk and /project), and QTUNE_SDK_DIR is
# overridden to /qtune-sdk for the build. The absolute QTUNE_SDK_DIR baked into a
# project's CMakeLists.txt is therefore only a fallback for non-Docker local
# builds — Docker builds never depend on the host path, so this works the same on
# macOS, Linux, and Windows (see docker-build.ps1 for the PowerShell twin).
#
# After a successful build the plugin is validated automatically (the container
# already ships python3 + pyelftools). The resulting <name>.so is written to
# <plugin-project-dir>/build/.

set -euo pipefail

IMAGE="espressif/idf:v5.3.2"
SDK_ROOT="$(cd "$(dirname "$0")" && pwd)"

PROJECT_DIR="${1:-$PWD}"
if [ ! -f "${PROJECT_DIR}/CMakeLists.txt" ]; then
  echo "error: no CMakeLists.txt in '${PROJECT_DIR}' — point this at a plugin project dir." >&2
  exit 1
fi
PROJECT_DIR="$(cd "${PROJECT_DIR}" && pwd)"

# Fail early with a friendly message if Docker isn't installed or not running.
if ! command -v docker >/dev/null 2>&1; then
  echo "error: Docker is not installed. Install Docker Desktop: https://docs.docker.com/get-docker/" >&2
  exit 1
fi
if ! docker info >/dev/null 2>&1; then
  echo "error: Docker is installed but not running. Start Docker Desktop and try again." >&2
  exit 1
fi

# Warn before the first build: pulling the pinned image is a multi-GB download
# that can take several minutes, so it doesn't look like the build has hung.
if ! docker image inspect "${IMAGE}" >/dev/null 2>&1; then
  echo "note: ${IMAGE} isn't cached yet — this first build downloads it (a few GB)" >&2
  echo "      and can take several minutes. Later builds reuse the cache and are fast." >&2
fi

echo "Building plugin in: ${PROJECT_DIR}"
echo "Mounting SDK:       ${SDK_ROOT}  ->  /qtune-sdk (read-only)"
echo "Using image:        ${IMAGE}"

# The base image's entrypoint sources $IDF_PATH/export.sh before exec'ing the
# command, so idf.py and cmake are on PATH.
#
# Mount the SDK read-only at /qtune-sdk and the project read-write at /project,
# then override QTUNE_SDK_DIR to the fixed container path.
#
# IMPORTANT: only run `idf.py set-target` when the project is NOT already
# configured for esp32s3. set-target wipes the build configuration, which forces
# a full from-scratch rebuild of LVGL + ESP-IDF every time. Skipping it on
# subsequent builds makes them incremental — editing one plugin source then
# recompiles just that file and relinks the .so.
#
# After the build, `cmake --build build --target validate` runs the same checks
# the firmware loader applies (exported symbols, ABI, LVGL version, uid) and
# fails the build if the .so wouldn't load.
exec docker run --rm -t \
  -v "${SDK_ROOT}:/qtune-sdk:ro" \
  -v "${PROJECT_DIR}:/project" \
  -w /project \
  -e IDF_TARGET=esp32s3 \
  "${IMAGE}" \
  sh -c 'set -e
         DEF=-DQTUNE_SDK_DIR=/qtune-sdk
         grep -q "CONFIG_IDF_TARGET=\"esp32s3\"" sdkconfig 2>/dev/null || idf.py $DEF set-target esp32s3
         idf.py $DEF build
         cmake --build build --target validate'
