# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Boyd Timothy

# main/CMakeLists.txt — example_tuner plugin component
#
# Only stub_app.c is registered (it provides the app_main() the normal IDF app
# image needs). The real plugin source, example_tuner.cpp, is intentionally NOT
# listed here: it references host firmware symbols (qt_get_*, screen_width, …)
# that are undefined in a standalone app link. qtune_project_so() in the
# top-level CMakeLists.txt globs this directory and compiles example_tuner.cpp
# into example_tuner.so, where undefined host symbols are allowed (resolved by
# the loader at runtime).
#
# INCLUDE_DIRS / REQUIRES here still apply to the .so compile because
# qtune_project_so() uses the aggregate COMPILE_INCLUDE_DIRECTORIES.

# q-tune-sdk repo root (component scope does not inherit the root CMakeLists'
# variable, so recompute it here). CMAKE_SOURCE_DIR is the top-level project dir.
if(NOT DEFINED QTUNE_SDK_DIR)
    set(QTUNE_SDK_DIR "${CMAKE_SOURCE_DIR}/../..")  # default: SDK repo root
endif()

idf_component_register(
    SRCS
        "stub_app.c"
    INCLUDE_DIRS
        "."
        # SDK headers: TunerGUIInterface, TunerNoteName, qt_get_*,
        # QTunePluginDescriptor, and the trimmed defines.h.
        "${QTUNE_SDK_DIR}/include"
    REQUIRES
        lvgl
        elf_loader
)
