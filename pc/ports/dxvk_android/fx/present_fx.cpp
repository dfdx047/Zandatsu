// Present effects for DXVK 1.9.2b: FSR 1 / CAS upscale and 30 -> 60 frame generation. See present_fx.h.
#include "present_fx.h"
#include "src/dxvk/dxvk_swapchain_blitter.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef __ANDROID__
#include <android/log.h>
#endif

#include <dxvk_present_vert.h>
#include "generated/dxvk_fsr_easu.h"
#include "generated/dxvk_fsr_rcas.h"
#include "generated/dxvk_cas.h"
#include "generated/dxvk_fi_p1.h"
#include "generated/dxvk_fi_p2.h"
#include "generated/dxvk_fi_p3.h"
#include "generated/dxvk_fi_p4.h"

// AMD FidelityFX FSR 1 constant setup on the CPU (FsrEasuCon, FsrRcasCon). MIT: fx/ffx/LICENSE-FidelityFX.txt.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wmissing-braces"
#pragma clang diagnostic ignored "-Wall"
#define A_CPU 1
#include "ffx/ffx_a.h"
#include "ffx/ffx_fsr1.h"
#pragma clang diagnostic pop

#ifdef __ANDROID__
// The game's own log file (game_log.h) when the app provides it, as DXVK's logger does.
extern "C" __attribute__((weak)) int nfsRuntimeLog(int, const char*, const char*, ...);
#endif

namespace dxvk {

  namespace {

    void fxLog(const char* format, ...) {
      char text[512];
      va_list args;
      va_start(args, format);
      std::vsnprintf(text, sizeof text, format, args);
      va_end(args);
#ifdef __ANDROID__
      if (nfsRuntimeLog)
        nfsRuntimeLog(ANDROID_LOG_INFO, "MGR", "present-fx %s", text);
      else
        __android_log_print(ANDROID_LOG_INFO, "MGR", "present-fx %s", text);
#else
      std::fprintf(stderr, "present-fx %s\n", text);
#endif
    }

    float envFloat(const char* name, float fallback) {
      const char* s = std::getenv(name);
      return s && *s ? float(std::atof(s)) : fallback;
    }

    int envInt(const char* name, int fallback) {
      const char* s = std::getenv(name);
      return s && *s ? std::atoi(s) : fallback;
    }

    uint32_t divUp(uint32_t a, uint32_t b) { return (a + b - 1) / b; }

    // Fragment shader slots (fx/*.frag).
    enum FsSlot : uint32_t { FsImage = 0, FsGamma = 1 };

    struct EasuPush { uint32_t con0[4], con1[4], con2[4], con3[4]; };
    struct RcasPush { uint32_t con[4]; uint32_t gamma; };
    struct CasPush  { float scale[2]; int32_t size[2]; float peak; uint32_t gamma; };

  }


  const DxvkPresentFxConfig& presentFxConfig() {
    static const DxvkPresentFxConfig config = [] {
      DxvkPresentFxConfig c;
      c.fsr       = std::clamp(envInt("DXVK_FSR", 0), 0, 2);
      c.sharpness = std::clamp(envFloat("DXVK_FSR_SHARPNESS", 0.8f), 0.0f, 1.0f);
      c.fi        = envInt("DXVK_FI", 0) != 0;
      c.fiQuality = std::clamp(envInt("DXVK_FI_QUALITY", 1), 0, 2);
      c.displayHz = uint32_t(std::clamp(envInt("DXVK_FI_HZ", 60), 30, 240));
      return c;
    }();
    return config;
  }


  DxvkPresentFx::DxvkPresentFx(const Rc<DxvkDevice>& device)
  : m_device(device), m_config(presentFxConfig()) {
    if (m_config.fi)
      m_repeat = std::max(1u, uint32_t(std::lround(double(m_config.displayHz) / 60.0)));
    m_tsPeriod = m_device->adapter()->deviceProperties().limits.timestampPeriod;
    if (const char* dump = std::getenv("DXVK_FX_DUMP"))
      m_dumpFrame = std::atoll(dump);
    if (m_config.fsr || m_config.fi) {
      createShaders();
      fxLog("config: FSR=%d (%s) sharpness=%.2f, FI=%d quality=%d display=%u Hz (each frame presented %u time(s))",
        m_config.fsr, m_config.fsr == 1 ? "EASU+RCAS" : m_config.fsr == 2 ? "CAS" : "off", m_config.sharpness,
        m_config.fi ? 1 : 0, m_config.fiQuality, m_config.displayHz, m_repeat);
    }
  }


  DxvkPresentFx::~DxvkPresentFx() {
  }


  void DxvkPresentFx::createShaders() {
    const std::array<DxvkResourceSlot, 2> fsSlots = {{
      { FsImage, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_VIEW_TYPE_2D, VK_ACCESS_SHADER_READ_BIT },
      { FsGamma, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_VIEW_TYPE_1D, VK_ACCESS_SHADER_READ_BIT },
    }};

    m_vs = m_device->createShader(VK_SHADER_STAGE_VERTEX_BIT, 0, nullptr, { 0u, 1u },
      SpirvCodeBuffer(dxvk_present_vert));
    m_fsEasu = m_device->createShader(VK_SHADER_STAGE_FRAGMENT_BIT, 1, fsSlots.data(),
      { 1u, 1u, 0u, uint32_t(sizeof(EasuPush)) }, SpirvCodeBuffer(dxvk_fsr_easu));
    m_fsRcas = m_device->createShader(VK_SHADER_STAGE_FRAGMENT_BIT, fsSlots.size(), fsSlots.data(),
      { 1u, 1u, 0u, uint32_t(sizeof(RcasPush)) }, SpirvCodeBuffer(dxvk_fsr_rcas));
    m_fsCas = m_device->createShader(VK_SHADER_STAGE_FRAGMENT_BIT, fsSlots.size(), fsSlots.data(),
      { 1u, 1u, 0u, uint32_t(sizeof(CasPush)) }, SpirvCodeBuffer(dxvk_cas));

    // Frame generation: the bindings of fx/dxvk_fi.glsl, the same layout for the four passes.
    constexpr VkAccessFlags R = VK_ACCESS_SHADER_READ_BIT, W = VK_ACCESS_SHADER_WRITE_BIT;
    const std::array<DxvkResourceSlot, 14> csSlots = {{
      {  0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_VIEW_TYPE_2D, R },      // u_cur
      {  1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_VIEW_TYPE_2D, R },      // u_prev
      {  2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          VK_IMAGE_VIEW_TYPE_2D, W },      // u_hist_out
      {  3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          VK_IMAGE_VIEW_TYPE_2D, W },      // u_gen
      {  4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_VIEW_TYPE_2D, R },      // u_q_prev
      {  5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_VIEW_TYPE_2D, R },      // u_q_cur
      {  6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          VK_IMAGE_VIEW_TYPE_2D, W },      // u_q_out
      {  7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_VIEW_TYPE_2D, R },      // u_s_prev
      {  8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_VIEW_TYPE_2D, R },      // u_s_cur
      {  9, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          VK_IMAGE_VIEW_TYPE_2D, W },      // u_s_out
      { 10, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          VK_IMAGE_VIEW_TYPE_2D, R | W },  // u_vs
      { 11, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          VK_IMAGE_VIEW_TYPE_2D, W },      // u_v8_out
      { 12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_VIEW_TYPE_2D, R },      // u_v8
      { 13, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         VK_IMAGE_VIEW_TYPE_MAX_ENUM, R | W },  // counters
    }};
    const uint32_t* code[4] = { dxvk_fi_p1, dxvk_fi_p2, dxvk_fi_p3, dxvk_fi_p4 };
    const size_t size[4] = { sizeof(dxvk_fi_p1), sizeof(dxvk_fi_p2), sizeof(dxvk_fi_p3), sizeof(dxvk_fi_p4) };
    for (uint32_t i = 0; i < 4; i++)
      m_csFi[i] = m_device->createShader(VK_SHADER_STAGE_COMPUTE_BIT, csSlots.size(), csSlots.data(),
        { 0u, 0u, 0u, uint32_t(sizeof(FiPush)) }, SpirvCodeBuffer(uint32_t(size[i] / sizeof(uint32_t)), code[i]));

    DxvkSamplerCreateInfo sampler;
    sampler.magFilter      = VK_FILTER_LINEAR;
    sampler.minFilter      = VK_FILTER_LINEAR;
    sampler.mipmapMode     = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.mipmapLodBias  = 0.0f;
    sampler.mipmapLodMin   = 0.0f;
    sampler.mipmapLodMax   = 0.0f;
    sampler.useAnisotropy  = VK_FALSE;
    sampler.maxAnisotropy  = 1.0f;
    sampler.addressModeU   = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeV   = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeW   = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.compareToDepth = VK_FALSE;
    sampler.compareOp      = VK_COMPARE_OP_ALWAYS;
    sampler.borderColor    = VkClearColorValue();
    sampler.usePixelCoord  = VK_FALSE;
    m_linear = m_device->createSampler(sampler);

    for (auto& slot : m_slots) {
      for (Timer* t : { &slot.up, &slot.fi }) {
        t->begin = m_device->createGpuQuery(VK_QUERY_TYPE_TIMESTAMP, 0, 0);
        t->end   = m_device->createGpuQuery(VK_QUERY_TYPE_TIMESTAMP, 0, 0);
      }
    }
  }


  DxvkPresentFx::FiImage DxvkPresentFx::createImage(VkExtent2D size, VkFormat format, bool attachment) {
    DxvkImageCreateInfo info;
    info.type        = VK_IMAGE_TYPE_2D;
    info.format      = format;
    info.flags       = 0;
    info.sampleCount = VK_SAMPLE_COUNT_1_BIT;
    info.extent      = { size.width, size.height, 1 };
    info.numLayers   = 1;
    info.mipLevels   = 1;
    info.tiling      = VK_IMAGE_TILING_OPTIMAL;
    if (attachment) {
      // EASU output: rendered, then sampled by RCAS.
      info.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      info.stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                  | VK_PIPELINE_STAGE_TRANSFER_BIT;
      info.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT
                  | VK_ACCESS_TRANSFER_READ_BIT;
      info.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    } else {
      // Frame generation: written by compute, sampled by compute and by the present passes (copied by the
      // lab dump).
      info.usage  = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      info.stages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
      info.access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
      info.layout = VK_IMAGE_LAYOUT_GENERAL;
    }

    FiImage result;
    result.image = m_device->createImage(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    DxvkImageViewCreateInfo view;
    view.type      = VK_IMAGE_VIEW_TYPE_2D;
    view.format    = format;
    view.aspect    = VK_IMAGE_ASPECT_COLOR_BIT;
    view.minLevel  = 0;
    view.numLevels = 1;
    view.minLayer  = 0;
    view.numLayers = 1;
    view.usage     = VK_IMAGE_USAGE_SAMPLED_BIT;
    result.sampled = m_device->createImageView(result.image, view);
    view.usage     = attachment ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : VK_IMAGE_USAGE_STORAGE_BIT;
    result.storage = m_device->createImageView(result.image, view);
    return result;
  }


  bool DxvkPresentFx::ensureFi(VkExtent2D size) {
    if (m_fiBroken)
      return false;
    if (m_fiSize == size && m_gen.image != nullptr)
      return true;
    try {
      m_fiSize = size;
      m_fiK = int32_t(std::max<uint32_t>(1, (size.width + 640) / 1280));
      const uint32_t d = 4 * uint32_t(m_fiK);
      m_fiQ = { divUp(size.width, d), divUp(size.height, d) };
      m_fiS = { divUp(m_fiQ.width, 4), divUp(m_fiQ.height, 4) };
      m_fiB = { divUp(m_fiQ.width, 2), divUp(m_fiQ.height, 2) };
      for (uint32_t i = 0; i < 2; i++) {
        m_hist[i] = createImage(size, VK_FORMAT_R8G8B8A8_UNORM, false);
        m_q[i]    = createImage(m_fiQ, VK_FORMAT_R8G8B8A8_UNORM, false);
        m_s[i]    = createImage(m_fiS, VK_FORMAT_R8G8B8A8_UNORM, false);
      }
      m_vs8 = createImage(m_fiS, VK_FORMAT_R16G16B16A16_SFLOAT, false);
      m_v8  = createImage(m_fiB, VK_FORMAT_R16G16B16A16_SFLOAT, false);
      m_gen = createImage(size, VK_FORMAT_R8G8B8A8_UNORM, false);
      if (m_counters == nullptr) {
        DxvkBufferCreateInfo info;
        info.size   = 256 * Slots;
        info.usage  = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        info.stages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        info.access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        m_counters = m_device->createBuffer(info, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        std::memset(m_counters->mapPtr(0), 0, info.size);
      }
    } catch (const DxvkError& e) {
      fxLog("frame generation off: could not create its images (%s)", e.message().c_str());
      m_fiBroken = true;
      m_gen = FiImage();
      return false;
    }
    m_historyValid = false;
    m_parity = 0;
    fxLog("frame generation: %ux%u (k=%d, luma %ux%u and %ux%u, blocks %ux%u)", size.width, size.height, m_fiK,
      m_fiQ.width, m_fiQ.height, m_fiS.width, m_fiS.height, m_fiB.width, m_fiB.height);
    return true;
  }


  bool DxvkPresentFx::beginFrame(const Rc<DxvkImageView>& backBuffer) {
    Slot& slot = m_slots[m_frame % Slots];
    collect(slot);
    slot.generated = false;
    m_stats.frames++;

    if (!m_config.fi)
      return false;

    auto now = std::chrono::steady_clock::now();
    auto gap = now - m_lastFrame;
    m_lastFrame = now;

    const auto& info = backBuffer->imageInfo();
    if (info.sampleCount != VK_SAMPLE_COUNT_1_BIT || !ensureFi({ info.extent.width, info.extent.height })) {
      m_historyValid = false;
      return false;
    }
    // A long gap (loading, pause): the history is too old to interpolate from.
    if (gap > std::chrono::milliseconds(150))
      m_historyValid = false;
    if (!m_historyValid)
      m_stats.notGenerated++;
    return m_historyValid;
  }


  Rc<DxvkImageView> DxvkPresentFx::generate(DxvkContext* ctx, const Rc<DxvkImageView>& backBuffer) {
    Slot& slot = m_slots[m_frame % Slots];
    startTimer(ctx, slot.fi);
    recordFi(ctx, backBuffer, true);
    stopTimer(ctx, slot.fi);
    if (dumping()) {
      dumpImage(ctx, m_hist[1 - m_parity].image, "previous");
      dumpImage(ctx, m_gen.image, "generated");
    }
    slot.generated = true;
    slot.countersPending = true;
    m_stats.generated++;
    return m_gen.sampled;
  }


  void DxvkPresentFx::keepHistory(DxvkContext* ctx, const Rc<DxvkImageView>& backBuffer) {
    if (!m_config.fi || m_fiBroken || m_gen.image == nullptr)
      return;
    const auto& info = backBuffer->imageInfo();
    if (info.sampleCount != VK_SAMPLE_COUNT_1_BIT || info.extent.width != m_fiSize.width || info.extent.height != m_fiSize.height)
      return;
    recordFi(ctx, backBuffer, false);
    m_historyValid = true;
  }


  void DxvkPresentFx::recordFi(DxvkContext* ctx, const Rc<DxvkImageView>& cur, bool generate) {
    const uint32_t prev = m_parity, next = 1 - m_parity;
    const uint32_t slotIndex = uint32_t(m_frame % Slots);

    FiPush push = { };
    push.full_size[0] = int32_t(m_fiSize.width);  push.full_size[1] = int32_t(m_fiSize.height);
    push.q_size[0]    = int32_t(m_fiQ.width);     push.q_size[1]    = int32_t(m_fiQ.height);
    push.s_size[0]    = int32_t(m_fiS.width);     push.s_size[1]    = int32_t(m_fiS.height);
    push.b_size[0]    = int32_t(m_fiB.width);     push.b_size[1]    = int32_t(m_fiB.height);
    push.k            = m_fiK;
    push.quality      = m_config.fiQuality;
    push.mode         = generate ? 0 : 1;

    for (uint32_t i = 0; i < 14; i++)
      ctx->bindResourceSampler(i, m_linear);
    ctx->bindResourceView(0,  cur, nullptr);
    ctx->bindResourceView(1,  m_hist[prev].sampled, nullptr);
    ctx->bindResourceView(2,  m_hist[next].storage, nullptr);
    ctx->bindResourceView(3,  m_gen.storage, nullptr);
    ctx->bindResourceView(4,  m_q[prev].sampled, nullptr);
    ctx->bindResourceView(5,  m_q[next].sampled, nullptr);
    ctx->bindResourceView(6,  m_q[next].storage, nullptr);
    ctx->bindResourceView(7,  m_s[prev].sampled, nullptr);
    ctx->bindResourceView(8,  m_s[next].sampled, nullptr);
    ctx->bindResourceView(9,  m_s[next].storage, nullptr);
    ctx->bindResourceView(10, m_vs8.storage, nullptr);
    ctx->bindResourceView(11, m_v8.storage, nullptr);
    ctx->bindResourceView(12, m_v8.sampled, nullptr);
    ctx->bindResourceBuffer(13, DxvkBufferSlice(m_counters, 256 * slotIndex, 16));
    ctx->pushConstants(0, sizeof(push), &push);

    // Pass 1 writes q/s[next], which passes 2 and 3 read through other views of the same images: DXVK's
    // hazard tracking puts the barriers between the dispatches.
    ctx->bindShader(VK_SHADER_STAGE_COMPUTE_BIT, m_csFi[0]);
    ctx->dispatch(divUp(m_fiS.width, 2), divUp(m_fiS.height, 2), 1);
    if (generate && m_config.fiQuality > 0) {
      ctx->bindShader(VK_SHADER_STAGE_COMPUTE_BIT, m_csFi[1]);
      ctx->dispatch(divUp(m_fiS.width, 8), divUp(m_fiS.height, 8), 1);
      ctx->bindShader(VK_SHADER_STAGE_COMPUTE_BIT, m_csFi[2]);
      ctx->dispatch(divUp(m_fiB.width, 8), divUp(m_fiB.height, 8), 1);
    }
    ctx->bindShader(VK_SHADER_STAGE_COMPUTE_BIT, m_csFi[3]);
    ctx->dispatch(divUp(m_fiSize.width, 8), divUp(m_fiSize.height, 8), 1);

    // Unbind, so that the images are not kept in the compute state.
    for (uint32_t i = 0; i < 13; i++)
      ctx->bindResourceView(i, nullptr, nullptr);
    ctx->bindResourceBuffer(13, DxvkBufferSlice());
    m_parity = next;
  }


  bool DxvkPresentFx::present(
          DxvkContext*            ctx,
          DxvkSwapchainBlitter*   blitter,
    const Rc<DxvkImageView>&      dstView,
          VkRect2D                dstRect,
    const Rc<DxvkImageView>&      srcView,
          VkRect2D                srcRect) {
    if (dumping())
      dumpImage(ctx, srcView->image(), "input");
    if (!m_config.fsr)
      return false;

    VkExtent2D dstExtent = { dstView->imageInfo().extent.width, dstView->imageInfo().extent.height };
    VkExtent2D srcExtent = { srcView->imageInfo().extent.width, srcView->imageInfo().extent.height };
    if (!dstRect.extent.width || !dstRect.extent.height)
      dstRect = { { 0, 0 }, dstExtent };
    if (!srcRect.extent.width || !srcRect.extent.height)
      srcRect = { { 0, 0 }, srcExtent };

    // Only the whole back buffer to the whole window, upscaling or 1:1; anything else: the stock blit.
    if (srcView->imageInfo().sampleCount != VK_SAMPLE_COUNT_1_BIT
     || srcRect.offset.x || srcRect.offset.y || dstRect.offset.x || dstRect.offset.y
     || dstRect.extent != dstExtent
     || srcRect.extent.width > dstExtent.width || srcRect.extent.height > dstExtent.height)
      return false;

    const bool sameSize = srcRect.extent == dstExtent;
    Rc<DxvkImageView> gamma = blitter->presentFxGammaView(ctx);

    // GPU time of one upscale: with several presents per game frame the last one is measured (a timestamp
    // query written again starts over).
    Slot& slot = m_slots[m_frame % Slots];
    startTimer(ctx, slot.up);

    char label[96];
    if (m_config.fsr == 1) {
      VkExtent2D rcasExtent = dstExtent;
      Rc<DxvkImageView> rcasSource = srcView;
      if (!sameSize) {
        if (m_easuImage == nullptr || m_easuImage->info().extent.width != dstExtent.width
         || m_easuImage->info().extent.height != dstExtent.height) {
          FiImage easu = createImage(dstExtent, VK_FORMAT_R8G8B8A8_UNORM, true);
          m_easuImage = easu.image;
          m_easuView = easu.sampled;
          m_easuTarget = easu.storage;
        }
        EasuPush easu;
        FsrEasuCon(easu.con0, easu.con1, easu.con2, easu.con3,
          AF1(srcRect.extent.width), AF1(srcRect.extent.height),
          AF1(srcExtent.width), AF1(srcExtent.height),
          AF1(dstExtent.width), AF1(dstExtent.height));
        draw(ctx, m_fsEasu, m_easuTarget, dstExtent, srcView, nullptr, &easu, sizeof(easu), 0);
        rcasSource = m_easuView;
      }
      RcasPush rcas = { };
      FsrRcasCon(rcas.con, 2.0f * (1.0f - m_config.sharpness));
      rcas.gamma = gamma != nullptr;
      draw(ctx, m_fsRcas, dstView, rcasExtent, rcasSource, gamma, &rcas, sizeof(rcas), 0);
      if (dumping()) {
        FiImage out = createImage(dstExtent, VK_FORMAT_R8G8B8A8_UNORM, true);
        draw(ctx, m_fsRcas, out.storage, rcasExtent, rcasSource, gamma, &rcas, sizeof(rcas), 0);
        dumpImage(ctx, out.image, "output");
      }
      std::snprintf(label, sizeof label, "%s %ux%u -> %ux%u", sameSize ? "RCAS" : "EASU+RCAS",
        srcRect.extent.width, srcRect.extent.height, dstExtent.width, dstExtent.height);
    } else {
      CasPush cas;
      cas.scale[0] = float(srcRect.extent.width) / float(dstExtent.width);
      cas.scale[1] = float(srcRect.extent.height) / float(dstExtent.height);
      cas.size[0]  = int32_t(srcRect.extent.width);
      cas.size[1]  = int32_t(srcRect.extent.height);
      cas.peak     = -1.0f / (8.0f + (5.0f - 8.0f) * m_config.sharpness);
      cas.gamma    = gamma != nullptr;
      draw(ctx, m_fsCas, dstView, dstExtent, srcView, gamma, &cas, sizeof(cas), sameSize ? 0 : 1);
      if (dumping()) {
        FiImage out = createImage(dstExtent, VK_FORMAT_R8G8B8A8_UNORM, true);
        draw(ctx, m_fsCas, out.storage, dstExtent, srcView, gamma, &cas, sizeof(cas), sameSize ? 0 : 1);
        dumpImage(ctx, out.image, "output");
      }
      std::snprintf(label, sizeof label, "CAS %ux%u -> %ux%u", srcRect.extent.width, srcRect.extent.height,
        dstExtent.width, dstExtent.height);
    }
    stopTimer(ctx, slot.up);
    if (m_upLabel != label) {
      m_upLabel = label;
      fxLog("upscale: %s", label);
    }
    m_stats.upscaled++;
    return true;
  }


  void DxvkPresentFx::draw(
          DxvkContext*            ctx,
    const Rc<DxvkShader>&         fs,
    const Rc<DxvkImageView>&      dstView,
          VkExtent2D              dstExtent,
    const Rc<DxvkImageView>&      srcView,
    const Rc<DxvkImageView>&      gammaView,
    const void*                   push,
          uint32_t                pushSize,
          uint32_t                specValue) {
    // Same fixed state as DxvkSwapchainBlitter::draw.
    DxvkInputAssemblyState iaState;
    iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    iaState.primitiveRestart  = VK_FALSE;
    iaState.patchVertexCount  = 0;
    ctx->setInputAssemblyState(iaState);
    ctx->setInputLayout(0, nullptr, 0, nullptr);

    DxvkRasterizerState rsState;
    rsState.polygonMode      = VK_POLYGON_MODE_FILL;
    rsState.cullMode         = VK_CULL_MODE_BACK_BIT;
    rsState.frontFace        = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.depthClipEnable  = VK_FALSE;
    rsState.depthBiasEnable  = VK_FALSE;
    rsState.conservativeMode = VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
    rsState.sampleCount      = VK_SAMPLE_COUNT_1_BIT;
    ctx->setRasterizerState(rsState);

    DxvkMultisampleState msState;
    msState.sampleMask            = 0xffffffff;
    msState.enableAlphaToCoverage = VK_FALSE;
    ctx->setMultisampleState(msState);

    VkStencilOpState stencilOp;
    stencilOp.failOp      = VK_STENCIL_OP_KEEP;
    stencilOp.passOp      = VK_STENCIL_OP_KEEP;
    stencilOp.depthFailOp = VK_STENCIL_OP_KEEP;
    stencilOp.compareOp   = VK_COMPARE_OP_ALWAYS;
    stencilOp.compareMask = 0xFFFFFFFF;
    stencilOp.writeMask   = 0xFFFFFFFF;
    stencilOp.reference   = 0;

    DxvkDepthStencilState dsState;
    dsState.enableDepthTest   = VK_FALSE;
    dsState.enableDepthWrite  = VK_FALSE;
    dsState.enableStencilTest = VK_FALSE;
    dsState.depthCompareOp    = VK_COMPARE_OP_ALWAYS;
    dsState.stencilOpFront    = stencilOp;
    dsState.stencilOpBack     = stencilOp;
    ctx->setDepthStencilState(dsState);

    DxvkLogicOpState loState;
    loState.enableLogicOp = VK_FALSE;
    loState.logicOp       = VK_LOGIC_OP_NO_OP;
    ctx->setLogicOpState(loState);

    DxvkBlendMode blendMode;
    blendMode.enableBlending = VK_FALSE;
    blendMode.colorSrcFactor = VK_BLEND_FACTOR_ONE;
    blendMode.colorDstFactor = VK_BLEND_FACTOR_ZERO;
    blendMode.colorBlendOp   = VK_BLEND_OP_ADD;
    blendMode.alphaSrcFactor = VK_BLEND_FACTOR_ONE;
    blendMode.alphaDstFactor = VK_BLEND_FACTOR_ZERO;
    blendMode.alphaBlendOp   = VK_BLEND_OP_ADD;
    blendMode.writeMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                             | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ctx->setBlendMode(0, blendMode);

    VkRect2D scissor = { { 0, 0 }, dstExtent };
    VkViewport viewport = { 0.0f, 0.0f, float(dstExtent.width), float(dstExtent.height), 0.0f, 1.0f };
    ctx->setViewports(1, &viewport, &scissor);

    DxvkRenderTargets renderTargets;
    renderTargets.color[0].view   = dstView;
    renderTargets.color[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ctx->bindRenderTargets(renderTargets);
    ctx->discardImageView(dstView, VK_IMAGE_ASPECT_COLOR_BIT);

    ctx->bindResourceSampler(FsImage, m_linear);
    ctx->bindResourceSampler(FsGamma, m_linear);
    ctx->bindResourceView(FsImage, srcView, nullptr);
    ctx->bindResourceView(FsGamma, gammaView, nullptr);

    ctx->bindShader(VK_SHADER_STAGE_VERTEX_BIT, m_vs);
    ctx->bindShader(VK_SHADER_STAGE_FRAGMENT_BIT, fs);
    ctx->pushConstants(0, pushSize, push);
    ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, 0, specValue);
    ctx->draw(3, 1, 0, 0);
    ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0);

    // The EASU image is read by the next pass: leave no binding to it behind.
    ctx->bindResourceView(FsImage, nullptr, nullptr);
    ctx->bindResourceView(FsGamma, nullptr, nullptr);
  }


  void DxvkPresentFx::startTimer(DxvkContext* ctx, Timer& timer) {
    if (timer.begin == nullptr)
      return;
    ctx->writeTimestampOutsidePass(timer.begin);
  }


  void DxvkPresentFx::stopTimer(DxvkContext* ctx, Timer& timer) {
    if (timer.end == nullptr)
      return;
    ctx->writeTimestampOutsidePass(timer.end);
    timer.pending = true;
  }


  double DxvkPresentFx::timerNs(Timer& timer) {
    if (!timer.pending)
      return -1.0;
    timer.pending = false;
    DxvkQueryData a = { }, b = { };
    if (timer.begin->getData(a) != DxvkGpuQueryStatus::Available
     || timer.end->getData(b) != DxvkGpuQueryStatus::Available
     || b.timestamp.time < a.timestamp.time)
      return -1.0;
    return double(b.timestamp.time - a.timestamp.time) * double(m_tsPeriod);
  }


  void DxvkPresentFx::collect(Slot& slot) {
    // This slot was last used 8 frames ago: its GPU work is done unless the queue is far behind (then the
    // sample is dropped).
    double up = timerNs(slot.up);
    if (up >= 0.0) { m_stats.upNs += up; m_stats.upSamples++; }
    double fi = timerNs(slot.fi);
    if (fi >= 0.0) { m_stats.fiNs += fi; m_stats.fiSamples++; }
    if (slot.countersPending && m_counters != nullptr && fi >= 0.0) {
      const uint32_t* c = reinterpret_cast<const uint32_t*>(m_counters->mapPtr(256 * (&slot - m_slots.data())));
      const uint64_t pixels = uint64_t(m_fiSize.width) * m_fiSize.height;
      if (uint64_t(c[0]) * 2 > uint64_t(m_fiB.width) * m_fiB.height) {
        m_stats.cuts++;
      } else {
        m_stats.pixels   += pixels;
        m_stats.moving   += pixels > c[2] ? pixels - c[2] : 0;
        m_stats.fallback += c[1];
      }
    }
    slot.countersPending = false;
  }


  void DxvkPresentFx::dumpImage(DxvkContext* ctx, const Rc<DxvkImage>& image, const char* name) {
    const auto& info = image->info();
    if (!(info.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) || info.sampleCount != VK_SAMPLE_COUNT_1_BIT
     || (info.format != VK_FORMAT_R8G8B8A8_UNORM && info.format != VK_FORMAT_B8G8R8A8_UNORM)) {
      fxLog("dump: %s skipped (format %d, usage %#x)", name, int(info.format), unsigned(info.usage));
      return;
    }
    DxvkBufferCreateInfo bufferInfo;
    bufferInfo.size   = VkDeviceSize(info.extent.width) * info.extent.height * 4;
    bufferInfo.usage  = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    bufferInfo.access = VK_ACCESS_TRANSFER_WRITE_BIT;
    Dump dump;
    dump.buffer = m_device->createBuffer(bufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    dump.extent = { info.extent.width, info.extent.height };
    dump.bgra   = info.format == VK_FORMAT_B8G8R8A8_UNORM;
    dump.name   = std::to_string(m_dumps.size()) + "-" + name;
    ctx->copyImageToBuffer(dump.buffer, 0, 0, 0, image, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
      VkOffset3D { 0, 0, 0 }, info.extent);
    m_dumps.push_back(std::move(dump));
  }


  void DxvkPresentFx::writeDumps() {
    std::string dir = ".";
    if (const char* d = std::getenv("DXVK_FX_DUMP_DIR"))
      dir = d;
    else if (const char* d = std::getenv("DXVK_STATE_CACHE_PATH"))
      dir = d;
    for (const Dump& dump : m_dumps) {
      std::string path = dir + "/fx-dump-" + std::to_string(m_dumpFrame) + "-" + dump.name + ".ppm";
      FILE* file = std::fopen(path.c_str(), "wb");
      if (!file) {
        fxLog("dump: cannot write %s", path.c_str());
        continue;
      }
      std::fprintf(file, "P6\n%u %u\n255\n", dump.extent.width, dump.extent.height);
      const uint8_t* p = reinterpret_cast<const uint8_t*>(dump.buffer->mapPtr(0));
      std::vector<uint8_t> row(size_t(dump.extent.width) * 3);
      for (uint32_t y = 0; y < dump.extent.height; y++) {
        for (uint32_t x = 0; x < dump.extent.width; x++, p += 4) {
          row[x * 3 + 0] = dump.bgra ? p[2] : p[0];
          row[x * 3 + 1] = p[1];
          row[x * 3 + 2] = dump.bgra ? p[0] : p[2];
        }
        std::fwrite(row.data(), 1, row.size(), file);
      }
      std::fclose(file);
      fxLog("dump: %s", path.c_str());
    }
    m_dumps.clear();
  }


  void DxvkPresentFx::endFrame() {
    // Lab dump: frame N's copies have completed by frame N + 4 (the swap chain waits for frame N + 1 - latency).
    if (m_dumpFrame >= 0 && int64_t(m_frame) == m_dumpFrame + 4 && !m_dumps.empty())
      writeDumps();
    m_frame++;
    if (!(m_config.fsr || m_config.fi) || m_stats.frames < 300)
      return;
    char up[128] = "off", fi[256] = "off";
    if (m_config.fsr)
      std::snprintf(up, sizeof up, "%s, %llu presents, GPU %.2f ms per upscale",
        m_upLabel.empty() ? "not used (plain blit)" : m_upLabel.c_str(), (unsigned long long) m_stats.upscaled,
        m_stats.upSamples ? m_stats.upNs / 1e6 / double(m_stats.upSamples) : 0.0);
    if (m_config.fi)
      std::snprintf(fi, sizeof fi, "generated %llu, not generated %llu (no history / gap / size change), scene cuts %llu, "
        "moving pixels %.1f%%, of them fallback %.1f%%, GPU %.2f ms per generated frame",
        (unsigned long long) m_stats.generated, (unsigned long long) m_stats.notGenerated, (unsigned long long) m_stats.cuts,
        m_stats.pixels ? 100.0 * double(m_stats.moving) / double(m_stats.pixels) : 0.0,
        m_stats.moving ? 100.0 * double(m_stats.fallback) / double(m_stats.moving) : 0.0,
        m_stats.fiSamples ? m_stats.fiNs / 1e6 / double(m_stats.fiSamples) : 0.0);
    fxLog("%llu game frames: upscale %s; frame generation %s", (unsigned long long) m_stats.frames, up, fi);
    m_stats = { };
  }

}
