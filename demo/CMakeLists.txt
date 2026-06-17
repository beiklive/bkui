add_executable(bkui_demo src/main.cpp)
add_executable(bkui_perf_demo src/perf_demo.cpp)
add_executable(bkui_ui_tree_demo src/ui_tree_demo.cpp)

foreach(target bkui_demo bkui_perf_demo bkui_ui_tree_demo)
    target_link_libraries(${target} PRIVATE BeikUI::bkui)
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(
        ${target}
        PRIVATE
            "${CMAKE_SOURCE_DIR}/bkui/third_party/platform/stb"
    )
endforeach()

set_target_properties(
    bkui_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${BKUI_RUNTIME_OUTPUT_DIR}"
)
set_target_properties(
    bkui_perf_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${BKUI_RUNTIME_OUTPUT_DIR}"
)
set_target_properties(
    bkui_ui_tree_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${BKUI_RUNTIME_OUTPUT_DIR}"
)

if(BKUI_PLATFORM_SWITCH)
    set(BKUI_SWITCH_ROMFS_DIR "${CMAKE_BINARY_DIR}/resources")
    set(BKUI_SWITCH_SHADER_DIR "${BKUI_SWITCH_ROMFS_DIR}/shaders")
    set(BKUI_SWITCH_NACP "${BKUI_RUNTIME_OUTPUT_DIR}/bkui_demo.nacp")
    set(BKUI_SWITCH_NRO "${BKUI_RUNTIME_OUTPUT_DIR}/bkui_demo.nro")

    if(NOT NX_UAM_EXE)
        find_program(
            NX_UAM_EXE
            uam
            HINTS "$ENV{DEVKITPRO}/tools/bin"
        )
    endif()

    if(NOT NX_UAM_EXE)
        message(FATAL_ERROR "uam was not found; install devkitPro uam to build Deko3D shaders.")
    endif()

    if(NOT NX_ELF2NRO_EXE)
        find_program(
            NX_ELF2NRO_EXE
            elf2nro
            HINTS "$ENV{DEVKITPRO}/tools/bin"
        )
    endif()

    if(NOT NX_ELF2NRO_EXE)
        message(FATAL_ERROR "elf2nro was not found; install devkitPro tools to package the Switch NRO.")
    endif()

    if(NOT NX_NACPTOOL_EXE)
        find_program(
            NX_NACPTOOL_EXE
            nacptool
            HINTS "$ENV{DEVKITPRO}/tools/bin"
        )
    endif()

    add_custom_target(
        bkui_demo.nro
        ALL
        DEPENDS bkui_demo bkui_perf_demo bkui_ui_tree_demo
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "${BKUI_SWITCH_ROMFS_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/resources" "${BKUI_SWITCH_ROMFS_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "${BKUI_SWITCH_ROMFS_DIR}/font"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${BKUI_SWITCH_SHADER_DIR}"
        COMMAND "${NX_UAM_EXE}" -s vert -o "${BKUI_SWITCH_SHADER_DIR}/deko_triangle_vsh.dksh" "${CMAKE_SOURCE_DIR}/resources/shaders/deko3d/triangle_vsh.glsl"
        COMMAND "${NX_UAM_EXE}" -s frag -o "${BKUI_SWITCH_SHADER_DIR}/deko_triangle_fsh.dksh" "${CMAKE_SOURCE_DIR}/resources/shaders/deko3d/triangle_fsh.glsl"
        COMMAND "${NX_UAM_EXE}" -s vert -o "${BKUI_SWITCH_SHADER_DIR}/deko_ui_vsh.dksh" "${CMAKE_SOURCE_DIR}/resources/shaders/deko3d/ui_vsh.glsl"
        COMMAND "${NX_UAM_EXE}" -s frag -o "${BKUI_SWITCH_SHADER_DIR}/deko_ui_fsh.dksh" "${CMAKE_SOURCE_DIR}/resources/shaders/deko3d/ui_fsh.glsl"
        COMMAND "$<$<BOOL:${NX_NACPTOOL_EXE}>:${NX_NACPTOOL_EXE}>" "$<$<BOOL:${NX_NACPTOOL_EXE}>:--create>" "$<$<BOOL:${NX_NACPTOOL_EXE}>:BeikUI Resource Demo>" "$<$<BOOL:${NX_NACPTOOL_EXE}>:beiklive>" "$<$<BOOL:${NX_NACPTOOL_EXE}>:1.0.0>" "$<$<BOOL:${NX_NACPTOOL_EXE}>:${BKUI_SWITCH_NACP}>"
        COMMAND "${NX_ELF2NRO_EXE}" "$<TARGET_FILE:bkui_demo>" "${BKUI_SWITCH_NRO}" "$<$<BOOL:${NX_NACPTOOL_EXE}>:--nacp=${BKUI_SWITCH_NACP}>" "--romfsdir=${BKUI_SWITCH_ROMFS_DIR}"
        BYPRODUCTS "${BKUI_SWITCH_NRO}" "${BKUI_SWITCH_NACP}"
        COMMENT "Packaging bkui_demo.nro with Switch romfs resources"
        VERBATIM
    )
else()
    file(
        GLOB_RECURSE
        BKUI_DEMO_RESOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/resources/*"
    )

    add_custom_target(
        bkui_demo.resources
        ALL
        DEPENDS ${BKUI_DEMO_RESOURCES}
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "$<TARGET_FILE_DIR:bkui_demo>/resources"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/resources" "$<TARGET_FILE_DIR:bkui_demo>/resources"
        COMMENT "Copying demo resources next to the executable"
        VERBATIM
    )

    add_dependencies(bkui_demo bkui_demo.resources)
    add_dependencies(bkui_perf_demo bkui_demo.resources)
    add_dependencies(bkui_ui_tree_demo bkui_demo.resources)
endif()
