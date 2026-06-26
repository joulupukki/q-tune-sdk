# main/CMakeLists.txt — Phase plugin component
#
# Only stub_app.c is registered (it provides the app_main() the normal IDF app
# image needs). The real plugin source is compiled into the .so by
# qtune_project_so() in the top-level CMakeLists.txt, which globs this directory.

if(NOT DEFINED QTUNE_SDK_DIR)
    set(QTUNE_SDK_DIR "${CMAKE_SOURCE_DIR}/../../..")
endif()

idf_component_register(
    SRCS
        "stub_app.c"
    INCLUDE_DIRS
        "."
        "${QTUNE_SDK_DIR}/include"
    REQUIRES
        lvgl
        elf_loader
)
