# DXVK 2.7.1 (D3D9 + DXVK core only) for Android arm64, built with the NDK (no meson).
# Drop-in for ../dxvk_android (DXVK 1.9.2b): same library name and same exported interface.
cmake_minimum_required(VERSION 3.22)
project(DXVK2Android C CXX)
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
  message(FATAL_ERROR "DXVK2 Android requires arm64-v8a")
endif()
set(DXVK2_SOURCE "" CACHE PATH "Source prepared by prepare.py (+ shaders.py)")
if(NOT EXISTS "${DXVK2_SOURCE}/PINNED_REVISION")
  message(FATAL_ERROR "Run prepare.py with the pinned DXVK v2.7.1 checkout")
endif()
set(DXVK2_PREPARE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${DXVK2_PREPARE_SCRIPT}" "${DXVK2_SOURCE}/ANDROID_PREPARE_SHA256")
file(SHA256 "${DXVK2_PREPARE_SCRIPT}" DXVK2_EXPECTED_PREPARE_SHA256)
file(READ "${DXVK2_SOURCE}/ANDROID_PREPARE_SHA256" DXVK2_ACTUAL_PREPARE_SHA256)
string(STRIP "${DXVK2_ACTUAL_PREPARE_SHA256}" DXVK2_ACTUAL_PREPARE_SHA256)
if(NOT DXVK2_ACTUAL_PREPARE_SHA256 STREQUAL DXVK2_EXPECTED_PREPARE_SHA256)
  message(FATAL_ERROR "DXVK2 patches are stale: rerun ${DXVK2_PREPARE_SCRIPT}")
endif()
if(NOT EXISTS "${DXVK2_SOURCE}/generated/dxvk_present_frag.h")
  message(FATAL_ERROR "Run shaders.py ${DXVK2_SOURCE}")
endif()

set(S "${DXVK2_SOURCE}/src")
file(GLOB DXVK2_SOURCES
  "${S}/util/*.cpp" "${S}/util/com/*.cpp" "${S}/util/config/*.cpp" "${S}/util/log/*.cpp"
  "${S}/util/sha1/*.c" "${S}/util/sha1/*.cpp" "${S}/util/sync/*.cpp"
  "${S}/spirv/*.cpp" "${S}/vulkan/*.cpp" "${S}/dxvk/*.cpp" "${S}/dxvk/hud/*.cpp"
  "${S}/dxso/*.cpp" "${S}/d3d9/*.cpp")
list(FILTER DXVK2_SOURCES EXCLUDE REGEX "dxvk_open(vr|xr)\\.cpp$")
# WSI: the platform dispatcher, the (stubbed) EDID parser and our Android driver.
list(APPEND DXVK2_SOURCES "${S}/wsi/wsi_platform.cpp" "${S}/wsi/wsi_edid.cpp" android_wsi.cpp)

add_library(dxvk_android SHARED ${DXVK2_SOURCES})
target_compile_features(dxvk_android PRIVATE cxx_std_17)
target_compile_definitions(dxvk_android PRIVATE DXVK_WSI_ANDROID VK_USE_PLATFORM_ANDROID_KHR NOMINMAX)
target_include_directories(dxvk_android PRIVATE "${DXVK2_SOURCE}" "${DXVK2_SOURCE}/generated"
  "${DXVK2_SOURCE}/include" "${DXVK2_SOURCE}/include/vulkan/include" "${DXVK2_SOURCE}/include/spirv/include"
  "${DXVK2_SOURCE}/include/native" "${DXVK2_SOURCE}/include/native/windows" "${DXVK2_SOURCE}/include/native/directx"
  # meson adds each library's own directory (implicit_include_directories): "../util/x.h" relies on it.
  "${S}/util" "${S}/spirv" "${S}/vulkan" "${S}/dxvk" "${S}/dxso" "${S}/d3d9" "${S}/wsi")
target_compile_options(dxvk_android PRIVATE -O2 -Wno-deprecated-declarations -Wno-unused-function
  -Wno-missing-field-initializers -Wno-unused-parameter -Wno-unused-private-field -Wno-extern-c-compat
  -Wno-unused-const-variable -Wno-missing-braces -Wno-microsoft-exception-spec -Wno-nontrivial-memcall
  -Wno-vla-cxx-extension)

# libmgrpc.so was linked against the 1.9.2b library and resolves part of libc++ from it
# (static libc++, default visibility): force the same members in so they stay exported.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/runtime_exports.txt" DXVK2_RUNTIME_EXPORTS REGEX "^[^#]")
foreach(symbol IN LISTS DXVK2_RUNTIME_EXPORTS)
  target_link_options(dxvk_android PRIVATE "-Wl,-u,${symbol}")
endforeach()
target_link_options(dxvk_android PRIVATE -Wl,--build-id)
target_link_libraries(dxvk_android PRIVATE vulkan android log dl)
