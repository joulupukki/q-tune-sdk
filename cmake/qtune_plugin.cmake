# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Boyd Timothy

# qtune_plugin.cmake — C++ shared-object builder for Q-Tune UI plugins.
#
# The stock elf_loader project_so() macro only globs *.c and compiles with the C
# compiler. Q-Tune's plugin interface headers (tuner_ui_interface.h ->
# defines.h -> tuner_math.h) are C++, so plugins must be compiled as C++.
#
# qtune_project_so(<name>) mirrors project_so() but:
#   - globs *.cpp/*.cc/*.cxx (and *.c) from the "main" component (+ ELF_COMPONENTS),
#   - compiles them with the C++ compiler, -fno-exceptions -fno-rtti (no C++ runtime
#     support is available in the loaded module; the descriptor must be a POD with
#     static initialization and the plugin must avoid exceptions/RTTI),
#   - links into <name>.so with -Wl,--allow-shlib-undefined so host symbols
#     (lv_*, qt_get_*, screen_width, fonts, libc/math) are resolved at load time,
#   - strips the result.
#
# Call AFTER project(). Requires CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT=y.
# Output: build/<name>.so

macro(qtune_project_so project_name)
    if(CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT)
        set(so_compile_flags -c
                             -fPIC
                             -fno-exceptions
                             -fno-rtti
                             -fno-use-cxa-atexit
                             -DCONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT)

        set(so_link_flags -shared
                          -fPIC
                          -static-libgcc
                          -nostdlib
                          -nostartfiles
                          -fdata-sections
                          -ffunction-sections
                          -Wl,--gc-sections
                          -fvisibility=hidden
                          -Wl,--strip-all
                          -Wl,--strip-debug
                          -Wl,--strip-discarded)

        set(so_output "${CMAKE_PROJECT_NAME}.so")

        # Derive the strip tool from the C++ compiler path.
        string(REPLACE "-elf-g++" "-elf-strip" CMAKE_STRIP_SO ${CMAKE_CXX_COMPILER})
        set(so_strip_flags --strip-unneeded
                           --remove-section=.comment
                           --remove-section=.got.loc
                           --remove-section=.dynamic)
        if(CONFIG_IDF_TARGET_ARCH_XTENSA)
            list(APPEND so_strip_flags --remove-section=.xt.lit
                                       --remove-section=.xt.prop
                                       --remove-section=.xtensa.info)
        elseif(CONFIG_IDF_TARGET_ARCH_RISCV)
            list(APPEND so_strip_flags --remove-section=.riscv.attributes)
        endif()

        idf_build_get_property(build_dir BUILD_DIR)

        # Collect sources from "main" plus any extra ELF_COMPONENTS.
        list(PREPEND ELF_COMPONENTS "main")
        set(so_sources "")
        set(so_dependencies "")
        set(so_obj_files "")

        foreach(c ${ELF_COMPONENTS})
            idf_component_get_property(component_dir ${c} COMPONENT_DIR)
            file(GLOB_RECURSE component_sources CONFIGURE_DEPENDS
                 "${component_dir}/*.cpp" "${component_dir}/*.cc"
                 "${component_dir}/*.cxx" "${component_dir}/*.c")
            foreach(src ${component_sources})
                if(NOT src MATCHES "/(test|tests|testing)/" AND
                   NOT src MATCHES "/test_[^/]*\\.(c|cpp)$" AND
                   NOT src MATCHES "${build_dir}")
                    list(APPEND so_sources ${src})
                endif()
            endforeach()
            if(${CMAKE_GENERATOR} STREQUAL "Unix Makefiles")
                add_custom_command(OUTPUT so_${c}_app
                    COMMAND +${CMAKE_MAKE_PROGRAM} "__idf_${c}/fast"
                    COMMENT "Build Component: ${c}")
                list(APPEND so_dependencies "so_${c}_app")
            else()
                list(APPEND so_dependencies "idf::${c}")
            endif()
        endforeach()

        # ESP-IDF compile definitions.
        idf_build_get_property(compile_defs COMPILE_DEFINITIONS)
        set(def_flags "")
        foreach(def ${compile_defs})
            list(APPEND def_flags "-D${def}")
        endforeach()

        # Include dirs: COMPILE_INCLUDE_DIRECTORIES holds only globally-appended
        # entries, NOT the per-component public includes (esp_common/esp_attr.h,
        # esp_log, etc.). Enumerate every build component's public INCLUDE_DIRS so
        # the plugin can include lvgl.h and its transitive ESP-IDF headers.
        set(include_flags "")
        idf_build_get_property(extra_incs COMPILE_INCLUDE_DIRECTORIES)
        foreach(inc_dir ${extra_incs})
            list(APPEND include_flags "-I${inc_dir}")
        endforeach()
        idf_build_get_property(all_components BUILD_COMPONENTS)
        foreach(comp ${all_components})
            idf_component_get_property(comp_dir ${comp} COMPONENT_DIR)
            idf_component_get_property(comp_incs ${comp} INCLUDE_DIRS)
            foreach(inc ${comp_incs})
                list(APPEND include_flags "-I${comp_dir}/${inc}")
            endforeach()
        endforeach()
        # Generated sdkconfig.h lives in the build config dir (lvgl's
        # lv_conf_kconfig.h includes it).
        list(APPEND include_flags "-I${build_dir}/config")

        # Compile each source to a PIC .o with the C++ compiler.
        set(so_obj_dir "${CMAKE_BINARY_DIR}/so_objs")
        foreach(src_file ${so_sources})
            file(RELATIVE_PATH src_rel ${CMAKE_SOURCE_DIR} ${src_file})
            string(REGEX REPLACE "[/\\.]" "_" obj_name ${src_rel})
            set(obj_file "${so_obj_dir}/${obj_name}.o")
            list(APPEND so_obj_files ${obj_file})
            add_custom_command(OUTPUT ${obj_file}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${so_obj_dir}
                COMMAND ${CMAKE_CXX_COMPILER} ${so_compile_flags} ${def_flags} ${include_flags} ${src_file} -o ${obj_file}
                DEPENDS ${src_file}
                COMMENT "Compile ${src_rel}")
        endforeach()

        add_custom_command(OUTPUT ${so_output}
            COMMAND ${CMAKE_CXX_COMPILER} ${so_link_flags} -o ${so_output} ${so_obj_files} -Wl,--allow-shlib-undefined
            COMMAND ${CMAKE_STRIP_SO} ${so_strip_flags} ${so_output}
            COMMAND ${CMAKE_COMMAND} -E echo "Built shared object: ${so_output}"
            DEPENDS ${so_dependencies} ${so_obj_files}
            COMMENT "Link Shared Object: ${so_output}"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        add_custom_target(so ALL DEPENDS ${so_output})
    endif()
endmacro()
