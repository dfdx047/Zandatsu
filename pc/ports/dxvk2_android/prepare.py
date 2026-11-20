"""Prepare the pinned zlib-licensed DXVK 2.7.1 (D3D9 only) for Android ARM64.

Copies the sources of a v2.7.1 checkout (with its include/ submodules) to OUTPUT,
applies the Android changes below, writes version.h/buildenv.h and the recipe hash
CMakeLists.txt checks. Same approach as ../dxvk_android/prepare.py (DXVK 1.9.2b).
"""
import argparse
import hashlib
import pathlib
import shutil
import subprocess

PIN = "c3dd74be6baec53786d4e064a572185b70347a17"  # tag v2.7.1
parser = argparse.ArgumentParser()
parser.add_argument("source", type=pathlib.Path)
parser.add_argument("output", type=pathlib.Path)
args = parser.parse_args()
revision = subprocess.check_output(["git", "-C", str(args.source), "rev-parse", "HEAD"], text=True).strip()
if revision != PIN:
    raise SystemExit("Unexpected DXVK revision: " + revision)
for header in ("include/vulkan/include/vulkan/vulkan.h", "include/spirv/include/spirv/unified1/spirv.hpp",
               "include/native/directx/d3d9.h"):
    if not (args.source / header).exists():
        raise SystemExit("Missing submodule header (git submodule update --init): " + header)
if args.output.exists():
    shutil.rmtree(args.output / "src", ignore_errors=True)
args.output.mkdir(parents=True, exist_ok=True)
ignore = shutil.ignore_patterns(".git")
for directory in ("src", "include"):
    shutil.copytree(args.source / directory, args.output / directory, dirs_exist_ok=True, ignore=ignore)
shutil.copy2(args.source / "LICENSE", args.output / "LICENSE")

def replace(path, old, new):
    file = args.output / path
    text = file.read_text()
    if old not in text:
        raise SystemExit("Patch mismatch: " + path)
    file.write_text(text.replace(old, new, 1))

# --- WSI: an "Android" driver (android_wsi.cpp), the default when DXVK_WSI_DRIVER is unset.
replace("src/wsi/wsi_platform.h", "#if defined(DXVK_WSI_GLFW)\n  extern WsiBootstrap GlfwWSI;\n#endif\n",
        "#if defined(DXVK_WSI_GLFW)\n  extern WsiBootstrap GlfwWSI;\n#endif\n#if defined(DXVK_WSI_ANDROID)\n  extern WsiBootstrap AndroidWSI;\n#endif\n")
replace("src/wsi/wsi_platform.cpp", "#if defined(DXVK_WSI_GLFW)\n    &GlfwWSI,\n#endif\n",
        "#if defined(DXVK_WSI_GLFW)\n    &GlfwWSI,\n#endif\n#if defined(DXVK_WSI_ANDROID)\n    &AndroidWSI,\n#endif\n")
replace("src/wsi/wsi_platform.cpp", "#if defined(DXVK_WSI_WIN32)\n        hint = \"Win32\";\n#else",
        "#if defined(DXVK_WSI_WIN32)\n        hint = \"Win32\";\n#elif defined(DXVK_WSI_ANDROID)\n        hint = \"Android\";\n#else")

# --- Vulkan: the driver chosen in the launcher (system loader or adrenotools), see android_wsi.cpp.
replace("src/vulkan/vulkan_loader.cpp", "  LibraryLoader::LibraryLoader() {\n    std::tie(m_library, m_getInstanceProcAddr) = loadVulkanLibrary();\n  }",
        "  LibraryLoader::LibraryLoader() {\n    // Android: the driver chosen in the launcher (system or adrenotools), see android_wsi.cpp.\n"
        "    m_library = nullptr;\n    m_getInstanceProcAddr = dxvkAndroidGetInstanceProcAddr();\n  }")
replace("src/vulkan/vulkan_loader.cpp", "namespace dxvk::vk {\n",
        "extern \"C\" PFN_vkGetInstanceProcAddr dxvkAndroidGetInstanceProcAddr();\n\nnamespace dxvk::vk {\n")

# --- Presenter: Android reports VK_SUBOPTIMAL_KHR for every landscape present (the
# image is not pre-rotated; the compositor rotates it). 2.7.1 recreates the swap chain
# on any status other than VK_SUCCESS: keep using a suboptimal swap chain.
replace("src/dxvk/dxvk_presenter.cpp", "    if (m_acquireStatus != VK_SUCCESS && m_swapchain)\n",
        "    if (m_acquireStatus == VK_SUBOPTIMAL_KHR)\n      m_acquireStatus = VK_SUCCESS;\n\n    if (m_acquireStatus != VK_SUCCESS && m_swapchain)\n")
replace("src/dxvk/dxvk_presenter.cpp", "    if (status == VK_SUCCESS) {\n      PresenterSync& nextSync",
        "    if (status == VK_SUCCESS || status == VK_SUBOPTIMAL_KHR) {\n      PresenterSync& nextSync")
replace("src/dxvk/dxvk_presenter.cpp", "    std::lock_guard lock(m_surfaceMutex);\n\n    if (status != VK_SUCCESS) {\n",
        "    std::lock_guard lock(m_surfaceMutex);\n\n    if (status == VK_SUBOPTIMAL_KHR)\n      status = VK_SUCCESS;\n\n    if (status != VK_SUCCESS) {\n")
# The surface keeps a reference to its ANativeWindow until DXVK destroys it (android_wsi.cpp).
replace("src/dxvk/dxvk_presenter.cpp", "  void Presenter::destroySurface() {\n    m_vki->vkDestroySurfaceKHR(m_vki->instance(), m_surface, nullptr);\n",
        "  void Presenter::destroySurface() {\n    if (m_surface == VK_NULL_HANDLE)\n      return;\n\n"
        "    m_vki->vkDestroySurfaceKHR(m_vki->instance(), m_surface, nullptr);\n    dxvkAndroidSurfaceDestroyed(m_surface);\n")
replace("src/dxvk/dxvk_presenter.cpp", "namespace dxvk {\n",
        "extern \"C\" void dxvkAndroidSurfaceDestroyed(VkSurfaceKHR surface);\n\nnamespace dxvk {\n")

# --- EDID: no libdisplay-info on Android; the Android WSI returns no EDID anyway.
(args.output / "src/wsi/wsi_edid.cpp").write_text(
    '#include "wsi_edid.h"\n\nnamespace dxvk::wsi {\n\n'
    '  // Android port: built without libdisplay-info, the WSI reports no EDID.\n'
    '  std::optional<WsiDisplayMetadata> parseColorimetryInfo(const WsiEdidData&) {\n'
    '    return std::nullopt;\n  }\n\n}\n')

# --- Log: warnings and errors to logcat (tag DXVK), or to the game log when the runtime provides one.
replace("src/util/log/log.cpp", '#include "log.h"\n',
        '#include "log.h"\n#ifdef __ANDROID__\n#include <android/log.h>\n'
        'extern "C" __attribute__((weak)) int nfsRuntimeLog(int, const char*, const char*, ...);\n#endif\n')
replace("src/util/log/log.cpp", "          std::cerr << adjusted;\n",
        "          std::cerr << adjusted;\n#ifdef __ANDROID__\n"
        "          if (level >= LogLevel::Warn)\n"
        "            (nfsRuntimeLog ? nfsRuntimeLog : __android_log_print)(level >= LogLevel::Error ? ANDROID_LOG_ERROR : ANDROID_LOG_WARN,\n"
        "              \"DXVK\", \"%s\", line.c_str());\n#endif\n")

(args.output / "version.h").write_text('#pragma once\n\n#define DXVK_VERSION "v2.7.1-android-arm64"\n')
(args.output / "buildenv.h").write_text('#pragma once\n\n#define DXVK_TARGET "aarch64"\n#define DXVK_COMPILER "clang"\n'
                                        '#define DXVK_COMPILER_VERSION __clang_version__\n')
(args.output / "PINNED_REVISION").write_text(PIN + "\n")
(args.output / "ANDROID_PREPARE_SHA256").write_text(hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest() + "\n", encoding="ascii")
print("Prepared DXVK " + PIN)
