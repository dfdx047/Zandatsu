// Marcadores de GPU por draw no Renut: ver renut_gpu_markers.h.

#include "renut_gpu_markers.h"

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
  banjo::renut_gpu::record::Marker(cmd, &Write, 0, index);  // o cmd pode ser o proxy do record worker
  return index;
}

void After(plume::RenderCommandList* cmd, uint32_t index) {
  if (!index || !cmd) return;
  banjo::renut_gpu::record::Marker(cmd, &Write, 4, index);
}

void OnDeviceLost() {
  static std::once_flag once;
  std::call_once(once, [] {
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
    std::fprintf(f, "  índice    frame  tipo      PS                VS                cor (va wxh fmt)          "
                    "profund. (va fmt)  prim  contagem inst\n");
    const uint32_t from = finished > 20 ? finished - 20 : 1;
    const uint32_t to = started + 20 < recorded ? started + 20 : recorded;
    for (uint32_t i = from; i <= to && i - from < 200; ++i) {
      const Entry& e = s.ring[i & (kRing - 1)];
      if (e.index != i) continue;
      const Info& n = e.info;
      const char* mark = (i > finished && i <= started) ? ">" : " ";
      std::fprintf(f, "%s %8u %8llu  %-8s  %016llX  %016llX  %08X %4ux%-4u fmt %-3u  %08X fmt %-3u  %4u  %8u %4u\n", mark,
                   i, (unsigned long long)e.frame, KindName(n.kind), (unsigned long long)n.ps,
                   (unsigned long long)n.vs, n.color_va, n.color_width, n.color_height, n.color_format, n.depth_va,
                   n.depth_format, n.primitive, n.count, n.instances);
    }
    std::fclose(f);
    REXLOG_ERROR("Renut: GPU parou entre os comandos {} (acabou) e {} (começou): {}", finished, started,
                 path.string());
  });
}

}  // namespace swordman::gpu_markers
