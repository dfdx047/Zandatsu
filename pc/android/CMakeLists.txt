# MGR:R PC port, Android arm64 lab build. Structure from codepdbh/nfsu2-recompiled android/app/src/main/cpp/CMakeLists.txt (MIT).
#  libmgrlab.so  - NativeActivity loader, the only native code inside the APK (no game code: distribution rule)
#  libmgrpc.so   - runtime + lifted game code + D3D9 bridge; pushed to the app's private files dir, never in the APK
#  libdxvk_android.so - dxvk-native 1.9.2b (codepdbh recipe)
cmake_minimum_required(VERSION 3.22)
project(MgrPcAndroid C CXX)
set(CMAKE_CXX_STANDARD 20)
set(RT "${CMAKE_CURRENT_SOURCE_DIR}/../runtime")
set(PORT "${CMAKE_CURRENT_SOURCE_DIR}/../ports")
set(GEN "${CMAKE_CURRENT_SOURCE_DIR}/../../build/pc/gen" CACHE PATH "lifted C")
set(PCRECOMP "${CMAKE_CURRENT_SOURCE_DIR}/../../build/lab/pc/pcrecomp-pinned/runtime" CACHE PATH "pcrecomp runtime")
set(DXVK_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../build/lab/pc/dxvk-source" CACHE PATH "Prepared DXVK native source")
set(GUEST_OPT "-O1" CACHE STRING "optimisation of the lifted code")
set(MGR_LAB_INCLUDE "" CACHE FILEPATH "untracked lab include for guest_mgr.inc")

add_library(native_app_glue STATIC "${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
target_include_directories(native_app_glue PUBLIC "${ANDROID_NDK}/sources/android/native_app_glue")

add_subdirectory("${PORT}/dxvk_android" "${CMAKE_CURRENT_BINARY_DIR}/dxvk")

file(GLOB GEN_SOURCES "${GEN}/recomp_*.c")
add_library(mgr_guest OBJECT ${GEN_SOURCES})
target_include_directories(mgr_guest PRIVATE "${PCRECOMP}/recomp32" "${GEN}" "${RT}")
target_compile_definitions(mgr_guest PRIVATE RECOMP_STRICT_ICALL)
target_compile_options(mgr_guest PRIVATE ${GUEST_OPT} -w -fwrapv -fno-strict-aliasing -ffp-contract=off -fexceptions
  -include "${RT}/arm64_guest.h")
set_target_properties(mgr_guest PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(mgrpc SHARED mgr_android_main.cpp "${RT}/guest_runtime.cpp" "${RT}/d3d9_bridge.cpp" "${RT}/overrides.c"
  $<TARGET_OBJECTS:mgr_guest>)
target_include_directories(mgrpc PRIVATE "${RT}" "${PCRECOMP}/recomp32" "${PCRECOMP}/win32hle" "${GEN}"
  "${DXVK_SOURCE}/include" "${DXVK_SOURCE}/include/native/windows" "${DXVK_SOURCE}/include/native/directx")
target_compile_definitions(mgrpc PRIVATE RECOMP_STRICT_ICALL NFS_D3D9_BACKEND)
if(MGR_LAB_INCLUDE)
  target_compile_definitions(mgrpc PRIVATE "MGR_LAB_INCLUDE=\"${MGR_LAB_INCLUDE}\"")
endif()
target_compile_options(mgrpc PRIVATE -O2 -fno-strict-aliasing -ffp-contract=off -w)
target_include_directories(mgrpc PRIVATE "${ANDROID_NDK}/sources/android/native_app_glue")
target_link_libraries(mgrpc PRIVATE android log vulkan dxvk_android aaudio dl)
target_link_options(mgrpc PRIVATE "-Wl,-z,max-page-size=16384")

# libadrenotools (bylaws, BSD-2): optional custom Vulkan driver (Turnip) chosen in <files>/mgr_driver.txt.
# Same pinned revision as app/src/main/cpp/CMakeLists.txt; -DFETCHCONTENT_SOURCE_DIR_ADRENOTOOLS=<src> avoids the download.
include(FetchContent)
if(NOT CMAKE_ANDROID_ARCH_ABI)
  set(CMAKE_ANDROID_ARCH_ABI "${ANDROID_ABI}")
endif()
FetchContent_Declare(adrenotools GIT_REPOSITORY https://github.com/bylaws/libadrenotools.git
  GIT_TAG 8fae8ce254dfc1344527e05301e43f37dea2df80 GIT_SUBMODULES_RECURSE TRUE)
FetchContent_MakeAvailable(adrenotools)

add_library(mgrlab SHARED mgr_loader.cpp)
target_link_libraries(mgrlab PRIVATE native_app_glue android log dl adrenotools)
target_link_options(mgrlab PRIVATE "-Wl,--exclude-libs,libadrenotools.a:liblinkernsbypass.a")
target_link_options(mgrlab PRIVATE "-uANativeActivity_onCreate" "-Wl,-z,max-page-size=16384")
