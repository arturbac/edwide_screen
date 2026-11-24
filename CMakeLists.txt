# edwide_screen: a plugin for edloader's list (not a d3d11 proxy) that lays the side parts of the game's picture over
# side monitors set at an angle (README). Built on Linux with clang-cl against MSVC's and the Windows SDK's headers
# and libraries (edplugin_common/cmake/clang-cl-msvc-sdk.cmake, MSVC_ROOT); the shader compiled by the SDK's fxc.exe
# under wine.
cmake_minimum_required(VERSION 3.29)

# the shared library edplugin_common, a clone in this directory on the commit edplugin_common.commit pins: the
# toolchain is in it, so it is looked for before project()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/edplugin_common/CMakeLists.txt")
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/edplugin_common.commit" edplugin_common_pinned LIMIT_COUNT 1)
  string(STRIP "${edplugin_common_pinned}" edplugin_common_pinned)
  message(FATAL_ERROR "no clone of edplugin_common in ${CMAKE_CURRENT_SOURCE_DIR}; in that directory:\n"
                      "  git clone https://github.com/arturbac/edplugin_common.git edplugin_common\n"
                      "  git -C edplugin_common checkout ${edplugin_common_pinned}")
endif()

project(edwide_screen LANGUAGES CXX)

# clang-cl, or clang's GNU-style driver targeting the MSVC ABI (the kdev-* presets, clang-msvc-sdk.cmake: an IDE's
# code model)
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   OR NOT (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC" OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC"))
  message(FATAL_ERROR "edwide_screen builds with clang-cl (or clang for the MSVC ABI): configure with a preset or -DCMAKE_TOOLCHAIN_FILE=edplugin_common/cmake/clang-cl-msvc-sdk.cmake")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)

# edplugin_common: the code and the build steps the plugins share, with CPM and stralgo (small_vectors under it),
# pinned there. CPM_SOURCE_CACHE (a filled cache) and FETCHCONTENT_FULLY_DISCONNECTED=ON configure without the network
add_subdirectory(edplugin_common)

set(gen "${CMAKE_BINARY_DIR}/gen")
file(MAKE_DIRECTORY "${gen}")

edplugin_common_fxc("${gen}/wings_vs.h" vs_5_0 vs_main g_wings_vs "${CMAKE_SOURCE_DIR}/shaders/wings.hlsl" /O3)
edplugin_common_fxc("${gen}/wings_ps.h" ps_5_0 ps_main g_wings_ps "${CMAKE_SOURCE_DIR}/shaders/wings.hlsl" /O3)

add_custom_target(edwide_generated DEPENDS "${gen}/wings_vs.h" "${gen}/wings_ps.h")

add_library(edwide_screen SHARED src/entry.cc src/settings_log.cc src/correction.cc src/hooks.cc)
target_include_directories(edwide_screen PRIVATE "${gen}")
target_link_libraries(edwide_screen PRIVATE edplugin_common::edplugin_common kernel32 user32 d3d11 dxgi dxguid)
edplugin_common_warnings(edwide_screen)
# a chain element of edloader's: its two calls, passed on to the next d3d11 and the factory hooked on the device made
target_link_options(edwide_screen PRIVATE LINKER:/DEBUG:FULL $<$<NOT:$<CONFIG:Debug>>:LINKER:/OPT:REF,/OPT:ICF>
                    LINKER:/EXPORT:D3D11CreateDevice=edwide_create_device LINKER:/EXPORT:D3D11CreateDeviceAndSwapChain=edwide_create_device_and_chain)
set_target_properties(edwide_screen PROPERTIES PREFIX "")
add_dependencies(edwide_screen edwide_generated)
edplugin_common_version(EDWIDE_VERSION edwide_version.h edwide_screen)
edplugin_common_pin(edwide_screen)

option(EDWIDE_TESTS "build the test program (tools/test.sh runs it under wine)" ON)
if(EDWIDE_TESTS)
  add_executable(edwide_test test/test_app.cc)
  target_include_directories(edwide_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
  target_link_libraries(edwide_test PRIVATE d3d11 dxgi dxguid user32 windowscodecs ole32)
  target_compile_definitions(edwide_test PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
  edplugin_common_warnings(edwide_test)

  # the log and the ini made by the plugin with no device and no drawing (tools/doc.sh)
  add_executable(edwide_doc test/doc_app.cc)
  target_compile_definitions(edwide_doc PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  edplugin_common_warnings(edwide_doc)
endif()
