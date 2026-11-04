// zdiag: timestamps — timestampPeriod/validBits, timestamps a volta de trabalho conhecido na GPU (compute e
// draws), coerencia com o tempo de CPU, e VK_EXT/KHR_calibrated_timestamps se existir.
#include <unistd.h>

#include <algorithm>
#include <cmath>

#include "gpu.h"

namespace zdiag {
namespace {

double R3(double x) { return std::round(x * 1000) / 1000; }

struct Workload {
  VkPipeline pipe = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkDescriptorSet set = VK_NULL_HANDLE;
  uint32_t groups = 1024;
};

void MakeWorkload(Gpu& g, Workload& w) {
  VkDescriptorSetLayoutBinding b{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
  VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  ci.bindingCount = 1;
  ci.pBindings = &b;
  VkDescriptorSetLayout dsl;
  VKC(vk.CreateDescriptorSetLayout(g.dev, &ci, nullptr, &dsl));
  VkDevice dev = g.dev;
  g.Defer([dev, dsl] { vk.DestroyDescriptorSetLayout(dev, dsl, nullptr); });
  w.layout = g.Layout({dsl}, 8, VK_SHADER_STAGE_COMPUTE_BIT);
  w.pipe = g.ComputePipeline(g.Shader("workload.comp"), w.layout);
  Buffer buf = g.MakeBuffer(VkDeviceSize(w.groups) * 64 * 16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);
  VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
  VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  pci.maxSets = 1;
  pci.poolSizeCount = 1;
  pci.pPoolSizes = &ps;
  VkDescriptorPool pool;
  VKC(vk.CreateDescriptorPool(g.dev, &pci, nullptr, &pool));
  g.Defer([dev, pool] { vk.DestroyDescriptorPool(dev, pool, nullptr); });
  VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  ai.descriptorPool = pool;
  ai.descriptorSetCount = 1;
  ai.pSetLayouts = &dsl;
  VKC(vk.AllocateDescriptorSets(g.dev, &ai, &w.set));
  VkDescriptorBufferInfo bi{buf.buf, 0, VK_WHOLE_SIZE};
  VkWriteDescriptorSet wr{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, w.set, 0, 0, 1,
                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bi};
  vk.UpdateDescriptorSets(g.dev, 1, &wr, 0, nullptr);
}

VkQueryPool QueryPool(Gpu& g, uint32_t n) {
  VkQueryPoolCreateInfo ci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
  ci.queryCount = n;
  VkQueryPool qp;
  VKC(vk.CreateQueryPool(g.dev, &ci, nullptr, &qp));
  VkDevice dev = g.dev;
  g.Defer([dev, qp] { vk.DestroyQueryPool(dev, qp, nullptr); });
  return qp;
}

std::vector<uint64_t> Results(Gpu& g, VkQueryPool qp, uint32_t n) {
  std::vector<uint64_t> v(n, 0);
  VKC(vk.GetQueryPoolResults(g.dev, qp, 0, n, n * 8, v.data(), 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
  return v;
}

// Corre 3 dispatches com 4 timestamps (TOP, BOTTOM, BOTTOM, BOTTOM).
std::vector<uint64_t> RunCompute(Gpu& g, Workload& w, VkQueryPool qp, uint32_t iters) {
  VkCommandBuffer cb = g.Begin();
  vk.CmdResetQueryPool(cb, qp, 0, 4);
  vk.CmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, w.pipe);
  vk.CmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, w.layout, 0, 1, &w.set, 0, nullptr);
  uint32_t pc[2] = {iters, w.groups * 64};
  vk.CmdPushConstants(cb, w.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 8, pc);
  vk.CmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, qp, 0);
  for (uint32_t i = 0; i < 3; ++i) {
    vk.CmdDispatch(cb, w.groups, 1, 1);
    vk.CmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, qp, i + 1);
    g.MemoryBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
  }
  g.Submit({cb});
  return Results(g, qp, 4);
}

struct Check {
  bool nonzero = true, monotonic = true;
};
Check Analyse(const std::vector<uint64_t>& ts) {
  Check c;
  for (size_t i = 0; i < ts.size(); ++i) {
    if (!ts[i]) c.nonzero = false;
    if (i && ts[i] < ts[i - 1]) c.monotonic = false;
  }
  return c;
}

void TsJson(Json& j, const std::vector<uint64_t>& ts) {
  j.k("ticks").arr();
  for (uint64_t x : ts) j.v(x);
  j.end();
}

}  // namespace

void TestTimestamps(Test& t) {
  Gpu g(t);
  uint32_t valid_bits = 0;
  if (t.Step("setup", [&](Json& info) {
        g.CreateInstance();
        DeviceReq req;
        req.calibrated = true;
        g.CreateDevice(req);
        uint32_t nq = 0;
        vk.GetPhysicalDeviceQueueFamilyProperties(g.pd, &nq, nullptr);
        std::vector<VkQueueFamilyProperties> qf(nq);
        vk.GetPhysicalDeviceQueueFamilyProperties(g.pd, &nq, qf.data());
        valid_bits = qf[g.qfam].timestampValidBits;
        t.details.kv("timestampPeriod", g.props.limits.timestampPeriod)
            .kv("timestampComputeAndGraphics", bool(g.props.limits.timestampComputeAndGraphics))
            .kv("queueFamily", g.qfam).kv("calibratedTimestamps", g.hasCalibrated);
        t.details.k("timestampValidBits").arr();
        for (auto& q : qf) t.details.v(q.timestampValidBits);
        t.details.end();
        info.kv("device", g.props.deviceName).kv("timestampPeriod", g.props.limits.timestampPeriod)
            .kv("validBits", valid_bits);
        return St::Pass;
      }) != St::Pass) {
    t.message = "setup falhou";
    return;
  }
  if (!valid_bits) {
    t.SkipStep("compute", "timestampValidBits = 0 na fila graphics");
    t.message = "sem timestamps na fila graphics";
    return;
  }
  const double period = g.props.limits.timestampPeriod;
  Workload w;
  VkQueryPool qp = VK_NULL_HANDLE;
  uint32_t iters = 64;
  std::string summary;

  t.Step("compute", [&](Json& info) {
    MakeWorkload(g, w);
    qp = QueryPool(g, 8);
    // Calibra a carga ate ~20 ms de GPU (ou 2 s de CPU).
    std::vector<uint64_t> ts;
    double gpu_ms = 0;
    for (int k = 0; k < 12; ++k) {
      t.Progress("compute/iters_" + std::to_string(iters));
      ts = RunCompute(g, w, qp, iters);
      gpu_ms = double(ts[3] - ts[0]) * period / 1e6;
      if (ts[3] <= ts[0] || gpu_ms >= 20.0 || g.lastSubmitCpuMs > 2000.0 || iters >= (1u << 20)) break;
      double scale = gpu_ms > 0.5 ? std::clamp(25.0 / gpu_ms, 1.5, 16.0) : 16.0;
      iters = uint32_t(std::min<double>(iters * scale, double(1u << 20)));
    }
    t.Progress("compute/final");
    ts = RunCompute(g, w, qp, iters);
    const double cpu_ms = g.lastSubmitCpuMs;
    Check c = Analyse(ts);
    gpu_ms = c.monotonic ? double(ts[3] - ts[0]) * period / 1e6 : 0;
    double d[3], dmin = 1e30, dmax = 0;
    for (int i = 0; i < 3; ++i) {
      d[i] = c.monotonic ? double(ts[i + 1] - ts[i]) * period / 1e6 : 0;
      dmin = std::min(dmin, d[i]);
      dmax = std::max(dmax, d[i]);
    }
    const double ratio = cpu_ms > 0 ? gpu_ms / cpu_ms : 0;
    // A GPU nao pode demorar mais do que o submit+espera (+1 ms de folga), nem muito menos com carga grande.
    const bool sane = gpu_ms > 0 && gpu_ms <= cpu_ms * 1.05 + 1.0 && (cpu_ms < 5.0 || ratio >= 0.2);
    TsJson(info, ts);
    info.kv("iters", iters).kv("nonzero", c.nonzero).kv("monotonic", c.monotonic).kv("gpuMs", R3(gpu_ms))
        .kv("cpuMs", R3(cpu_ms)).kv("ratio", R3(ratio)).kv("sane", sane);
    info.k("dispatchMs").arr().v(R3(d[0])).v(R3(d[1])).v(R3(d[2])).end();
    t.details.k("compute").obj().kv("iters", iters).kv("nonzero", c.nonzero).kv("monotonic", c.monotonic)
        .kv("gpuMs", R3(gpu_ms)).kv("cpuMs", R3(cpu_ms)).kv("ratio", R3(ratio)).kv("sane", sane)
        .kv("dispatchSpread", dmin > 0 ? R3(dmax / dmin) : 0.0).end();
    char b[128];
    snprintf(b, sizeof(b), "compute GPU %.1f ms / CPU %.1f ms (%.2f)", gpu_ms, cpu_ms, ratio);
    summary = b;
    return c.nonzero && c.monotonic && sane ? St::Pass : St::Fail;
  });

  t.Step("graphics", [&](Json& info) {
    Scope s(g);
    VkPipelineLayout layout = SolidLayout(g);
    VkRenderPass rp = g.RenderPass(VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_UNDEFINED);
    SolidPipeOpts o;
    o.rp = rp;
    VkPipeline pipe = SolidPipeline(g, layout, o);
    Image im = g.MakeImage(1024, 1024, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    VkFramebuffer fb = g.Framebuffer(rp, im);
    VkQueryPool q = QueryPool(g, 4);
    VkCommandBuffer cb = g.Begin();
    vk.CmdResetQueryPool(cb, q, 0, 4);
    vk.CmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, q, 0);
    VkClearValue cv{};
    VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    bi.renderPass = rp;
    bi.framebuffer = fb;
    bi.renderArea = {{0, 0}, {1024, 1024}};
    bi.clearValueCount = 1;
    bi.pClearValues = &cv;
    vk.CmdBeginRenderPass(cb, &bi, VK_SUBPASS_CONTENTS_INLINE);
    SetViewportScissor(cb, {0, 0, 1024, 1024}, {0, 0, 1024, 1024});
    vk.CmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
    const int draws = 300;
    for (int i = 0; i < draws; ++i) {
      float col[4] = {float(i % 7) / 7.0f, float(i % 5) / 5.0f, float(i % 3) / 3.0f, 1.0f};
      PushSolid(g, cb, layout, col);
      vk.CmdDraw(cb, 3, 1, 0, 0);
      if (i == draws / 2) vk.CmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, q, 1);
    }
    vk.CmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, q, 2);
    vk.CmdEndRenderPass(cb);
    vk.CmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, q, 3);
    g.Submit({cb});
    const double cpu_ms = g.lastSubmitCpuMs;
    auto ts = Results(g, q, 4);
    Check c = Analyse(ts);
    const double gpu_ms = c.monotonic ? double(ts[3] - ts[0]) * period / 1e6 : 0;
    const bool sane = gpu_ms > 0 && gpu_ms <= cpu_ms * 1.05 + 1.0;
    TsJson(info, ts);
    info.kv("draws", draws).kv("nonzero", c.nonzero).kv("monotonic", c.monotonic).kv("gpuMs", R3(gpu_ms))
        .kv("cpuMs", R3(cpu_ms)).kv("sane", sane);
    t.details.k("graphics").obj().kv("draws", draws).kv("nonzero", c.nonzero).kv("monotonic", c.monotonic)
        .kv("gpuMs", R3(gpu_ms)).kv("cpuMs", R3(cpu_ms)).kv("sane", sane).end();
    // Em tilers os timestamps dentro do passo podem ser todos iguais (o passo corre de uma vez): so reporta.
    return c.nonzero && c.monotonic && sane ? St::Pass : St::Fail;
  });

  if (!g.hasCalibrated) {
    t.SkipStep("calibrated", "sem VK_EXT/KHR_calibrated_timestamps");
  } else {
    t.Step("calibrated", [&](Json& info) {
      uint32_t n = 0;
      vk.GetPhysicalDeviceCalibrateableTimeDomainsEXT(g.pd, &n, nullptr);
      std::vector<VkTimeDomainEXT> domains(n);
      vk.GetPhysicalDeviceCalibrateableTimeDomainsEXT(g.pd, &n, domains.data());
      bool has_dev = false, has_mono = false, has_raw = false;
      info.k("domains").arr();
      for (auto d : domains) {
        info.v(int(d));
        has_dev |= d == VK_TIME_DOMAIN_DEVICE_EXT;
        has_mono |= d == VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT;
        has_raw |= d == VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT;
      }
      info.end();
      if (!has_dev || (!has_mono && !has_raw)) {
        info.kv("reason", "sem DEVICE + CLOCK_MONOTONIC(_RAW)");
        return St::Skip;
      }
      const VkTimeDomainEXT host = has_mono ? VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT : VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT;
      auto sample = [&](uint64_t out[2], uint64_t* dev_ns) {
        VkCalibratedTimestampInfoEXT ci[2] = {{VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT, nullptr, VK_TIME_DOMAIN_DEVICE_EXT},
                                              {VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT, nullptr, host}};
        VKC(vk.GetCalibratedTimestampsEXT(g.dev, 2, ci, out, dev_ns));
      };
      uint64_t a[2], b[2], dev0, dev1;
      sample(a, &dev0);
      usleep(200000);
      sample(b, &dev1);
      const double dev_delta = double(b[0] - a[0]) * period, host_delta = double(b[1] - a[1]);
      const double rate = host_delta > 0 ? dev_delta / host_delta : 0;
      // Timestamp de um dispatch entre duas amostras calibradas.
      bool inside = false;
      uint64_t c0[2], c1[2], d0, d1;
      std::vector<uint64_t> ts;
      if (w.pipe) {
        sample(c0, &d0);
        ts = RunCompute(g, w, qp, std::max<uint32_t>(iters / 4, 16));
        sample(c1, &d1);
        const uint64_t mask = valid_bits >= 64 ? ~0ull : ((1ull << valid_bits) - 1);
        inside = (ts[0] & mask) >= (c0[0] & mask) && (ts[3] & mask) <= (c1[0] & mask);
        info.k("dispatch").obj().kv("calibratedBefore", c0[0]).kv("first", ts[0]).kv("last", ts[3])
            .kv("calibratedAfter", c1[0]).kv("inside", inside).end();
      }
      const bool ok = rate > 0.98 && rate < 1.02 && (!w.pipe || inside);
      info.kv("hostDomain", int(host)).kv("deviceDeltaMs", R3(dev_delta / 1e6)).kv("hostDeltaMs", R3(host_delta / 1e6))
          .kv("rate", R3(rate)).kv("maxDeviationNs", dev1);
      t.details.k("calibrated").obj().kv("hostDomain", int(host)).kv("rate", R3(rate)).kv("maxDeviationNs", dev1)
          .kv("dispatchInside", inside).kv("ok", ok).end();
      return ok ? St::Pass : St::Fail;
    });
  }
  t.message = summary + (t.fails ? "; " + std::to_string(t.fails) + " passo(s) falharam" : "");
}

}  // namespace zdiag
