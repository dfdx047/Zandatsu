// Marcadores de GPU por draw no Renut (swordman_renut_gpu_markers, desligado por padrão), para encontrar o draw em
// que a GPU pendura (VK_ERROR_DEVICE_LOST, "kgsl: GPU hang detected"). Chamados pelo Renut (patch renut/0014).
//
// Antes e depois de cada draw, clear, resolve e scan-out, a GPU escreve o índice do comando num buffer
// HOST_VISIBLE|HOST_COHERENT mapeado (vkCmdWriteBufferMarkerAMD: "começou" no início do pipeline, "acabou" no fim),
// que continua legível depois do device lost. Do lado da CPU fica um anel com o que cada índice desenhou. No device
// lost escreve logs/renut_gpu_hang.txt com os comandos à volta dos últimos índices escritos pela GPU.
#pragma once

#include <cstdint>

namespace plume {
struct RenderCommandList;
}

namespace swordman::gpu_markers {

enum Kind : uint32_t { kDraw = 1, kClear = 2, kResolve = 3, kScanOut = 4, kMip = 5 };

struct Info {
  uint32_t kind = kDraw;
  uint64_t ps = 0, vs = 0;  // hashes dos shaders do jogo (0 = nenhum / do Renut)
  uint32_t color_va = 0, color_width = 0, color_height = 0, color_format = 0;
  uint32_t depth_va = 0, depth_format = 0;
  uint32_t primitive = 0, count = 0, instances = 0;
};

// swordman_renut_gpu_markers (lido uma vez).
bool On();
// Antes do comando: devolve o seu índice (0 = marcadores desligados ou sem VK_AMD_buffer_marker).
uint32_t Before(plume::RenderCommandList* cmd, const Info& info);
// Depois do comando.
void After(plume::RenderCommandList* cmd, uint32_t index);
// No device lost (uma vez): logs/renut_gpu_hang.txt.
void OnDeviceLost();

}  // namespace swordman::gpu_markers
