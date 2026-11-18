# edwide_screen: a plugin for edloader's list (not a d3d11 proxy) that lays the side parts of the game's picture over
# side monitors set at an angle (README). Built on Linux with clang-cl against MSVC's and the Windows SDK's headers
# and libraries (cmake/clang-cl-msvc-sdk.cmake, MSVC_ROOT); the shader compiled by the SDK's fxc.exe under wine.
cmake_minimum_required(VERSION 3.29)
project(edwide_screen LANGUAGES CXX)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  message(FATAL_ERROR "edwide_screen builds with clang-cl: configure with a preset or -DCMAKE_TOOLCHAIN_FILE=cmake/clang-cl-msvc-sdk.cmake")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)

find_program(EDWIDE_WINE wine REQUIRED)
file(GLOB fxc_found "${MSVC_ROOT}/Windows Kits/10/bin/*/x64/fxc.exe")
list(SORT fxc_found)
list(POP_BACK fxc_found fxc_newest)
set(EDWIDE_FXC "${fxc_newest}" CACHE FILEPATH "fxc.exe of a Windows SDK, run under wine")
if(NOT EXISTS "${EDWIDE_FXC}")
  message(FATAL_ERROR "EDWIDE_FXC='${EDWIDE_FXC}': no fxc.exe (looked in ${MSVC_ROOT}/Windows Kits/10/bin/*/x64)")
endif()

set(gen "${CMAKE_BINARY_DIR}/gen")
file(MAKE_DIRECTORY "${gen}")

# fxc is a Windows program: options start with '/', so the paths it gets are relative to the build directory
# (outputs) or Windows paths through wine's Z: drive (inputs)
function(edwide_fxc out profile entry var source)
  cmake_path(RELATIVE_PATH out BASE_DIRECTORY "${CMAKE_BINARY_DIR}" OUTPUT_VARIABLE out_rel)
  add_custom_command(
    OUTPUT "${out}"
    COMMAND ${CMAKE_COMMAND} -E env WINEDEBUG=-all
            "${EDWIDE_WINE}" "${EDWIDE_FXC}" /nologo ${ARGN} /T ${profile} /E ${entry} /Vn ${var}
            /Fh "${out_rel}" "Z:${source}"
    DEPENDS "${source}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "fxc ${profile} ${var}"
    VERBATIM)
endfunction()

edwide_fxc("${gen}/wings_vs.h" vs_5_0 vs_main g_wings_vs "${CMAKE_SOURCE_DIR}/shaders/wings.hlsl" /O3)
edwide_fxc("${gen}/wings_ps.h" ps_5_0 ps_main g_wings_ps "${CMAKE_SOURCE_DIR}/shaders/wings.hlsl" /O3)

add_custom_target(edwide_version
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DOUT=${gen}/edwide_version.h -DNAME=EDWIDE_VERSION
          -P ${CMAKE_SOURCE_DIR}/cmake/version.cmake
  BYPRODUCTS "${gen}/edwide_version.h"
  COMMENT "edwide_screen version")
add_custom_target(edwide_generated DEPENDS "${gen}/wings_vs.h" "${gen}/wings_ps.h")

add_library(edwide_screen SHARED src/plugin.cc)
target_include_directories(edwide_screen PRIVATE "${gen}")
target_link_libraries(edwide_screen PRIVATE kernel32 user32 d3d11 dxgi dxguid)
target_compile_definitions(edwide_screen PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(edwide_screen PRIVATE /W4 "/FI${gen}/edwide_version.h")
target_link_options(edwide_screen PRIVATE /DEBUG:FULL $<$<NOT:$<CONFIG:Debug>>:/OPT:REF /OPT:ICF>)
set_target_properties(edwide_screen PROPERTIES PREFIX "")
add_dependencies(edwide_screen edwide_version edwide_generated)

option(EDWIDE_TESTS "build the test program (tools/test.sh runs it under wine)" ON)
if(EDWIDE_TESTS)
  add_executable(edwide_test test/test_app.cc)
  target_include_directories(edwide_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
  target_link_libraries(edwide_test PRIVATE d3d11 dxgi dxguid user32 windowscodecs ole32)
  target_compile_definitions(edwide_test PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
  target_compile_options(edwide_test PRIVATE /W4)
endif()
