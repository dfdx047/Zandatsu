"""Prepare the pinned zlib-licensed DXVK native 1.9.2b for Android ARM64."""
import argparse
import hashlib
import pathlib
import shutil
import subprocess

PIN = "c8dc91fabd00cac11d697ccf07426e798393cd40"
parser = argparse.ArgumentParser()
parser.add_argument("source", type=pathlib.Path)
parser.add_argument("output", type=pathlib.Path)
args = parser.parse_args()
revision = subprocess.check_output(["git", "-C", str(args.source), "rev-parse", "HEAD"], text=True).strip()
if revision != PIN:
    raise SystemExit("Unexpected DXVK revision: " + revision)
args.output.mkdir(parents=True, exist_ok=True)
for directory in ("src", "include"):
    shutil.copytree(args.source / directory, args.output / directory, dirs_exist_ok=True)
shutil.copy2(args.source / "LICENSE", args.output / "LICENSE")
for header in ("vulkan_android.h", "vulkan_xlib.h"):
    shutil.copy2(pathlib.Path(__file__).parent / "include" / header, args.output / "include/vulkan" / header)

def replace(path, old, new):
    file = args.output / path
    text = file.read_text()
    if old not in text:
        raise SystemExit("Patch mismatch: " + path)
    file.write_text(text.replace(old, new))

replace("src/util/util_bit.h", "#pragma once", "#pragma once\n#include <cstdint>")
replace("src/d3d9/d3d9_device.cpp", "enabled.core.features.textureCompressionBC = VK_TRUE;", "enabled.core.features.textureCompressionBC = supported.core.features.textureCompressionBC;")

replace("src/util/util_bit.h", "#ifndef _MSC_VER", "#if defined(__i386__) || defined(__x86_64__)\n#ifndef _MSC_VER")
replace("src/util/util_bit.h", '#include "util_likely.h"', '#endif // x86 intrinsics\n#include "util_likely.h"')
replace("src/util/util_bit.h", "#elif defined(__GNUC__) || defined(__clang__)\n    uint32_t res;", "#elif defined(__aarch64__) || defined(__arm__)\n    return n ? __builtin_ctz(n) : 32;\n    #elif defined(__GNUC__) || defined(__clang__)\n    uint32_t res;")
replace("src/util/util_bit.h", "#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)", "#if defined(__i386__) || defined(__x86_64__)")
replace("src/util/sync/sync_spinlock.h", "_mm_pause();", '#if defined(__aarch64__) || defined(__arm__)\n        asm volatile("yield");\n#else\n        _mm_pause();\n#endif')
replace("src/util/util_vector.h", "    __m128 value = _mm_load_ps(a.data);", "#if defined(__aarch64__) || defined(__arm__)\n    for (unsigned i = 0; i < 4; ++i) result.data[i] = a.data[i] == a.data[i] ? a.data[i] : 0.0f;\n#else\n    __m128 value = _mm_load_ps(a.data);")
replace("src/util/util_vector.h", "    _mm_store_ps(result.data, value);", "    _mm_store_ps(result.data, value);\n#endif")
# Android reports VK_SUBOPTIMAL_KHR for every landscape present (the image is not
# pre-rotated; the compositor rotates it). Treating that as a failed present recreated
# the swap chain, with a vkDeviceWaitIdle, on every frame.
replace("src/d3d9/d3d9_swapchain.cpp", '    VkResult status = m_device->waitForSubmission(&m_presentStatus);\n\n    if (status != VK_SUCCESS)\n      RecreateSwapChain(m_vsync);', '    VkResult status = m_device->waitForSubmission(&m_presentStatus);\n\n    if (status != VK_SUCCESS && status != VK_SUBOPTIMAL_KHR)\n      RecreateSwapChain(m_vsync);')
# DXVK resolves Vulkan through the driver selected at runtime, not the linked loader.
replace("src/vulkan/vulkan_loader.cpp", '  static const PFN_vkGetInstanceProcAddr GetInstanceProcAddr = vkGetInstanceProcAddr;', '  // Android: the driver chosen in the launcher (system or adrenotools), see android_wsi.cpp.\n  extern "C" PFN_vkGetInstanceProcAddr dxvkAndroidGetInstanceProcAddr();\n  static PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char* name) {\n    return dxvkAndroidGetInstanceProcAddr()(instance, name);\n  }')
# D3D9 sets a single viewport; Adreno 610 (Redmi Note 8) has no multiViewport.
replace("src/d3d9/d3d9_device.cpp", '    enabled.core.features.multiViewport = VK_TRUE;', '    enabled.core.features.multiViewport = supported.core.features.multiViewport;')
# Old Adreno 6xx drivers crash resetting command buffers whose descriptor pools were
# already recycled: reset them first in reset(), not later in beginRecording().
replace("src/dxvk/dxvk_cmdlist.cpp", '    if ((m_graphicsPool && m_vkd->vkResetCommandPool(m_vkd->device(), m_graphicsPool, 0) != VK_SUCCESS)\n     || (m_transferPool && m_vkd->vkResetCommandPool(m_vkd->device(), m_transferPool, 0) != VK_SUCCESS))\n      Logger::err("DxvkCommandList: Failed to reset command buffer");\n    ', '    // Command pools were reset in reset(), once the GPU finished with them.\n    ')
replace("src/dxvk/dxvk_cmdlist.cpp", '  void DxvkCommandList::reset() {\n    // Signal resources and events to\n    // avoid stalling main thread\n    m_signalTracker.reset();', '  void DxvkCommandList::reset() {\n    // Reset the command buffers while everything they reference still exists.\n    // Recycling descriptor pools and releasing resources first is legal, but\n    // the 2020 Adreno 6xx driver (Redmi Note 8, Adreno 610) then follows freed\n    // descriptor state while resetting and calls through a null pointer.\n    if ((m_graphicsPool && m_vkd->vkResetCommandPool(m_vkd->device(), m_graphicsPool, 0) != VK_SUCCESS)\n     || (m_transferPool && m_vkd->vkResetCommandPool(m_vkd->device(), m_transferPool, 0) != VK_SUCCESS))\n      Logger::err("DxvkCommandList: Failed to reset command buffer");\n\n    // Signal resources and events to\n    // avoid stalling main thread\n    m_signalTracker.reset();')
# Mali: no clip/cull distances (its compiler crashes on them) and no geometry shaders;
# PowerVR: no sample-rate shading. Enable those only where supported.
replace('src/d3d9/d3d9_device.cpp', '    enabled.core.features.shaderClipDistance = VK_TRUE;\n    enabled.core.features.shaderCullDistance = VK_TRUE;\n', '    // Mali has no clip/cull distances and its compiler crashes (null pointer in\n    // cmpbe_v2_compile_multiple_shaders) on shaders that write them, even though\n    // vkCreateDevice accepts the feature. Without it, shaders skip user clip planes.\n    enabled.core.features.shaderClipDistance = supported.core.features.shaderClipDistance;\n    enabled.core.features.shaderCullDistance = supported.core.features.shaderCullDistance;\n    g_d3d9ClipDistance = supported.core.features.shaderClipDistance;\n')
replace('src/d3d9/d3d9_device.cpp', '    enabled.core.features.geometryShader = VK_TRUE;\n', '    enabled.core.features.geometryShader = supported.core.features.geometryShader;\n')
replace('src/d3d9/d3d9_device.cpp', '    enabled.core.features.sampleRateShading = VK_TRUE;\n', '    enabled.core.features.sampleRateShading = supported.core.features.sampleRateShading;\n')
replace('src/d3d9/d3d9_device.cpp', 'namespace dxvk {\n', 'namespace dxvk {\n\n  // Whether vertex shaders may write gl_ClipDistance (set per device creation).\n  bool g_d3d9ClipDistance = true;\n')
replace('src/dxso/dxso_compiler.cpp', '  void DxsoCompiler::emitVsInit() {\n    m_module.enableCapability(spv::CapabilityClipDistance);\n', '  void DxsoCompiler::emitVsInit() {\n    if (g_d3d9ClipDistance)\n      m_module.enableCapability(spv::CapabilityClipDistance);\n')
replace('src/dxso/dxso_compiler.cpp', '    this->emitLinkerOutputSetup();\n\n    this->emitVsClipping();\n', '    this->emitLinkerOutputSetup();\n\n    if (g_d3d9ClipDistance)\n      this->emitVsClipping();\n')
replace('src/dxso/dxso_compiler.cpp', 'namespace dxvk {\n', 'namespace dxvk {\n\n  extern bool g_d3d9ClipDistance;\n')
replace('src/d3d9/d3d9_fixed_function.cpp', '    // VS Caps\n    m_module.enableCapability(spv::CapabilityClipDistance);\n', '    // VS Caps\n    if (g_d3d9ClipDistance)\n      m_module.enableCapability(spv::CapabilityClipDistance);\n')
replace('src/d3d9/d3d9_fixed_function.cpp', '    if (m_vsKey.Data.Contents.VertexClipping)\n      emitVsClipping(vtx);\n', '    if (m_vsKey.Data.Contents.VertexClipping && g_d3d9ClipDistance)\n      emitVsClipping(vtx);\n')
replace('src/d3d9/d3d9_fixed_function.cpp', 'namespace dxvk {\n', 'namespace dxvk {\n\n  extern bool g_d3d9ClipDistance;\n')
# DXVK warnings and errors reach logcat (tag DXVK) for tester reports.
replace('src/util/log/log.cpp', '#include "log.h"\n', '#include "log.h"\n#ifdef __ANDROID__\n#include <android/log.h>\n#endif\n')
replace('src/util/log/log.cpp', '        std::cerr << prefix << line << std::endl;\n', '        std::cerr << prefix << line << std::endl;\n#ifdef __ANDROID__\n        // stderr goes nowhere on Android: send warnings and errors to logcat,\n        // so tester reports (logcat.txt) show why DXVK failed.\n        if (level >= LogLevel::Warn)\n          __android_log_print(level >= LogLevel::Error ? ANDROID_LOG_ERROR : ANDROID_LOG_WARN, "DXVK", "%s", line.c_str());\n#endif\n')
# ...and the game's own log file when the app provides one (game_log.h), so they
# survive in diagnostic reports after logcat has dropped them.
replace('src/util/log/log.cpp', '#include "log.h"\n', '#include "log.h"\n#ifdef __ANDROID__\nextern "C" __attribute__((weak)) int nfsRuntimeLog(int, const char*, const char*, ...);\n#endif\n')
replace('src/util/log/log.cpp', '          __android_log_print(level >= LogLevel::Error ? ANDROID_LOG_ERROR : ANDROID_LOG_WARN, "DXVK", "%s", line.c_str());\n',
        '          (nfsRuntimeLog ? nfsRuntimeLog : __android_log_print)(level >= LogLevel::Error ? ANDROID_LOG_ERROR : ANDROID_LOG_WARN, "DXVK", "%s", line.c_str());\n')
(args.output / "version.h").write_text('#define DXVK_VERSION "native-1.9.2b-android-arm64"\n')
(args.output / "PINNED_REVISION").write_text(PIN + "\n")
print("Prepared DXVK " + PIN)

# Mali-G52/G57/G76 drivers lack depthClamp and fillModeNonSolid (CreateDevice
# D3DERR_NOTAVAILABLE): request them only when supported.
replace('src/d3d9/d3d9_device.cpp', '    enabled.core.features.depthClamp = VK_TRUE;\n', '    // Mali-G52/G57/G76 drivers have no depthClamp: D3D9 always clips depth, so\n    // pipelines only need it when the device has it (g_dxvkDepthClamp).\n    enabled.core.features.depthClamp = supported.core.features.depthClamp;\n    g_dxvkDepthClamp = supported.core.features.depthClamp;\n')
replace('src/d3d9/d3d9_device.cpp', '    enabled.core.features.fillModeNonSolid = VK_TRUE;\n', '    // Without it D3DFILL_WIREFRAME/POINT draw filled (see the rasterizer state).\n    enabled.core.features.fillModeNonSolid = supported.core.features.fillModeNonSolid;\n')
replace('src/d3d9/d3d9_device.cpp', '    state.polygonMode     = DecodeFillMode(D3DFILLMODE(rs[D3DRS_FILLMODE]));\n', '    state.polygonMode     = DecodeFillMode(D3DFILLMODE(rs[D3DRS_FILLMODE]));\n    if (!m_dxvkDevice->features().core.features.fillModeNonSolid)\n      state.polygonMode   = VK_POLYGON_MODE_FILL;\n')
replace('src/d3d9/d3d9_device.cpp', 'namespace dxvk {\n', 'namespace dxvk {\n  extern bool g_dxvkDepthClamp;\n')
replace('src/dxvk/dxvk_graphics.cpp', 'namespace dxvk {\n', 'namespace dxvk {\n  // False when the device lacks depthClamp (set before device creation).\n  bool g_dxvkDepthClamp = true;\n')
replace('src/dxvk/dxvk_graphics.cpp', '    rsInfo.depthClampEnable       = VK_TRUE;\n', '    rsInfo.depthClampEnable       = g_dxvkDepthClamp;\n')
replace('src/dxvk/dxvk_graphics.cpp', '      rsInfo.depthClampEnable = !state.rs.depthClipEnable();\n', '      rsInfo.depthClampEnable = g_dxvkDepthClamp && !state.rs.depthClipEnable();\n')
replace('src/dxvk/dxvk_meta_blit.cpp', 'namespace dxvk {\n', 'namespace dxvk {\n  extern bool g_dxvkDepthClamp;\n')
replace('src/dxvk/dxvk_meta_blit.cpp', '    rsState.depthClampEnable    = VK_TRUE;\n', '    rsState.depthClampEnable    = g_dxvkDepthClamp;\n')
replace('src/dxvk/dxvk_meta_copy.cpp', 'namespace dxvk {\n', 'namespace dxvk {\n  extern bool g_dxvkDepthClamp;\n')
replace('src/dxvk/dxvk_meta_copy.cpp', '    rsState.depthClampEnable    = VK_TRUE;\n', '    rsState.depthClampEnable    = g_dxvkDepthClamp;\n')
replace('src/dxvk/dxvk_meta_resolve.cpp', 'namespace dxvk {\n', 'namespace dxvk {\n  extern bool g_dxvkDepthClamp;\n')
replace('src/dxvk/dxvk_meta_resolve.cpp', '    rsState.depthClampEnable    = VK_TRUE;\n', '    rsState.depthClampEnable    = g_dxvkDepthClamp;\n')

# Reset on Mali: log why the swap chain could not be recreated.
replace('src/d3d9/d3d9_swapchain.cpp', '    if (m_presenter->recreateSwapChain(presenterDesc) != VK_SUCCESS)\n      throw DxvkError("D3D9SwapChainEx: Failed to recreate swap chain");\n', '    VkResult recreated = m_presenter->recreateSwapChain(presenterDesc);\n    if (recreated != VK_SUCCESS) {\n      Logger::err(str::format("D3D9SwapChainEx: Failed to recreate swap chain: ", recreated,\n        " (", presenterDesc.imageExtent.width, "x", presenterDesc.imageExtent.height, ")"));\n      throw DxvkError("D3D9SwapChainEx: Failed to recreate swap chain");\n    }\n')
# Reset after a failed back buffer allocation: no null back buffers.
replace("src/d3d9/d3d9_swapchain.cpp", '  void D3D9SwapChainEx::DestroyBackBuffers() {\n    for (auto& backBuffer : m_backBuffers)\n      backBuffer->ClearContainer();\n', '  void D3D9SwapChainEx::DestroyBackBuffers() {\n    // Entries stay null when a previous CreateBackBuffers threw half way.\n    for (auto& backBuffer : m_backBuffers)\n      if (backBuffer != nullptr)\n        backBuffer->ClearContainer();\n')
replace("src/d3d9/d3d9_swapchain.cpp", '    for (uint32_t i = 0; i < m_backBuffers.size(); i++)\n      m_backBuffers[i] = new D3D9Surface(m_parent, &desc, this);\n', '    // Android: allocating the images can fail (out of device memory on Reset,\n    // MediaTek/Unisoc reports). Leave no null entries behind for the next\n    // Reset, which used to crash in ClearContainer.\n    try {\n      for (uint32_t i = 0; i < m_backBuffers.size(); i++)\n        m_backBuffers[i] = new D3D9Surface(m_parent, &desc, this);\n    } catch (...) {\n      DestroyBackBuffers();\n      throw;\n    }\n')
# Persistent VkPipelineCache (pipeline_cache.cpp): no recompiling every pipeline each session.
replace("src/vulkan/vulkan_presenter.cpp", '    m_vkd->vkDestroySwapchainKHR(m_vkd->device(), m_swapchain, nullptr);', '    if (m_swapchain != VK_NULL_HANDLE)\n      m_vkd->vkDestroySwapchainKHR(m_vkd->device(), m_swapchain, nullptr);')
replace("src/vulkan/vulkan_presenter.cpp", '    m_vki->vkDestroySurfaceKHR(m_vki->instance(), m_surface, nullptr);', '    if (m_surface != VK_NULL_HANDLE)\n      m_vki->vkDestroySurfaceKHR(m_vki->instance(), m_surface, nullptr);\n    m_surface = VK_NULL_HANDLE;')
# The surface keeps a reference to its ANativeWindow until DXVK destroys it
# (android_wsi.cpp). Destroying a surface whose window Android already freed
# crashed in the driver when the game came back from the background.
replace("src/vulkan/vulkan_presenter.cpp", '#include "../wsi/wsi_presenter.h"\n', '#include "../wsi/wsi_presenter.h"\n\nextern "C" __attribute__((weak)) void dxvkAndroidSurfaceDestroyed(VkSurfaceKHR surface);\n')
replace("src/vulkan/vulkan_presenter.cpp", 'm_vki->vkDestroySurfaceKHR(m_vki->instance(), m_surface, nullptr);', '{ m_vki->vkDestroySurfaceKHR(m_vki->instance(), m_surface, nullptr); if (dxvkAndroidSurfaceDestroyed) dxvkAndroidSurfaceDestroyed(m_surface); }')
# PowerVR BXM (MT6855) faults inside vkCreateGraphicsPipelines: see pipeline_guard.h.
shutil.copy2(pathlib.Path(__file__).parent / "pipeline_guard.h", args.output / "src/dxvk/dxvk_pipeline_guard.h")
replace("src/dxvk/dxvk_graphics.cpp", '#include "dxvk_graphics.h"\n', '#include "dxvk_graphics.h"\n#include "dxvk_pipeline_guard.h"\n')
replace("src/dxvk/dxvk_graphics.cpp", '''    if (m_vkd->vkCreateGraphicsPipelines(m_vkd->device(),
          m_pipeMgr->m_cache->handle(), 1, &info, nullptr, &pipeline) != VK_SUCCESS) {
''', '''    VkResult createResult;
    if (m_pipeMgr->m_device->adapter()->deviceProperties().vendorID == powervr::VendorId) {
      // Valid for every driver, plainer for this one: no shared cache object, no
      // null arrays where the states are dynamic, no vertex formats it cannot fetch.
      std::array<VkViewport, DxvkLimits::MaxNumViewports> anyViewports = { };
      std::array<VkRect2D,   DxvkLimits::MaxNumViewports> anyScissors  = { };
      vpInfo.pViewports = anyViewports.data();
      vpInfo.pScissors  = anyScissors.data();
      for (uint32_t i = 0; i < state.il.attributeCount(); i++) {
        VkFormatProperties format = m_pipeMgr->m_device->adapter()->formatProperties(viAttribs[i].format);
        if (!(format.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT))
          viAttribs[i].format = VK_FORMAT_R8G8B8A8_SNORM;
      }
      createResult = powervr::guarded([&] (int level) {
        if (level >= 1) {  // No extension structures.
          rsInfo.pNext = nullptr;
          viInfo.pNext = nullptr;
        }
        if (level >= 2) {  // Only the render targets in use (the render pass declares 8).
          uint32_t used = 0;
          for (uint32_t i = 0; i < MaxNumRenderTargets; i++)
            if (passFormat.color[i].format != VK_FORMAT_UNDEFINED)
              used = i + 1;
          cbInfo.attachmentCount = used;
        }
        if (level >= 3) {  // Shaders with their default specialization constants.
          for (auto& stage : stages)
            stage.pSpecializationInfo = nullptr;
        }
        return m_vkd->vkCreateGraphicsPipelines(m_vkd->device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
      });
    } else {
      createResult = m_vkd->vkCreateGraphicsPipelines(m_vkd->device(),
        m_pipeMgr->m_cache->handle(), 1, &info, nullptr, &pipeline);
    }
    if (createResult != VK_SUCCESS) {
''')
shutil.copy2(pathlib.Path(__file__).parent / "pipeline_cache.cpp", args.output / "src/dxvk/dxvk_pipecache.cpp")
shutil.copy2(pathlib.Path(__file__).parent / "pipeline_cache.h", args.output / "src/dxvk/dxvk_pipecache.h")
replace("src/dxvk/dxvk_pipemanager.cpp", '    m_cache     (new DxvkPipelineCache(device->vkd())) {', '    m_cache     (new DxvkPipelineCache(device->vkd(), device->properties().core.properties)) {')
(args.output / "ANDROID_PREPARE_SHA256").write_text(hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest() + "\n", encoding="ascii")
