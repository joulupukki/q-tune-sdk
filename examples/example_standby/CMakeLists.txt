# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Boyd Timothy

cmake_minimum_required(VERSION 3.16)

# Standard ESP-IDF project() setup. project() triggers the component manager to
# download the dependencies in main/idf_component.yml (lvgl, elf_loader).
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(example_standby)

# Path to the q-tune-sdk repo root (this example lives at
# <repo>/examples/example_standby). When you copy this example OUT of the SDK
# repo, change THIS ONE LINE to point at your local q-tune-sdk checkout, e.g.
#   set(QTUNE_SDK_DIR "$ENV{HOME}/src/q-tune-sdk")
if(NOT DEFINED QTUNE_SDK_DIR)
    set(QTUNE_SDK_DIR "${CMAKE_SOURCE_DIR}/../..")  # default: SDK repo root
endif()

# Publish the SDK + LVGL include roots into the global build properties so the
# standalone .so compile (qtune_project_so) can find the headers.
idf_build_set_property(COMPILE_INCLUDE_DIRECTORIES
    "${QTUNE_SDK_DIR}/include" APPEND)

idf_component_get_property(lvgl_dir lvgl__lvgl COMPONENT_DIR)
idf_build_set_property(COMPILE_INCLUDE_DIRECTORIES "${lvgl_dir}" APPEND)
idf_build_set_property(COMPILE_INCLUDE_DIRECTORIES "${lvgl_dir}/src" APPEND)
idf_build_set_property(COMPILE_INCLUDE_DIRECTORIES "${lvgl_dir}/.." APPEND)

# Q-Tune SDK C++ shared-object builder (stock elf_loader project_so is C-only).
# Output: build/example_standby.so
include(${QTUNE_SDK_DIR}/cmake/qtune_plugin.cmake)
qtune_project_so(example_standby)

# Optional offline validation. After `idf.py build`, run:
#   cmake --build build --target validate
# to check the .so the same way the firmware loader will (requires python3 +
# pyelftools: pip install pyelftools).
add_custom_target(validate
    COMMAND python3 ${QTUNE_SDK_DIR}/tools/validate_plugin.py
            ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.so
    USES_TERMINAL)
add_dependencies(validate so)
