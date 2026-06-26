# Hyperdrive — Q-Tune standby plugin
# Generated from the Q-Tune SDK template by tools/new_plugin.py.

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hyperdrive)

# Path to the q-tune-sdk repo root. This bundled sample lives at
# <repo>/plugins/standby/hyperdrive, so the repo root is three levels up. If you
# copy this project OUT of the SDK repo, change THIS ONE LINE to an absolute path,
# e.g. set(QTUNE_SDK_DIR "$ENV{HOME}/src/q-tune-sdk").
if(NOT DEFINED QTUNE_SDK_DIR)
    set(QTUNE_SDK_DIR "${CMAKE_SOURCE_DIR}/../../..")
endif()

# Publish the SDK + LVGL headers into the global build properties so the
# standalone .so compile (qtune_project_so) can find them.
idf_build_set_property(COMPILE_INCLUDE_DIRECTORIES
    "${QTUNE_SDK_DIR}/include" APPEND)

idf_component_get_property(lvgl_dir lvgl__lvgl COMPONENT_DIR)
idf_build_set_property(COMPILE_INCLUDE_DIRECTORIES "${lvgl_dir}" APPEND)
idf_build_set_property(COMPILE_INCLUDE_DIRECTORIES "${lvgl_dir}/src" APPEND)
idf_build_set_property(COMPILE_INCLUDE_DIRECTORIES "${lvgl_dir}/.." APPEND)

# Build the plugin shared object. Output: build/hyperdrive.so
include(${QTUNE_SDK_DIR}/cmake/qtune_plugin.cmake)
qtune_project_so(hyperdrive)

# Offline validation: after `idf.py build`, run `cmake --build build --target validate`.
add_custom_target(validate
    COMMAND python3 ${QTUNE_SDK_DIR}/tools/validate_plugin.py
            ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.so
    USES_TERMINAL)
add_dependencies(validate so)
