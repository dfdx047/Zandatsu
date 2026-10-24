// Renut (renderizador nativo de Direct3D do mgr-nx-deko) no SwordMan: a escolha no arranque e os hooks que o Renut
// partilha com a cola do MGR deles (app/mgr_src/mgr_me_hooks.cpp). Compilado so com SM_RENUT (cmake/renut.cmake).
//
// swordman_renderer = xenos (padrao): o Xenia emulado de sempre; todos os hooks do Renut chamam a funcao original.
// swordman_renderer = renut: o Direct3D do jogo corre no Renut (plume/Vulkan), sem anel PM4 nem EDRAM.

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <string>

#include <dlfcn.h>
#include <sched.h>

#include <adrenotools/driver.h>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/ppc.h>

#include <rex/logging.h>

#include "renut_engine/gpu/sa2_renut_yield.h"

REXCVAR_DECLARE(std::string, present_effect);  // SDK (ui/presenter.cpp)

REXCVAR_DEFINE_STRING(swordman_renderer, "xenos", "SwordMan",
                      "Renderizador, escolhido no arranque. xenos = GPU do Xbox emulada (Xenia, plugin xenos). "
                      "renut = Direct3D do jogo nativo no Renut do mgr-nx-deko (plume, Vulkan)")
    .allowed({"xenos", "renut"})
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mgr_renut_diag_original_draw, false, "MGR/Diagnostics",
                    "Renut: corre tambem o Draw original do jogo antes do do Renut (diagnostico)");

// Frames apresentados pelo jogo no modo Renut (o VdSwap do SDK nao e chamado: o Renut substitui o D3DDevice_Swap).
std::atomic<uint64_t> g_sa2_game_frames{0};

// SDK (patch 0100): pausa dos menus do app e contador do vigia (android_hooks.cpp). No modo Xenia o VdSwap chama-o;
// no Renut o VdSwap nunca corre, por isso e chamado aqui, no inicio do Swap, na mesma thread do jogo.
extern "C" void (*rex_vdswap_hook)(void);
void SwordManRenutShotOnSwap();          // abaixo
void SwordManRenutOverlayStatsOnSwap();  // abaixo
void SwordManRenutPlaceMainThreadOnSwap();  // abaixo

REX_EXTERN(__imp__sub_826B3510);
REX_HOOK_RAW(sub_826B3510) {  // D3DDevice_Swap
  if (banjo::renut_gpu::Active()) {
    if (rex_vdswap_hook) rex_vdswap_hook();
    SwordManRenutShotOnSwap();
    SwordManRenutOverlayStatsOnSwap();
    SwordManRenutPlaceMainThreadOnSwap();
    renut_native_sub_826B3510(ctx, base);
    g_sa2_game_frames.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  __imp__sub_826B3510(ctx, base);
}

REX_EXTERN(__imp__sub_826BCD40);
REX_HOOK_RAW(sub_826BCD40) {  // D3DDevice_DrawIndexedVertices
  if (banjo::renut_gpu::Active() && REXCVAR_GET(mgr_renut_diag_original_draw)) {
    const PPCContext arguments = ctx;
    __imp__sub_826BCD40(ctx, base);
    ctx = arguments;
  }
  SA2_RENUT_YIELD_SHARED(sub_826BCD40);
  __imp__sub_826BCD40(ctx, base);
}

REX_EXTERN(__imp__sub_826BC928);
REX_HOOK_RAW(sub_826BC928) {  // D3DDevice_DrawVertices
  if (banjo::renut_gpu::Active() && REXCVAR_GET(mgr_renut_diag_original_draw)) {
    const PPCContext arguments = ctx;
    __imp__sub_826BC928(ctx, base);
    ctx = arguments;
  }
  SA2_RENUT_YIELD_SHARED(sub_826BC928);
  __imp__sub_826BC928(ctx, base);
}

// ---------------------------------------------------------------------------------------------------------------
// O que o Renut le da cola do SA2 deles (app/src/cpu/sa2_enhancements.cpp, me/sa2_render_mode.cpp, sa2_capture.cpp).
// Esses ficheiros trazem a maquinaria do SA2 (autoverificacao do codegen, contadores do SDK deles, captura de video);
// aqui so o que o renderizador chama, com os padroes deles.
// ---------------------------------------------------------------------------------------------------------------

#include "cpu/sa2_enhancements.h"
#include "me/sa2_render_mode.h"
#include "sa2_capture.h"

REXCVAR_DEFINE_INT32(sa2_render_height_handheld, 720, "SwordMan",
                     "Renut: altura de render (720 = os 1280x720 do jogo). Multiplos de 180 de 360 a 2160; a largura "
                     "segue a 16:9")
    .range(180, 4320)
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

namespace sa2::enh {
// sa2_cpu_enhancements / sa2_noncpu_enhancements, ligados por padrao no deles: cada melhoria do Renut segue a sua
// propria cvar.
bool CpuMaster() { return true; }
bool NonCpuMaster() { return true; }
}  // namespace sa2::enh

namespace sa2::render {
// Sem modo "docked" no Android: sempre a altura de sa2_render_height_handheld.
bool Docked() { return false; }
int32_t RenderHeightOf(bool) { return REXCVAR_GET(sa2_render_height_handheld); }
void OnPresented(uint64_t) {}
}  // namespace sa2::render

namespace sa2::capture {
// Sem a captura de video em lockstep do PC deles (a do SwordMan e a do AutoPilot).
bool RenutLockstepOn() { return false; }
void RenutLockstepFrame(uint64_t, const rex::ui::RawImage*) {}
}  // namespace sa2::capture

// ---------------------------------------------------------------------------------------------------------------
// Driver Vulkan do plume (patch renut/0006): o mesmo que o SDK carrega para o Xenia (vulkan_instance.cpp,
// Dbz3OpenCustomVulkanDriver): o GameActivity define REX_VULKAN_DRIVER_DIR/NAME (aba Drivers, Turnip) e
// REX_NATIVE_LIB_DIR; sem elas, ou se falhar, o plume usa o libvulkan do sistema.
// ---------------------------------------------------------------------------------------------------------------
static void* OpenVulkanDriver();

// Uma so abertura por processo (a verificacao de funcoes e o plume usam o mesmo driver).
extern "C" void* plume_android_vulkan_library() {
  static void* const handle = OpenVulkanDriver();
  return handle;
}

static void* OpenVulkanDriver() {
  const char* driver_dir = std::getenv("REX_VULKAN_DRIVER_DIR");
  const char* driver_name = std::getenv("REX_VULKAN_DRIVER_NAME");
  const char* hook_dir = std::getenv("REX_NATIVE_LIB_DIR");
  if (!driver_dir || !*driver_dir || !driver_name || !*driver_name || !hook_dir || !*hook_dir) {
    REXLOG_INFO("Renut: driver Vulkan do sistema");
    return nullptr;
  }
  std::string dir(driver_dir), hooks(hook_dir);
  if (dir.back() != '/') dir += '/';
  if (hooks.back() != '/') hooks += '/';
  void* handle = adrenotools_open_libvulkan(RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM, nullptr, hooks.c_str(), dir.c_str(),
                                            driver_name, nullptr, nullptr);
  if (handle) {
    REXLOG_INFO("Renut: driver Vulkan alternativo {}{}", dir, driver_name);
  } else {
    const char* error = dlerror();
    REXLOG_ERROR("Renut: o driver alternativo {}{} falhou a carregar ({}); a usar o do sistema", dir, driver_name,
                 error ? error : "sem detalhe");
  }
  return handle;
}

// ---------------------------------------------------------------------------------------------------------------
// Diagnostico: swordman_renut_shot_s = N grava, N segundos apos o primeiro Swap, a imagem que o Renut apresenta
// (renut::gpu::CaptureFrame: a mesma passagem de scan-out, para um alvo fora da swapchain) em logs/renut_shot.ppm.
// ---------------------------------------------------------------------------------------------------------------
#include <chrono>
#include <fstream>
#include <thread>

#include <rex/main_android.h>

#include "renut_engine/gpu/banjo_renut_capture.h"

REXCVAR_DEFINE_INT32(swordman_renut_shot_s, 0, "SwordMan",
                     "Renut (diagnostico): segundos apos o primeiro frame para gravar logs/renut_shot.ppm; 0 = nunca");

void SwordManRenutShotOnSwap() {
  static std::atomic<bool> started{false};
  const int32_t seconds = REXCVAR_GET(swordman_renut_shot_s);
  if (seconds <= 0 || started.exchange(true)) return;
  std::thread([seconds] {
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    rex::ui::RawImage image;
    if (!renut::gpu::CaptureFrame(image, 2000)) {
      REXLOG_WARN("Renut: captura falhou (nenhum frame apresentado em 2 s)");
      return;
    }
    const auto path = rex::GetAndroidAppDirectory() / "logs" / "renut_shot.ppm";
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << image.width << " " << image.height << "\n255\n";
    uint64_t sum = 0;
    for (uint32_t y = 0; y < image.height; ++y) {
      const uint8_t* row = image.data.data() + y * image.stride;
      for (uint32_t x = 0; x < image.width; ++x) {
        out.write(reinterpret_cast<const char*>(row + x * 4), 3);
        sum += row[x * 4] + row[x * 4 + 1] + row[x * 4 + 2];
      }
    }
    REXLOG_INFO("Renut: captura {}x{} em {} (media {:.1f})", image.width, image.height, path.string(),
                double(sum) / (3.0 * image.width * image.height));
  }).detach();
}

// ---------------------------------------------------------------------------------------------------------------
// O que os shaders traduzidos e o plume assumem sem verificar (docs/renut-port-analysis.md 5.2). Corre no
// OnPreSetup, antes de qualquer superficie: sem estas funcoes o MgrApp fica no Xenia, com a lista do que falta no log.
// ---------------------------------------------------------------------------------------------------------------
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

bool SwordManRenutDeviceSupported() {
  void* library = plume_android_vulkan_library();
  if (!library) library = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
  auto get_instance_proc = library ? reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                                         dlsym(library, "vkGetInstanceProcAddr"))
                                   : nullptr;
  if (!get_instance_proc) {
    REXLOG_ERROR("Renut: sem driver Vulkan");
    return false;
  }
  auto create_instance =
      reinterpret_cast<PFN_vkCreateInstance>(get_instance_proc(nullptr, "vkCreateInstance"));
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.apiVersion = VK_API_VERSION_1_2;
  VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  info.pApplicationInfo = &app;
  VkInstance instance = VK_NULL_HANDLE;
  if (!create_instance || create_instance(&info, nullptr, &instance) != VK_SUCCESS) {
    REXLOG_ERROR("Renut: vkCreateInstance (Vulkan 1.2) falhou");
    return false;
  }
#define SM_VK(name) auto name = reinterpret_cast<PFN_##name>(get_instance_proc(instance, #name))
  SM_VK(vkDestroyInstance);
  SM_VK(vkEnumeratePhysicalDevices);
  SM_VK(vkGetPhysicalDeviceFeatures2);
  SM_VK(vkGetPhysicalDeviceProperties2);
  SM_VK(vkGetPhysicalDeviceFormatProperties);
#undef SM_VK
  uint32_t count = 1;
  VkPhysicalDevice device = VK_NULL_HANDLE;
  vkEnumeratePhysicalDevices(instance, &count, &device);
  std::string missing;
  if (!device) {
    missing = " dispositivo";
  } else {
    VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceFeatures2 f{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &f12};
    vkGetPhysicalDeviceFeatures2(device, &f);
    VkPhysicalDeviceVulkan12Properties p12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};
    VkPhysicalDeviceProperties2 p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &p12};
    vkGetPhysicalDeviceProperties2(device, &p);
    VkFormatProperties d32s8{};
    vkGetPhysicalDeviceFormatProperties(device, VK_FORMAT_D32_SFLOAT_S8_UINT, &d32s8);
    const auto need = [&](bool ok, const char* what) {
      if (!ok) missing += std::string(" ") + what;
    };
    need(f12.bufferDeviceAddress, "bufferDeviceAddress");
    need(f.features.shaderInt64, "shaderInt64");
    need(f12.descriptorIndexing && f12.runtimeDescriptorArray && f12.descriptorBindingPartiallyBound &&
             f12.descriptorBindingVariableDescriptorCount && f12.descriptorBindingSampledImageUpdateAfterBind,
         "descriptorIndexing");
    need(p12.maxDescriptorSetUpdateAfterBindSampledImages >= 8192, "8192-texturas");
    need(f.features.textureCompressionBC, "textureCompressionBC");
    need(f.features.vertexPipelineStoresAndAtomics, "vertexPipelineStoresAndAtomics");
    need(d32s8.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, "D32S8");
    REXLOG_INFO("Renut: {} (Vulkan {}.{}), geometryShader {}", p.properties.deviceName,
                VK_API_VERSION_MAJOR(p.properties.apiVersion), VK_API_VERSION_MINOR(p.properties.apiVersion),
                f.features.geometryShader ? "sim" : "nao (retangulos e point sprites ficam errados)");
  }
  vkDestroyInstance(instance, nullptr);
  if (!missing.empty()) {
    REXLOG_ERROR("Renut: o driver nao tem{}; a usar o Xenia", missing);
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Overlay de depuracao do SDK no modo Renut: no Xenia o command processor conta os swaps e diz a resolucao; aqui e o
// Swap do Renut. O "Upscaler" vem do SDK 0175: o present do Renut escala o frame para o ecra (bilinear), sem o FSR
// do presenter do Xenia.
// ---------------------------------------------------------------------------------------------------------------
#include <rex/perf/frame_rate.h>

void SwordManRenutOverlayStatsOnSwap() {
  static const bool once = [] {
    const int32_t height = REXCVAR_GET(sa2_render_height_handheld);
    rex::perf::RecordGuestFrontbuffer(1280, 720);
    rex::perf::SetDrawResolutionScale(1, 1, 1, 1);
    rex::perf::SetDrawResolutionPercent(uint32_t(height > 0 ? height * 100 / 720 : 100));
    return true;
  }();
  (void)once;
  // O filtro do passe final (renut/0019: FSR 1, CAS), ao vivo: o menu muda present_effect com o jogo a correr.
  static std::string shown;
  const std::string &effect = REXCVAR_GET(present_effect);
  if (effect != shown) {
    shown = effect;
    rex::perf::SetUpscalerOverride(effect == "fsr"   ? "Renut \xC2\xB7 AMD FSR 1"
                                   : effect == "cas" ? "Renut \xC2\xB7 AMD CAS"
                                                     : "Renut \xC2\xB7 bilinear");
  }
  rex::perf::RecordGuestSwap();
}

// ---------------------------------------------------------------------------------------------------------------
// Núcleos da thread principal do jogo (a do Swap) e da thread de render do Renut (patch renut/0011).
// swordman_renut_cores: 0 = o escalonador decide (como antes); 1 = as duas nos núcleos grandes, livres entre eles;
// 2 = a principal no núcleo mais rápido e a de render nos outros grandes (nunca no mesmo). Núcleos lidos do
// cpufreq: "grandes" = máximo acima do mais lento (no SD865: 4-6 A77 a 2,42 GHz e 7 A77 a 2,84 GHz).
// ---------------------------------------------------------------------------------------------------------------
#include <fstream>
#include <vector>

REXCVAR_DEFINE_INT32(swordman_renut_cores, 0, "SwordMan",
                     "Renut: núcleos da thread principal e da de render. 0 = livres; 1 = as duas nos núcleos grandes; "
                     "2 = principal no mais rápido, render nos outros grandes")
    .range(0, 2)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

struct CoreSets {
  cpu_set_t big{};      // todos os grandes
  cpu_set_t fastest{};  // o de frequência mais alta
  cpu_set_t others{};   // grandes sem o mais rápido
  bool ok = false;
};

const CoreSets& Cores() {
  static const CoreSets sets = [] {
    CoreSets out;
    std::vector<long> freq;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
      std::ifstream f("/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/cpufreq/cpuinfo_max_freq");
      long khz = 0;
      if (!(f >> khz)) break;
      freq.push_back(khz);
    }
    if (freq.size() < 2) return out;
    long lo = freq[0], hi = freq[0];
    int fastest = 0;
    for (size_t i = 0; i < freq.size(); ++i) {
      lo = std::min(lo, freq[i]);
      if (freq[i] > hi) { hi = freq[i]; fastest = int(i); }
    }
    if (hi == lo) return out;
    CPU_ZERO(&out.big); CPU_ZERO(&out.fastest); CPU_ZERO(&out.others);
    CPU_SET(fastest, &out.fastest);
    for (size_t i = 0; i < freq.size(); ++i) {
      if (freq[i] > lo) {
        CPU_SET(int(i), &out.big);
        if (int(i) != fastest) CPU_SET(int(i), &out.others);
      }
    }
    // Um só núcleo grande: as duas partilham-no (melhor que um pequeno).
    if (CPU_COUNT(&out.others) == 0) out.others = out.fastest;
    out.ok = true;
    return out;
  }();
  return sets;
}

void PlaceCurrentThread(const char* what, bool main_thread) {
  const int32_t mode = REXCVAR_GET(swordman_renut_cores);
  const CoreSets& c = Cores();
  if (mode == 0 || !c.ok) return;
  const cpu_set_t& set = mode == 1 ? c.big : (main_thread ? c.fastest : c.others);
  const bool ok = sched_setaffinity(0, sizeof(set), &set) == 0;
  std::string list;
  for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
    if (CPU_ISSET(cpu, &set)) list += (list.empty() ? "" : ",") + std::to_string(cpu);
  REXLOG_INFO("Renut: {} nos núcleos {}{}", what, list, ok ? "" : " (recusado)");
}

}  // namespace

extern "C" void swordman_renut_render_thread_start() { PlaceCurrentThread("thread de render", false); }

void SwordManRenutPlaceMainThreadOnSwap() {
  static const bool once = (PlaceCurrentThread("thread principal do jogo", true), true);
  (void)once;
}
