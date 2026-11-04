// zdiag: diagnostico Vulkan autonomo (libzdiag.so e zdiag_cli).
//
// Nao depende do jogo nem do SDK. O Vulkan e carregado em tempo de execucao (driver do sistema
// ou um driver alternativo via libadrenotools) e cada teste cria a sua propria instancia/device.
// Cada teste escreve <outDir>/<teste>.json e, antes de cada passo, <outDir>/<teste>.progress
// (para o processo pai saber onde o driver fechou o processo).
#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace zdiag {

//-----------------------------------------------------------------------------
// JSON (escritor em fluxo, virgulas automaticas)
//-----------------------------------------------------------------------------
class Json {
 public:
  Json& obj() { sep(); s_ += '{'; st_.push_back({'}', true}); return *this; }
  Json& arr() { sep(); s_ += '['; st_.push_back({']', true}); return *this; }
  Json& end() { s_ += st_.back().close; st_.pop_back(); return *this; }
  // Fecha tudo o que ficou aberto (um teste que lancou a meio).
  Json& end_all() { if (after_key_) null(); while (!st_.empty()) end(); return *this; }
  Json& k(std::string_view key) { sep(); str(key); s_ += ':'; after_key_ = true; return *this; }
  Json& v(std::string_view x) { sep(); str(x); return *this; }
  Json& v(const char* x) { return x ? v(std::string_view(x)) : null(); }
  Json& v(const std::string& x) { return v(std::string_view(x)); }
  Json& null() { sep(); s_ += "null"; return *this; }
  Json& raw(std::string_view x) { sep(); s_ += x; return *this; }
  template <class T>
    requires std::is_arithmetic_v<T>
  Json& v(T x) {
    sep();
    if constexpr (std::is_same_v<T, bool>) s_ += x ? "true" : "false";
    else if constexpr (std::is_floating_point_v<T>) num(double(x));
    else if constexpr (std::is_signed_v<T>) s_ += std::to_string((long long)x);
    else s_ += std::to_string((unsigned long long)x);
    return *this;
  }
  template <class T, size_t N>
  Json& v(const T (&a)[N]) {
    if constexpr (std::is_same_v<std::remove_cv_t<T>, char>) return v(std::string_view(a, strnlen(a, N)));
    else {
      arr();
      for (const T& x : a) v(x);
      return end();
    }
  }
  template <class T>
  Json& kv(std::string_view key, const T& x) { return k(key).v(x); }
  Json& hex(std::string_view key, uint64_t x);
  const std::string& str() const { return s_; }
  bool empty() const { return s_.empty(); }

 private:
  struct Level { char close; bool first; };
  void sep() {
    if (after_key_) { after_key_ = false; return; }
    if (!st_.empty()) {
      if (!st_.back().first) s_ += ',';
      st_.back().first = false;
    }
  }
  void str(std::string_view x);
  void num(double x);
  std::string s_;
  std::vector<Level> st_;
  bool after_key_ = false;
};

//-----------------------------------------------------------------------------
// Tabela de funcoes Vulkan (carregadas via vkGetInstanceProcAddr / vkGetDeviceProcAddr)
//-----------------------------------------------------------------------------
#define ZD_VK_GLOBAL(X) \
  X(CreateInstance) X(EnumerateInstanceExtensionProperties) X(EnumerateInstanceLayerProperties) \
  X(EnumerateInstanceVersion)

#define ZD_VK_INSTANCE(X) \
  X(DestroyInstance) X(EnumeratePhysicalDevices) X(GetPhysicalDeviceProperties) \
  X(GetPhysicalDeviceProperties2) X(GetPhysicalDeviceFeatures) X(GetPhysicalDeviceFeatures2) \
  X(GetPhysicalDeviceFormatProperties) X(GetPhysicalDeviceImageFormatProperties) \
  X(GetPhysicalDeviceMemoryProperties) X(GetPhysicalDeviceQueueFamilyProperties) \
  X(EnumerateDeviceExtensionProperties) X(CreateDevice) X(GetDeviceProcAddr) \
  X(GetPhysicalDeviceCalibrateableTimeDomainsEXT)

#define ZD_VK_DEVICE(X) \
  X(DestroyDevice) X(GetDeviceQueue) X(DeviceWaitIdle) X(QueueSubmit) X(QueueWaitIdle) \
  X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers) X(FreeCommandBuffers) \
  X(BeginCommandBuffer) X(EndCommandBuffer) X(CreateFence) X(DestroyFence) X(WaitForFences) \
  X(CreateBuffer) X(DestroyBuffer) X(CreateImage) X(DestroyImage) X(CreateImageView) X(DestroyImageView) \
  X(GetBufferMemoryRequirements) X(GetImageMemoryRequirements) X(AllocateMemory) X(FreeMemory) \
  X(BindBufferMemory) X(BindImageMemory) X(MapMemory) X(UnmapMemory) X(InvalidateMappedMemoryRanges) \
  X(FlushMappedMemoryRanges) X(CreateShaderModule) X(DestroyShaderModule) X(CreatePipelineLayout) \
  X(DestroyPipelineLayout) X(CreateGraphicsPipelines) X(CreateComputePipelines) X(DestroyPipeline) \
  X(CreateRenderPass) X(DestroyRenderPass) X(CreateFramebuffer) X(DestroyFramebuffer) \
  X(CreateDescriptorSetLayout) X(DestroyDescriptorSetLayout) X(CreateDescriptorPool) \
  X(DestroyDescriptorPool) X(AllocateDescriptorSets) X(UpdateDescriptorSets) X(CreateSampler) \
  X(DestroySampler) X(CreateQueryPool) X(DestroyQueryPool) X(GetQueryPoolResults) \
  X(CmdBeginRenderPass) X(CmdEndRenderPass) X(CmdBindPipeline) X(CmdSetViewport) X(CmdSetScissor) \
  X(CmdDraw) X(CmdDispatch) X(CmdPushConstants) X(CmdBindDescriptorSets) X(CmdPipelineBarrier) \
  X(CmdCopyImageToBuffer) X(CmdCopyBufferToImage) X(CmdCopyBuffer) X(CmdFillBuffer) \
  X(CmdClearColorImage) X(CmdClearDepthStencilImage) X(CmdClearAttachments) X(CmdWriteTimestamp) \
  X(CmdResetQueryPool) X(GetBufferDeviceAddress) X(CmdBeginRendering) X(CmdEndRendering) \
  X(GetCalibratedTimestampsEXT)

struct VkFns {
  PFN_vkGetInstanceProcAddr GetInstanceProcAddr = nullptr;
#define ZD_DECL(n) PFN_vk##n n = nullptr;
  ZD_VK_GLOBAL(ZD_DECL) ZD_VK_INSTANCE(ZD_DECL) ZD_VK_DEVICE(ZD_DECL)
#undef ZD_DECL
};
extern VkFns vk;

struct LoaderInfo {
  std::string nativeLibDir, driverDir, driverName;
};
// Carrega a libvulkan (uma vez por processo). false + erro em lastLoaderError().
bool LoadVulkan(const LoaderInfo& li);
void LoadInstanceFns(VkInstance inst);
void LoadDeviceFns(VkDevice dev);
void WriteLoaderJson(Json& j);  // {"library": "system|custom", ...}
const std::string& LastLoaderError();

const char* ResultName(VkResult r);

// Falha Vulkan: lancada por VKC(); apanhada pelo passo (que fica "fail").
struct VkFail : std::runtime_error {
  VkResult result;
  VkFail(VkResult r, const std::string& what) : std::runtime_error(what + " -> " + ResultName(r)), result(r) {}
};
void NoteResult(VkResult r);  // marca device perdido / GPU presa
#define VKC(expr)                                                                \
  do {                                                                           \
    VkResult zd_r_ = (expr);                                                     \
    if (zd_r_ != VK_SUCCESS) { ::zdiag::NoteResult(zd_r_); throw ::zdiag::VkFail(zd_r_, #expr); } \
  } while (0)

//-----------------------------------------------------------------------------
// Opcoes "chave=valor;chave=valor"
//-----------------------------------------------------------------------------
struct Options {
  std::map<std::string, std::string> kv;
  static Options Parse(std::string_view s);
  std::string str(const std::string& k, const std::string& def = "") const;
  long num(const std::string& k, long def) const;
};

//-----------------------------------------------------------------------------
// Contexto de um teste: progresso, passos, resultado
//-----------------------------------------------------------------------------
enum class St { Pass, Fail, Skip };
const char* StName(St s);

class Test {
 public:
  Test(std::string id, std::string out_dir, Options opt);
  const std::string id, outDir;
  const Options opt;

  // Escreve o nome do sub-passo em <outDir>/<id>.progress (antes de o executar) e aplica
  // force_crash=<id>/<passo>. Thread-safe.
  void Progress(const std::string& step);
  // Corre um passo: fn(info) devolve o estado; excecoes = fail (com "error" no info).
  St Step(const std::string& name, const std::function<St(Json& info)>& fn);
  St SkipStep(const std::string& name, const std::string& reason);

  Json details;         // objeto aberto pelo executor; o teste acrescenta chaves
  std::string message;  // resumo curto para humanos
  int passes = 0, fails = 0, skips = 0;
  std::string steps_json() const { return steps_.str(); }
  void Finish();  // fecha arrays/objetos
  // PNG <outDir>/<id>_<nome>.png (rgba: 4 canais por pixel, stride em bytes); devolve o nome do ficheiro.
  std::string Png(const std::string& name, const uint8_t* rgba, int w, int h, int stride, int scale,
                  bool keep_alpha = false);
  bool gpu_dead = false;  // device perdido / timeout: os passos seguintes ficam "skip"
  std::string gpu_dead_reason;

 private:
  Json steps_;
  std::mutex progress_mu_;
  std::string progress_path_, crash_at_;
  bool fsync_ = true;
};

using TestFn = void (*)(Test&);
struct TestDef {
  const char* id;
  TestFn fn;
};
const std::vector<TestDef>& AllTests();
// Corre um teste (o processo inteiro, se o driver fechar). Devolve o JSON do resultado.
std::string RunTest(const std::string& id, const std::string& out_dir, const LoaderInfo& li,
                    const std::string& options, bool in_process = false);
std::string TestsJson();

void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
double NowMs();

// PNG (zlib)
bool WritePng(const std::string& path, int w, int h, int channels, const uint8_t* px);

// Shaders SPIR-V embutidos (compilados com glslc no build)
struct Spirv {
  const char* name;
  const uint32_t* code;
  size_t bytes;
};
const Spirv& FindShader(const char* name);  // lanca se nao existir
const Spirv* BcDecodeShader();              // o do Renut (nullptr se o build nao o tiver)

// Testes
void TestInfo(Test& t);
void TestPipelinesSerial(Test& t);
void TestPipelinesParallel(Test& t);
void TestViewportScissor(Test& t);
void TestClearLoadOp(Test& t);
void TestBcDecode(Test& t);
void TestTimestamps(Test& t);
void TestCpuCores(Test& t);

// Perfil de CPU do processo do jogo (profiler.cpp): uma thread que escreve em dir ate o processo acabar.
bool StartProfiler(const std::string& dir);

}  // namespace zdiag
