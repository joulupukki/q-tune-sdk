# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Boyd Timothy
#
# Reproducible build environment for Q-Tune UI plugins.
#
# Pinned to the exact ESP-IDF version the firmware is built with (v5.3.2). The
# base image already contains the toolchain and an entrypoint that sources
# $IDF_PATH/export.sh before running any command, so `idf.py` works directly.
#
# Build the image:   docker build -t q-tune-sdk .
# Build a plugin:    docker run --rm -v "$PWD":/project -w /project q-tune-sdk \
#                        sh -c "idf.py set-target esp32s3 && idf.py build"
# (or just use ./docker-build.sh, which wraps the above.)

FROM espressif/idf:v5.3.2

# Default target for plugins is the ESP32-S3 (the Q-Tune MCU).
ENV IDF_TARGET=esp32s3

WORKDIR /project

# Sensible default: build the project mounted at /project.
CMD ["sh", "-c", "idf.py set-target esp32s3 && idf.py build"]
