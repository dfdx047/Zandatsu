// Marcadores de GPU por draw no Renut: ver renut_gpu_markers.h.

#include "renut_gpu_markers.h"

#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <plume_vulkan.h>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/main_android.h>

#include "renut_engine/gpu/backend/video.h"
#include "renut_engine/gpu/banjo_renut_record.h"

REXCVAR_DEFINE_BOOL(swordman_renut_gpu_markers, false, "SwordMan",
                    "Renut (diagnóstico): marcadores de GPU por draw; no device lost escreve logs/renut_gpu_hang.txt "
                    "com os comandos em que a GPU parou")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(swordman_renut_gpu_markers_cpu, false, "SwordMan",
                    "Renut (diagnóstico): com swordman_renut_gpu_markers, só o anel da CPU (nada é escrito na GPU, o "
                    "ritmo da GPU fica igual); no device lost escreve os últimos 16384 comandos para cruzar com o "
                    "snapshot do kgsl (/sys/class/kgsl/kgsl-3d0/snapshot/dump). Também a pedido: criar "
                    "logs/renut_gpu_ring.req escreve logs/renut_gpu_ring_<n>.txt com o anel inteiro (tools/kgsl/"
                    "hangwatch.sh faz isso quando o dmesg mostra um hang que o kgsl recuperou, sem device lost)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(swordman_renut_gpu_markers_end, true, "SwordMan",
                    "Renut (diagnóstico): com swordman_renut_gpu_markers, também o marcador do fim de cada comando "
                    "(o fim do pipeline espera pelo draw: +~5 ms de GPU e muda o ritmo); false = só o do início")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace swordman::gpu_reset {
void Trigger(const char* why);  // app/src/main/cpp/gpu_reset.cpp
}

// Frames do jogo (src/renut_glue.cpp, contados no Swap).
extern std::atomic<uint64_t> g_sa2_game_frames;

namespace swordman::gpu_markers {
namespace {

constexpr uint32_t kRing = 1u << 16;  // comandos guardados (índice & (kRing - 1))

struct Entry {
  uint32_t index = 0;
  uint64_t frame = 0;
  Info info;
};

struct State {
  bool ready = false;
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  volatile uint32_t* words = nullptr;  // [0] último que começou, [1] último que acabou
  PFN_vkCmdWriteBufferMarkerAMD write_marker = nullptr;
  PFN_vkCmdWriteBufferMarker2AMD write_marker2 = nullptr;
  std::atomic<uint32_t> next{1};
  Entry ring[kRing];
};

State& S() {
  static State s;
  return s;
}

// Uma vez, na primeira chamada (thread de render, com o dispositivo criado).
bool Ready() {
  static const bool ok = [] {
    State& s = S();
    auto* device = static_cast<plume::VulkanDevice*>(renut::gpu::Video::Device());
    if (!device) return false;
    // O plume não chama volkLoadDevice: o vkCmdWriteBufferMarkerAMD global é o trampolim da instância, que nunca é
    // nulo mesmo que o dispositivo não tenha a extensão. O do dispositivo é nulo se ela não estiver ligada.
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device->physicalDevice, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(device->physicalDevice, nullptr, &count, exts.data());
    bool listed = false;
    for (const auto& e : exts) listed |= std::strcmp(e.extensionName, VK_AMD_BUFFER_MARKER_EXTENSION_NAME) == 0;
    // No Turnip v27 o vkCmdWriteBufferMarkerAMD é o do Mesa comum, que salta para o CmdWriteBufferMarker2AMD do
    // dispositivo; essa entrada estava nula (SIGSEGV em pc 0). O 2AMD (com synchronization2) vem primeiro.
    s.write_marker2 = reinterpret_cast<PFN_vkCmdWriteBufferMarker2AMD>(
        vkGetDeviceProcAddr(device->vk, "vkCmdWriteBufferMarker2AMD"));
    s.write_marker = reinterpret_cast<PFN_vkCmdWriteBufferMarkerAMD>(
        vkGetDeviceProcAddr(device->vk, "vkCmdWriteBufferMarkerAMD"));
    REXLOG_INFO("Renut: marcadores de GPU: VK_AMD_buffer_marker {}, vkCmdWriteBufferMarker2AMD {:p}, "
                "vkCmdWriteBufferMarkerAMD {:p}", listed ? "anunciado" : "não anunciado",
                reinterpret_cast<void*>(s.write_marker2), reinterpret_cast<void*>(s.write_marker));
    if (!listed || !s.write_marker2) {
      REXLOG_WARN("Renut: marcadores de GPU sem vkCmdWriteBufferMarker2AMD neste driver: desligados");
      return false;
    }
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = 256;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (vkCreateBuffer(device->vk, &info, nullptr, &s.buffer) != VK_SUCCESS) return false;
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device->vk, s.buffer, &req);
    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties(device->physicalDevice, &props);
    const VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    uint32_t type = UINT32_MAX;
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i)
      if ((req.memoryTypeBits & (1u << i)) && (props.memoryTypes[i].propertyFlags & want) == want) {
        type = i;
        break;
      }
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = type;
    void* mapped = nullptr;
    if (type == UINT32_MAX || vkAllocateMemory(device->vk, &alloc, nullptr, &s.memory) != VK_SUCCESS ||
        vkBindBufferMemory(device->vk, s.buffer, s.memory, 0) != VK_SUCCESS ||
        vkMapMemory(device->vk, s.memory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS) {
      REXLOG_WARN("Renut: marcadores de GPU: sem memória HOST_VISIBLE|HOST_COHERENT");
      return false;
    }
    s.words = static_cast<volatile uint32_t*>(mapped);
    s.words[0] = s.words[1] = 0;
    REXLOG_INFO("Renut: marcadores de GPU ligados (VK_AMD_buffer_marker, memória tipo {})", type);
    return true;
  }();
  return ok;
}

// Na lista real (com banjo_renut_record_worker, a thread worker quando reproduz o comando gravado pelo proxy, na sua
// posição). offset 0 = "começou" (início do pipeline), 4 = "acabou" (fim).
void Write(plume::RenderCommandList* real, uint32_t offset, uint32_t value) {
  VkCommandBuffer vk = static_cast<plume::VulkanCommandList*>(real)->vk;
  S().write_marker2(vk, offset ? VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                    S().buffer, offset, value);
}

const char* KindName(uint32_t kind) {
  switch (kind) {
    case kDraw: return "draw";
    case kClear: return "clear";
    case kResolve: return "resolve";
    case kScanOut: return "scan-out";
    case kMip: return "mip";
    default: return "?";
  }
}

// Os comandos [from, to] do anel (no máximo limit) no formato do renut_gpu_hang.txt (tools/kgsl/hangmap.py).
void WriteCommands(FILE* f, uint32_t from, uint32_t to, uint32_t limit, uint32_t started, uint32_t finished) {
  State& s = S();
  std::fprintf(f, "  índice    frame  tipo      PS                VS                cor (va wxh fmt)          "
                  "profund. (va fmt)  prim  contagem inst\n");
  for (uint32_t i = from; i <= to && i - from < limit; ++i) {
    const Entry& e = s.ring[i & (kRing - 1)];
    if (e.index != i) continue;
    const Info& n = e.info;
    const char* mark = (finished ? (i > finished && i <= started) : i == started) ? ">" : " ";
    std::fprintf(f, "%s %8u %8llu  %-8s  %016llX  %016llX  %08X %4ux%-4u fmt %-3u  %08X fmt %-3u  %4u  %8u %4u\n", mark,
                 i, (unsigned long long)e.frame, KindName(n.kind), (unsigned long long)n.ps,
                 (unsigned long long)n.vs, n.color_va, n.color_width, n.color_height, n.color_format, n.depth_va,
                 n.depth_format, n.primitive, n.count, n.instances);
  }
}

// Só CPU: uma vez por frame do jogo, se existir logs/renut_gpu_ring.req, escreve o anel inteiro (os comandos até
// este) em logs/renut_gpu_ring_<n>.txt e apaga o pedido. O hang que o kgsl recupera não chega a device lost.
void CheckRingRequest(uint64_t frame, uint32_t index) {
  static uint64_t last_frame = 0;
  static uint32_t dumps = 0;
  if (frame == last_frame) return;
  last_frame = frame;
  static const std::string req = (rex::GetAndroidAppDirectory() / "logs" / "renut_gpu_ring.req").string();
  struct stat st;
  if (::stat(req.c_str(), &st) != 0 || dumps >= 20) return;
  ::unlink(req.c_str());
  const auto path = rex::GetAndroidAppDirectory() / "logs" / ("renut_gpu_ring_" + std::to_string(++dumps) + ".txt");
  FILE* f = std::fopen(path.c_str(), "w");
  if (!f) return;
  std::fprintf(f, "anel a pedido no frame %llu: gravados %u (só CPU, a GPU não escreveu nada)\n\n",
               (unsigned long long)frame, index);
  WriteCommands(f, index >= kRing ? index - kRing + 1 : 1, index, kRing, 0, 0);
  std::fclose(f);
  REXLOG_INFO("Renut: anel da GPU a pedido (frame {}, comando {}): {}", frame, index, path.string());
}

}  // namespace

bool On() {
  static const bool on = REXCVAR_GET(swordman_renut_gpu_markers);
  return on;
}

uint32_t Before(plume::RenderCommandList* cmd, const Info& info) {
  if (!On() || !cmd || !Ready()) return 0;
  State& s = S();
  const uint32_t index = s.next.fetch_add(1, std::memory_order_relaxed);
  Entry& e = s.ring[index & (kRing - 1)];
  e.index = index;
  e.frame = g_sa2_game_frames.load(std::memory_order_relaxed);
  e.info = info;
  static const bool cpu_only = REXCVAR_GET(swordman_renut_gpu_markers_cpu);
  if (cpu_only) {
    CheckRingRequest(e.frame, index);
    return index;
  }
  banjo::renut_gpu::record::Marker(cmd, &Write, 0, index);  // o cmd pode ser o proxy do record worker
  return index;
}

void After(plume::RenderCommandList* cmd, uint32_t index) {
  static const bool end = REXCVAR_GET(swordman_renut_gpu_markers_end) && !REXCVAR_GET(swordman_renut_gpu_markers_cpu);
  if (!index || !cmd || !end) return;
  banjo::renut_gpu::record::Marker(cmd, &Write, 4, index);
}

void OnDeviceLost() {
  static std::once_flag once;
  std::call_once(once, [] {
    // Zandatsu (27/10): o Renut deixa de submeter e o app volta ao launcher (app/src/main/cpp/gpu_reset.cpp).
    swordman::gpu_reset::Trigger("o Vulkan perdeu o dispositivo (sonda do Renut)");
    if (!On() || !Ready()) return;
    State& s = S();
    const uint32_t started = s.words[0], finished = s.words[1];
    const uint32_t recorded = s.next.load() - 1;
    const auto path = rex::GetAndroidAppDirectory() / "logs" / "renut_gpu_hang.txt";
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return;
    std::fprintf(f, "GPU: o último comando que começou foi o %u e o último que acabou foi o %u; gravados %u\n",
                 started, finished, recorded);
    std::fprintf(f, "(o hang está entre 'acabou' e 'começou': os comandos marcados com > ainda não tinham acabado)\n\n");
    // Sem o marcador do fim (swordman_renut_gpu_markers_end = false) "acabou" fica 0: a janela é à volta do início.
    const uint32_t base = finished ? finished : started;
    uint32_t from = base > 20 ? base - 20 : 1;
    uint32_t limit = 200;
    // Só CPU: a GPU não escreveu nada; os últimos 16384 comandos gravados (cruzar com o snapshot do kgsl).
    if (REXCVAR_GET(swordman_renut_gpu_markers_cpu)) {
      from = recorded > 16384 ? recorded - 16383 : 1;
      limit = 16384;
    }
    const uint32_t to = REXCVAR_GET(swordman_renut_gpu_markers_cpu) ? recorded
                        : (started + 20 < recorded ? started + 20 : recorded);
    WriteCommands(f, from, to, limit, started, finished);
    std::fclose(f);
    REXLOG_ERROR("Renut: GPU parou entre os comandos {} (acabou) e {} (começou): {}", finished, started,
                 path.string());
  });
}

}  // namespace swordman::gpu_markers
