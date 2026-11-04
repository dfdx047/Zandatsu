// zdiag: instancia/device por teste e ajudas Vulkan (buffers, imagens, passes, leitura de volta).
#pragma once

#include "zdiag.h"

namespace zdiag {

struct DeviceReq {
  bool bda = false;           // bufferDeviceAddress (+ shaderInt64 se existir)
  bool dynamicRendering = false;
  bool calibrated = false;    // VK_EXT/KHR_calibrated_timestamps
  bool bc = false;            // textureCompressionBC
};

struct Buffer {
  VkBuffer buf = VK_NULL_HANDLE;
  VkDeviceMemory mem = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
  uint8_t* map = nullptr;
  bool coherent = true;
  VkDeviceAddress addr = 0;
};

struct Image {
  VkImage img = VK_NULL_HANDLE;
  VkDeviceMemory mem = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkFormat format = VK_FORMAT_UNDEFINED;
  uint32_t w = 0, h = 0;
};

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
};

class Gpu {
 public:
  explicit Gpu(Test& t) : t_(t) {}
  ~Gpu();
  Gpu(const Gpu&) = delete;

  void CreateInstance();
  void CreateDevice(const DeviceReq& req);

  VkInstance inst = VK_NULL_HANDLE;
  VkPhysicalDevice pd = VK_NULL_HANDLE;
  VkDevice dev = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  uint32_t qfam = 0;
  uint32_t instanceVersion = VK_API_VERSION_1_0, apiVersion = VK_API_VERSION_1_0;
  VkPhysicalDeviceProperties props{};
  VkPhysicalDeviceFeatures feats{};
  VkPhysicalDeviceMemoryProperties memProps{};
  std::vector<VkExtensionProperties> devExts;
  bool HasExt(const char* name) const;
  bool props2 = false;  // vkGetPhysicalDeviceProperties2/Features2 disponiveis
  // Suportado / ligado no device
  bool hasBda = false, hasInt64 = false, hasDynRendering = false, hasCalibrated = false, hasBc = false;
  VkCommandPool pool = VK_NULL_HANDLE;
  uint64_t timeoutNs = 10'000'000'000ull;

  // Lixeira: cada objeto criado regista o seu destrutor; Release(mark) destroi ate a marca.
  size_t Mark() const { return trash_.size(); }
  void Release(size_t mark);
  void Defer(std::function<void()> f) { trash_.push_back(std::move(f)); }

  uint32_t MemType(uint32_t bits, VkMemoryPropertyFlags want, VkMemoryPropertyFlags fallback) const;
  Buffer MakeBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool host, bool device_address = false,
                    bool readback = false);
  Image MakeImage(uint32_t w, uint32_t h, VkFormat fmt, VkImageUsageFlags usage,
                  VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
  VkShaderModule Shader(const char* name);
  VkShaderModule ShaderFromWords(const uint32_t* code, size_t bytes);
  VkRenderPass RenderPass(VkFormat color, VkAttachmentLoadOp load, VkImageLayout initial,
                          VkFormat depth = VK_FORMAT_UNDEFINED,
                          VkAttachmentLoadOp depth_load = VK_ATTACHMENT_LOAD_OP_CLEAR);
  VkFramebuffer Framebuffer(VkRenderPass rp, const Image& color, const Image* depth = nullptr);
  VkPipelineLayout Layout(const std::vector<VkDescriptorSetLayout>& sets, uint32_t push_bytes,
                          VkShaderStageFlags push_stages);
  VkPipeline ComputePipeline(VkShaderModule mod, VkPipelineLayout layout,
                             const VkSpecializationInfo* spec = nullptr);

  // Comandos: Begin() aloca e comeca; Submit() termina, submete (um vkQueueSubmit por grupo) e espera.
  VkCommandBuffer Begin();
  void Submit(const std::vector<VkCommandBuffer>& cbs, bool separate_submits = false);
  double lastSubmitCpuMs = 0;

  void Barrier(VkCommandBuffer cb, VkImage img, VkImageLayout from, VkImageLayout to,
               VkPipelineStageFlags src_stage, VkAccessFlags src_access, VkPipelineStageFlags dst_stage,
               VkAccessFlags dst_access, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
  void MemoryBarrier(VkCommandBuffer cb, VkPipelineStageFlags src_stage, VkAccessFlags src_access,
                     VkPipelineStageFlags dst_stage, VkAccessFlags dst_access);
  // Copia a imagem (no layout 'from', escrita pelo estagio src_stage) para um buffer de leitura.
  Buffer RecordReadback(VkCommandBuffer cb, const Image& im, VkImageLayout from, VkPipelineStageFlags src_stage,
                        VkAccessFlags src_access);
  std::vector<uint8_t> ReadMapped(const Buffer& b);
  void Clear(VkCommandBuffer cb, const Image& im, VkImageLayout from, const float rgba[4],
             VkImageLayout to);

  Test& test() { return t_; }

 private:
  Test& t_;
  std::vector<std::function<void()>> trash_;
};

// RAII: destroi o que foi criado desde a construcao (se a GPU nao estiver presa).
struct Scope {
  Gpu& g;
  size_t mark;
  explicit Scope(Gpu& gpu) : g(gpu), mark(gpu.Mark()) {}
  ~Scope() { g.Release(mark); }
};

// Pixels RGBA8: cor empacotada (r | g<<8 | b<<16 | a<<24).
inline uint32_t Rgba(int r, int g, int b, int a = 255) {
  return uint32_t(r) | uint32_t(g) << 8 | uint32_t(b) << 16 | uint32_t(a) << 24;
}
struct Coverage {
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // caixa dos pixels com a cor (x1/y1 exclusivos); vazia se count=0
  uint64_t count = 0;                  // pixels com a cor dentro da regiao
  uint64_t total = 0;                  // pixels da regiao
};
// Mede os pixels == cor (equal=false: != cor) dentro (inside=true) ou fora da regiao.
Coverage Measure(const std::vector<uint8_t>& px, int w, int h, uint32_t color, Rect region, bool inside = true,
                 bool equal = true);
void CoverageJson(Json& j, const char* key, const Coverage& c);
void RectJson(Json& j, const char* key, Rect r);

// Pipeline do triangulo de ecra inteiro (fs_tri.vert + solid.frag); push constants {vec4 cor; vec4 z}.
struct SolidPipeOpts {
  VkRenderPass rp = VK_NULL_HANDLE;   // ou VK_NULL_HANDLE + color_format para dynamic rendering
  VkFormat color_format = VK_FORMAT_R8G8B8A8_UNORM;
  VkFormat depth_format = VK_FORMAT_UNDEFINED;
  bool depth_test = false;
  const VkViewport* static_viewport = nullptr;  // null = estado dinamico
  const VkRect2D* static_scissor = nullptr;
};
VkPipeline SolidPipeline(Gpu& g, VkPipelineLayout layout, const SolidPipeOpts& o);
VkPipelineLayout SolidLayout(Gpu& g);
void PushSolid(Gpu& g, VkCommandBuffer cb, VkPipelineLayout layout, const float rgba[4], float z = 0.5f);
void SetViewportScissor(VkCommandBuffer cb, Rect vp, Rect sc);

VkFormat PickDepthFormat(Gpu& g);

}  // namespace zdiag
