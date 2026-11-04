// zdiag: pipelines_serial / pipelines_parallel — 96 pipelines graficos distintos sem VkPipelineCache
// (o compilador do driver corre a serio) + 4 de compute. O paralelo reproduz a corrida do driver PowerVR
// (Scudo "invalid chunk state" no vulkan.mtk.so com varias threads a criar pipelines).
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <thread>

#include "gpu.h"

namespace zdiag {
namespace {

constexpr int kFrags = 8, kSpecs = 4, kStates = 3, kPipelines = kFrags * kSpecs * kStates;
const char* kVert[kStates] = {"pl_basic.vert", "pl_full.vert", "pl_inst.vert"};
const char* kFrag[kFrags] = {"pl_f0.frag", "pl_f1.frag", "pl_f2.frag", "pl_f3.frag",
                             "pl_f4.frag", "pl_f5.frag", "pl_f6.frag", "pl_f7.frag"};
const int32_t kSpecVals[kSpecs][2] = {{0, 4}, {1, 8}, {2, 16}, {3, 32}};  // MODE, ITER

struct PipeIdx {
  int frag, spec, state;
};
PipeIdx Decode(int i) { return {i / (kSpecs * kStates), (i / kStates) % kSpecs, i % kStates}; }
std::string PipeName(int i) {
  PipeIdx p = Decode(i);
  return "f" + std::to_string(p.frag) + "_s" + std::to_string(p.spec) + "_st" + std::to_string(p.state);
}

struct Setup {
  VkRenderPass rp = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkFormat depth = VK_FORMAT_UNDEFINED;
  VkShaderModule vert[kStates]{}, frag[kFrags]{};
  VkDescriptorSetLayout compute_set = VK_NULL_HANDLE;
  VkPipelineLayout compute_layout = VK_NULL_HANDLE;
};

void MakeSetup(Gpu& g, Setup& s) {
  s.depth = PickDepthFormat(g);
  s.rp = g.RenderPass(VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_UNDEFINED, s.depth);
  VkDescriptorSetLayoutBinding b[2] = {
      {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
      {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
  VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  ci.bindingCount = 2;
  ci.pBindings = b;
  VkDescriptorSetLayout dsl;
  VKC(vk.CreateDescriptorSetLayout(g.dev, &ci, nullptr, &dsl));
  VkDevice dev = g.dev;
  g.Defer([dev, dsl] { vk.DestroyDescriptorSetLayout(dev, dsl, nullptr); });
  s.layout = g.Layout({dsl}, 64, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

  VkDescriptorSetLayoutBinding cb{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
  ci.bindingCount = 1;
  ci.pBindings = &cb;
  VKC(vk.CreateDescriptorSetLayout(g.dev, &ci, nullptr, &s.compute_set));
  VkDescriptorSetLayout cs = s.compute_set;
  g.Defer([dev, cs] { vk.DestroyDescriptorSetLayout(dev, cs, nullptr); });
  s.compute_layout = g.Layout({s.compute_set}, 8, VK_SHADER_STAGE_COMPUTE_BIT);
}

VkShaderModule RawModule(VkDevice dev, const char* name, VkResult* r) {
  const Spirv& sp = FindShader(name);
  VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  ci.codeSize = sp.bytes;
  ci.pCode = sp.code;
  VkShaderModule m = VK_NULL_HANDLE;
  *r = vk.CreateShaderModule(dev, &ci, nullptr, &m);
  return m;
}

// Cria o pipeline i (estado e layout de vertices por 'state'). Thread-safe (so le 's').
VkResult CreateGraphics(VkDevice dev, const Setup& s, int i, VkShaderModule vs, VkShaderModule fs, VkPipeline* out) {
  const PipeIdx p = Decode(i);
  VkSpecializationMapEntry me[2] = {{0, 0, 4}, {1, 4, 4}};
  VkSpecializationInfo spec{2, me, 8, kSpecVals[p.spec]};
  VkPipelineShaderStageCreateInfo st[2]{};
  st[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main", &spec};
  st[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", &spec};

  // Layouts de vertices: A = so posicao; B = pos/uv/cor/normal intercalados; C = pos + atributo por instancia.
  VkVertexInputBindingDescription bind[2];
  VkVertexInputAttributeDescription attr[4];
  VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vi.pVertexBindingDescriptions = bind;
  vi.pVertexAttributeDescriptions = attr;
  if (p.state == 0) {
    bind[0] = {0, 12, VK_VERTEX_INPUT_RATE_VERTEX};
    attr[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
    vi.vertexBindingDescriptionCount = 1;
    vi.vertexAttributeDescriptionCount = 1;
  } else if (p.state == 1) {
    bind[0] = {0, 32, VK_VERTEX_INPUT_RATE_VERTEX};
    attr[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
    attr[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, 12};
    attr[2] = {2, 0, VK_FORMAT_R8G8B8A8_UNORM, 20};
    attr[3] = {3, 0, VK_FORMAT_R8G8B8A8_SNORM, 24};
    vi.vertexBindingDescriptionCount = 1;
    vi.vertexAttributeDescriptionCount = 4;
  } else {
    bind[0] = {0, 12, VK_VERTEX_INPUT_RATE_VERTEX};
    bind[1] = {1, 16, VK_VERTEX_INPUT_RATE_INSTANCE};
    attr[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
    attr[1] = {1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0};
    vi.vertexBindingDescriptionCount = 2;
    vi.vertexAttributeDescriptionCount = 2;
  }
  VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  ia.topology = p.state == 1 ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  vp.viewportCount = vp.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = p.state == 0 ? VK_CULL_MODE_BACK_BIT : p.state == 2 ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_NONE;
  rs.frontFace = p.state == 2 ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rs.depthBiasEnable = p.state == 1;
  rs.depthBiasConstantFactor = 1.0f;
  rs.depthBiasSlopeFactor = 1.5f;
  rs.lineWidth = 1.0f;
  VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  ds.depthTestEnable = p.state != 2;
  ds.depthWriteEnable = p.state == 0;
  ds.depthCompareOp = p.state == 0 ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_LESS_OR_EQUAL;
  ds.maxDepthBounds = 1.0f;
  VkPipelineColorBlendAttachmentState ba{};
  ba.colorWriteMask = p.state == 2 ? 0x7 : 0xF;
  if (p.state == 1) {
    ba.blendEnable = VK_TRUE;
    ba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ba.colorBlendOp = VK_BLEND_OP_ADD;
    ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ba.alphaBlendOp = VK_BLEND_OP_ADD;
  } else if (p.state == 2) {
    ba.blendEnable = VK_TRUE;
    ba.srcColorBlendFactor = ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    ba.srcAlphaBlendFactor = ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  }
  VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  cb.attachmentCount = 1;
  cb.pAttachments = &ba;
  VkDynamicState dyn[3] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_BLEND_CONSTANTS};
  VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dy.dynamicStateCount = p.state == 1 ? 3 : 2;
  dy.pDynamicStates = dyn;
  VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  ci.stageCount = 2;
  ci.pStages = st;
  ci.pVertexInputState = &vi;
  ci.pInputAssemblyState = &ia;
  ci.pViewportState = &vp;
  ci.pRasterizationState = &rs;
  ci.pMultisampleState = &ms;
  ci.pDepthStencilState = &ds;
  ci.pColorBlendState = &cb;
  ci.pDynamicState = &dy;
  ci.layout = s.layout;
  ci.renderPass = s.rp;
  *out = VK_NULL_HANDLE;
  return vk.CreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &ci, nullptr, out);
}

struct Timing {
  std::vector<double> ms;
  std::vector<VkResult> res;
  double wall = 0, module_ms = 0;
  explicit Timing(int n) : ms(n, 0.0), res(n, VK_SUCCESS) {}
};

double R2(double x) { return std::round(x * 100) / 100; }

// Estatisticas de uma ronda: em info do passo e em details.<key>.
int StatsJson(Json& j, const Timing& tm, const std::vector<std::string>& names) {
  std::vector<double> sorted = tm.ms;
  std::sort(sorted.begin(), sorted.end());
  double sum = 0;
  for (double x : tm.ms) sum += x;
  const size_t n = sorted.size();
  j.kv("count", n).kv("wallMs", R2(tm.wall)).kv("sumMs", R2(sum)).kv("avgMs", R2(n ? sum / n : 0))
      .kv("minMs", R2(n ? sorted.front() : 0)).kv("p95Ms", R2(n ? sorted[std::min(n - 1, size_t(n * 0.95))] : 0))
      .kv("maxMs", R2(n ? sorted.back() : 0));
  if (tm.module_ms > 0) j.kv("moduleSumMs", R2(tm.module_ms));
  std::vector<size_t> order(n);
  for (size_t i = 0; i < n; ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return tm.ms[a] > tm.ms[b]; });
  j.k("slowest").arr();
  for (size_t i = 0; i < std::min<size_t>(5, n); ++i)
    j.obj().kv("name", names[order[i]]).kv("ms", R2(tm.ms[order[i]])).end();
  j.end();
  int fails = 0;
  j.k("failures").arr();
  for (size_t i = 0; i < n; ++i)
    if (tm.res[i] != VK_SUCCESS) {
      fails++;
      j.obj().kv("index", i).kv("name", names[i]).kv("result", ResultName(tm.res[i])).end();
    }
  j.end();
  return fails;
}

std::vector<std::string> GraphicsNames() {
  std::vector<std::string> v;
  for (int i = 0; i < kPipelines; ++i) v.push_back(PipeName(i));
  return v;
}

// Compute: workload MODE 0/1, reduce com grupos de 64 e 256 (ou o maximo do device).
constexpr int kComputes = 4;
const char* kComputeNames[kComputes] = {"workload_m0", "workload_m1", "reduce_64", "reduce_big"};

VkResult CreateCompute(Gpu& g, const Setup& s, int i, VkShaderModule work, VkShaderModule red, VkPipeline* out) {
  uint32_t wg_big = std::min<uint32_t>(256, std::min(g.props.limits.maxComputeWorkGroupSize[0],
                                                     g.props.limits.maxComputeWorkGroupInvocations));
  const uint32_t val = i < 2 ? uint32_t(i) : i == 2 ? 64u : wg_big;
  VkSpecializationMapEntry me{0, 0, 4};
  VkSpecializationInfo spec{1, &me, 4, &val};
  VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  ci.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT,
              i < 2 ? work : red, "main", &spec};
  ci.layout = s.compute_layout;
  *out = VK_NULL_HANDLE;
  return vk.CreateComputePipelines(g.dev, VK_NULL_HANDLE, 1, &ci, nullptr, out);
}

int ThreadCount(const Test& t) {
  long cores = sysconf(_SC_NPROCESSORS_ONLN);
  int n = int(std::clamp<long>(cores / 2, 2, 4));
  return int(std::clamp<long>(t.opt.num("threads", n), 1, 16));
}

void DestroyAll(VkDevice dev, std::vector<VkPipeline>& p) {
  for (VkPipeline x : p)
    if (x) vk.DestroyPipeline(dev, x, nullptr);
  p.clear();
}

// Uma ronda de 96 pipelines graficos com 'threads' threads. own_modules: cada pipeline cria os seus
// VkShaderModule (tambem em paralelo) em vez de usar os partilhados.
Timing GraphicsRound(Gpu& g, Test& t, const Setup& s, const std::string& prefix, int threads, bool own_modules) {
  Timing tm(kPipelines);
  std::vector<VkPipeline> pipes(kPipelines, VK_NULL_HANDLE);
  std::vector<VkShaderModule> mods(kPipelines * 2, VK_NULL_HANDLE);
  std::vector<double> mod_ms(kPipelines, 0.0);
  std::atomic<int> next{0};
  auto worker = [&] {
    for (int i; (i = next.fetch_add(1)) < kPipelines;) {
      t.Progress(prefix + "/pipeline_" + std::to_string(i));
      if (t.gpu_dead) break;
      VkShaderModule vs = s.vert[Decode(i).state], fs = s.frag[Decode(i).frag];
      if (own_modules) {
        double m0 = NowMs();
        VkResult r1, r2;
        vs = mods[2 * i] = RawModule(g.dev, kVert[Decode(i).state], &r1);
        fs = mods[2 * i + 1] = RawModule(g.dev, kFrag[Decode(i).frag], &r2);
        mod_ms[i] = NowMs() - m0;
        if (r1 != VK_SUCCESS || r2 != VK_SUCCESS) {
          tm.res[i] = r1 != VK_SUCCESS ? r1 : r2;
          continue;
        }
      }
      double t0 = NowMs();
      tm.res[i] = CreateGraphics(g.dev, s, i, vs, fs, &pipes[i]);
      tm.ms[i] = NowMs() - t0;
      NoteResult(tm.res[i]);
    }
  };
  double w0 = NowMs();
  if (threads <= 1) {
    worker();
  } else {
    std::vector<std::thread> th;
    for (int k = 0; k < threads; ++k) th.emplace_back(worker);
    for (auto& x : th) x.join();
  }
  tm.wall = NowMs() - w0;
  for (double x : mod_ms) tm.module_ms += x;
  t.Progress(prefix + "/destroy");
  DestroyAll(g.dev, pipes);
  for (VkShaderModule m : mods)
    if (m) vk.DestroyShaderModule(g.dev, m, nullptr);
  return tm;
}

void RunPipelines(Test& t, bool parallel) {
  Gpu g(t);
  Setup s;
  const int threads = parallel ? ThreadCount(t) : 1;
  if (t.Step("setup", [&](Json& info) {
        g.CreateInstance();
        g.CreateDevice({});
        MakeSetup(g, s);
        info.kv("device", g.props.deviceName).kv("threads", threads).kv("pipelines", kPipelines)
            .kv("depthFormat", int(s.depth));
        return St::Pass;
      }) != St::Pass) {
    t.message = "setup falhou";
    return;
  }
  t.details.kv("threads", threads).kv("pipelines", kPipelines).kv("computePipelines", kComputes)
      .kv("pipelineCache", false);

  t.Step("modules", [&](Json& info) {
    double t0 = NowMs();
    for (int i = 0; i < kStates; ++i) {
      t.Progress(std::string("modules/") + kVert[i]);
      s.vert[i] = g.Shader(kVert[i]);
    }
    for (int i = 0; i < kFrags; ++i) {
      t.Progress(std::string("modules/") + kFrag[i]);
      s.frag[i] = g.Shader(kFrag[i]);
    }
    info.kv("count", kStates + kFrags).kv("ms", R2(NowMs() - t0));
    return St::Pass;
  });

  const auto names = GraphicsNames();
  std::vector<std::pair<std::string, double>> summary;
  auto round = [&](const std::string& name, int nthreads, bool own) {
    t.Step(name, [&](Json& info) {
      Timing tm = GraphicsRound(g, t, s, name, nthreads, own);
      info.kv("threads", nthreads).kv("ownModules", own);
      int fails = StatsJson(info, tm, names);
      t.details.k(name).obj().kv("threads", nthreads).kv("ownModules", own);
      StatsJson(t.details, tm, names);
      t.details.end();
      double sum = 0;
      for (double x : tm.ms) sum += x;
      summary.push_back({name, tm.wall});
      info.kv("avgPerPipelineMs", R2(sum / kPipelines));
      return fails ? St::Fail : St::Pass;
    });
  };
  if (parallel) {
    round("round1", threads, false);
    round("round2", threads, true);
  } else {
    round("serial", 1, false);
  }

  t.Step("compute", [&](Json& info) {
    VkShaderModule work = g.Shader("workload.comp"), red = g.Shader("reduce.comp");
    Timing tm(kComputes);
    std::vector<VkPipeline> pipes(kComputes, VK_NULL_HANDLE);
    auto one = [&](int i) {
      t.Progress(std::string("compute/") + kComputeNames[i]);
      double t0 = NowMs();
      tm.res[i] = CreateCompute(g, s, i, work, red, &pipes[i]);
      tm.ms[i] = NowMs() - t0;
    };
    double w0 = NowMs();
    if (parallel) {
      std::vector<std::thread> th;
      for (int i = 0; i < kComputes; ++i) th.emplace_back(one, i);
      for (auto& x : th) x.join();
    } else {
      for (int i = 0; i < kComputes; ++i) one(i);
    }
    tm.wall = NowMs() - w0;
    DestroyAll(g.dev, pipes);
    std::vector<std::string> cn(kComputeNames, kComputeNames + kComputes);
    int fails = StatsJson(info, tm, cn);
    t.details.k("compute").obj();
    StatsJson(t.details, tm, cn);
    t.details.end();
    return fails ? St::Fail : St::Pass;
  });

  char msg[256];
  std::string m;
  for (auto& [n, w] : summary) {
    snprintf(msg, sizeof(msg), "%s%s %d pipelines em %.0f ms", m.empty() ? "" : "; ", n.c_str(), kPipelines, w);
    m += msg;
  }
  if (parallel) m += " (" + std::to_string(threads) + " threads)";
  if (t.fails) m += "; " + std::to_string(t.fails) + " passo(s) falharam";
  t.message = m;
}

}  // namespace

void TestPipelinesSerial(Test& t) { RunPipelines(t, false); }
void TestPipelinesParallel(Test& t) { RunPipelines(t, true); }

}  // namespace zdiag
