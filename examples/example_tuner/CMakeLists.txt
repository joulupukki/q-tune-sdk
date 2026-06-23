# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Boyd Timothy

cmake_minimum_required(VERSION 3.16)

# Standard ESP-IDF project() setup. project() triggers the component manager to
# download the dependencies in main/idf_component.yml (lvgl, elf_loader).
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(example_tuner)

# Path to the q-tune-sdk repo root (this example lives at
# <repo>/examples/example_tuner). When you copy this example OUT of the SDK repo,
# change THIS ONE LINE to point at your local q-tune-sdk checkout, e.g.
#   set(QTUNE_SDK_DIR "$ENV{HOME}/src/q-tune-sdk")
if(NOT DEFINED QTUNE_SDK_DIR)
    set(QTUNE_SDK_DIR "${CMAKE_SOURCE_DIR}/../..")  # default: SDK repo root
endif()

# qtune_project_so() compiles the plugin source standalone using only the GLOBAL
# COMPILE_INCLUDE_DIRECTORIES / COMPILE_DEFINITIONS build properties — it does NOT
# inherit a component's private INCLUDE_DIRS or lvgl's interface includes. Publish
# everything the plugin needs into the global properties here:
#   1. the SDK headers (qtune_plugin.h and friends), and
#   2. lvgl's header roots.
idf_build_set_property(COMPILE_INCLUDE_DIRECTORIES
    "${QTUNE_SDK_DIR}/include" APPEND)

idf_component_get_property(lvgl_dir lvgl__lvgl COMPONENT_DIR)
idf_build_set_property(COMPILE_INCLUDE_DIRECTORIES "${lvgl_dir}" APPEND)
idf_build_set_property(COMPILE_INCLUDE_DIRECTORIES "${lvgl_dir}/src" APPEND)
idf_build_set_property(COMPILE_INCLUDE_DIRECTORIES "${lvgl_dir}/.." APPEND)

# Q-Tune SDK C++ shared-object builder. The stock elf_loader project_so() only
# globs *.c and compiles with the C compiler; Q-Tune plugins are C++ (the
# interface headers pull in tuner_math.h), so we use qtune_project_so() instead.
# Output: build/example_tuner.so
include(${QTUNE_SDK_DIR}/cmake/qtune_plugin.cmake)
qtune_project_so(example_tuner)

# Optional offline validation. After `idf.py build`, run:
#   cmake --build build --target validate
# to check the .so the same way the firmware loader will (requires python3 +
# pyelftools: pip install pyelftools).
add_custom_target(validate
    COMMAND python3 ${QTUNE_SDK_DIR}/tools/validate_plugin.py
            ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.so
    USES_TERMINAL)
add_dependencies(validate so)
