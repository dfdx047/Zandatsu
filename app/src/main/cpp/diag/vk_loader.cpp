// zdiag: carrega a libvulkan em tempo de execucao (sistema ou driver alternativo via libadrenotools).
#include <dlfcn.h>

#include <cstdio>

#include "zdiag.h"

#ifdef ZDIAG_HAS_ADRENOTOOLS
#include <adrenotools/driver.h>
#endif

namespace zdiag {

VkFns vk;

namespace {
void* g_handle = nullptr;
bool g_custom = false;
std::string g_desc, g_error, g_requested;

std::string Slash(std::string s) {
  if (!s.empty() && s.back() != '/') s += '/';
  return s;
}
}  // namespace

const std::string& LastLoaderError() { return g_error; }

bool LoadVulkan(const LoaderInfo& li) {
  if (vk.GetInstanceProcAddr) return true;
  g_error.clear();
  const bool want_custom = !li.driverDir.empty() && !li.driverName.empty();
  if (want_custom) {
    g_requested = Slash(li.driverDir) + li.driverName;
#ifdef ZDIAG_HAS_ADRENOTOOLS
    // Igual a external/rexglue-sdk/src/ui/vulkan/vulkan_instance.cpp (Dbz3OpenCustomVulkanDriver).
    std::string hook = Slash(li.nativeLibDir), dir = Slash(li.driverDir);
    g_handle = adrenotools_open_libvulkan(RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM, nullptr, hook.c_str(),
                                          dir.c_str(), li.driverName.c_str(), nullptr, nullptr);
    if (!g_handle) {
      const char* e = dlerror();
      g_error = std::string("adrenotools falhou com ") + g_requested + ": " + (e ? e : "sem detalhe");
    }
#else
    g_error = "este build nao tem libadrenotools (driver alternativo indisponivel)";
#endif
    // Sem troca silenciosa: se o driver pedido nao carregar, o teste falha.
    if (!g_handle) return false;
    g_custom = true;
    g_desc = g_requested;
  } else {
    g_handle = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!g_handle) {
      const char* e = dlerror();
      g_error = std::string("dlopen(libvulkan.so): ") + (e ? e : "?");
      return false;
    }
    g_desc = "libvulkan.so";
  }
  vk.GetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(g_handle, "vkGetInstanceProcAddr"));
  if (!vk.GetInstanceProcAddr) {
    g_error = "vkGetInstanceProcAddr nao encontrado";
    return false;
  }
#define ZD_LOAD(n) vk.n = reinterpret_cast<PFN_vk##n>(vk.GetInstanceProcAddr(nullptr, "vk" #n));
  ZD_VK_GLOBAL(ZD_LOAD)
#undef ZD_LOAD
  if (!vk.CreateInstance) {
    g_error = "vkCreateInstance nao encontrado";
    return false;
  }
  return true;
}

void LoadInstanceFns(VkInstance inst) {
#define ZD_LOAD(n) vk.n = reinterpret_cast<PFN_vk##n>(vk.GetInstanceProcAddr(inst, "vk" #n));
  ZD_VK_INSTANCE(ZD_LOAD)
#undef ZD_LOAD
  auto alt = [&](auto& fn, const char* name) {
    if (!fn) fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(vk.GetInstanceProcAddr(inst, name));
  };
  alt(vk.GetPhysicalDeviceProperties2, "vkGetPhysicalDeviceProperties2KHR");
  alt(vk.GetPhysicalDeviceFeatures2, "vkGetPhysicalDeviceFeatures2KHR");
  alt(vk.GetPhysicalDeviceCalibrateableTimeDomainsEXT, "vkGetPhysicalDeviceCalibrateableTimeDomainsKHR");
}

void LoadDeviceFns(VkDevice dev) {
#define ZD_LOAD(n) vk.n = reinterpret_cast<PFN_vk##n>(vk.GetDeviceProcAddr(dev, "vk" #n));
  ZD_VK_DEVICE(ZD_LOAD)
#undef ZD_LOAD
  auto alt = [&](auto& fn, const char* name) {
    if (!fn) fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(vk.GetDeviceProcAddr(dev, name));
  };
  alt(vk.GetBufferDeviceAddress, "vkGetBufferDeviceAddressKHR");
  alt(vk.CmdBeginRendering, "vkCmdBeginRenderingKHR");
  alt(vk.CmdEndRendering, "vkCmdEndRenderingKHR");
  alt(vk.GetCalibratedTimestampsEXT, "vkGetCalibratedTimestampsKHR");
}

void WriteLoaderJson(Json& j) {
  j.obj().kv("library", g_custom ? "custom" : g_handle ? "system" : "none").kv("path", g_desc);
  if (!g_requested.empty()) j.kv("requested", g_requested);
  if (!g_error.empty()) j.kv("error", g_error);
  j.end();
}

const char* ResultName(VkResult r) {
  switch (r) {
#define R(x) case x: return #x;
    R(VK_SUCCESS) R(VK_NOT_READY) R(VK_TIMEOUT) R(VK_EVENT_SET) R(VK_EVENT_RESET) R(VK_INCOMPLETE)
    R(VK_ERROR_OUT_OF_HOST_MEMORY) R(VK_ERROR_OUT_OF_DEVICE_MEMORY) R(VK_ERROR_INITIALIZATION_FAILED)
    R(VK_ERROR_DEVICE_LOST) R(VK_ERROR_MEMORY_MAP_FAILED) R(VK_ERROR_LAYER_NOT_PRESENT)
    R(VK_ERROR_EXTENSION_NOT_PRESENT) R(VK_ERROR_FEATURE_NOT_PRESENT) R(VK_ERROR_INCOMPATIBLE_DRIVER)
    R(VK_ERROR_TOO_MANY_OBJECTS) R(VK_ERROR_FORMAT_NOT_SUPPORTED) R(VK_ERROR_FRAGMENTED_POOL)
    R(VK_ERROR_UNKNOWN) R(VK_ERROR_OUT_OF_POOL_MEMORY) R(VK_ERROR_INVALID_EXTERNAL_HANDLE)
    R(VK_ERROR_FRAGMENTATION) R(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS) R(VK_PIPELINE_COMPILE_REQUIRED)
    R(VK_ERROR_INVALID_SHADER_NV)
#undef R
    default: {
      static thread_local char b[32];
      snprintf(b, sizeof(b), "VkResult(%d)", int(r));
      return b;
    }
  }
}

}  // namespace zdiag
