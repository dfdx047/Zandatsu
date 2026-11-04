// zdiag: viewport_scissor e clear_loadop — verificacao automatica de pixels lidos de volta.
//
// viewport_scissor reproduz dois bugs vistos em Mali de tres digitos (G710/G720) no nosso renderer:
//  "cut": ecra cortado — um passo pequeno (960x540) seguido, no mesmo command buffer, de um passo do
//         tamanho do ecra; o segundo tem de ficar todo pintado.
//  "minimap": scissor pequeno com renderArea do ecra inteiro; nada fora do scissor pode ser escrito.
#include <algorithm>
#include <cstdio>

#include "gpu.h"

namespace zdiag {
namespace {

const float kBlack[4] = {0, 0, 0, 1};
const float kBg[4] = {0, 0, 0.2f, 1};      // fundo (51 em 8 bits)
const float kSmall[4] = {1, 0, 0, 1};      // passo A (pequeno)
const float kGreen[4] = {0, 1, 0, 1};
const float kMagenta[4] = {1, 0, 1, 1};
const float kRed[4] = {1, 0, 0, 1};
const float kBlue[4] = {0, 0, 1, 1};
const float kYellow[4] = {1, 1, 0, 1};
const float kWhite[4] = {1, 1, 1, 1};

uint32_t Pack(const float c[4]) {
  auto b = [](float x) { return int(x * 255.0f + 0.5f); };
  return Rgba(b(c[0]), b(c[1]), b(c[2]), b(c[3]));
}

struct Ctx {
  Gpu* g = nullptr;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkRenderPass rp_clear = VK_NULL_HANDLE, rp_load = VK_NULL_HANDLE;
  VkRenderPass rp_clear_keep = VK_NULL_HANDLE;  // CLEAR com initialLayout COLOR_ATTACHMENT (fora da area: conteudo antigo)
  VkPipeline pipe = VK_NULL_HANDLE, pipe_dr = VK_NULL_HANDLE;
  int png_scale = 2;
  bool inject_bug = false;  // opcao inject_bug=1: estraga o estado de proposito (prova que a verificacao apanha)
};

constexpr VkImageUsageFlags kTargetUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT;

void BeginPass(VkCommandBuffer cb, VkRenderPass rp, VkFramebuffer fb, Rect area, const float clear[4],
               const VkClearDepthStencilValue* depth = nullptr) {
  VkClearValue cv[2]{};
  memcpy(cv[0].color.float32, clear, 16);
  if (depth) cv[1].depthStencil = *depth;
  VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  bi.renderPass = rp;
  bi.framebuffer = fb;
  bi.renderArea = {{area.x, area.y}, {uint32_t(area.w), uint32_t(area.h)}};
  bi.clearValueCount = depth ? 2 : 1;
  bi.pClearValues = cv;
  vk.CmdBeginRenderPass(cb, &bi, VK_SUBPASS_CONTENTS_INLINE);
}

void Draw(Ctx& c, VkCommandBuffer cb, VkPipeline p, const float color[4], float z = 0.5f) {
  vk.CmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, p);
  PushSolid(*c.g, cb, c.layout, color, z);
  vk.CmdDraw(cb, 3, 1, 0, 0);
}

std::vector<uint8_t> Finish(Gpu& g, const std::vector<VkCommandBuffer>& cbs, const Buffer& rb, bool separate = false) {
  g.Submit(cbs, separate);
  return g.ReadMapped(rb);
}

//-----------------------------------------------------------------------------
// viewport_scissor / cut
//-----------------------------------------------------------------------------
enum class CutMode { SameCb, SeparateCb, SeparateSubmit, StateBeforeBegin, DynRendering, StaticState };

struct CutVariant {
  const char* name;
  CutMode mode;
  int w, h;
  const char* what;
};

St RunCut(Ctx& c, Test& t, const CutVariant& v, Json& info) {
  Gpu& g = *c.g;
  if (v.mode == CutMode::DynRendering && !g.hasDynRendering) {
    info.kv("reason", "sem dynamicRendering");
    return St::Skip;
  }
  Scope scope(g);
  const int sw = 960, sh = 540;
  Image a = g.MakeImage(sw, sh, VK_FORMAT_R8G8B8A8_UNORM, kTargetUsage);
  Image b = g.MakeImage(v.w, v.h, VK_FORMAT_R8G8B8A8_UNORM, kTargetUsage);
  const Rect ra{0, 0, sw, sh}, rb{0, 0, v.w, v.h};

  VkPipeline pa = c.pipe, pb = c.pipe;
  if (v.mode == CutMode::StaticState) {
    VkViewport vpa{0, 0, float(sw), float(sh), 0, 1}, vpb{0, 0, float(v.w), float(v.h), 0, 1};
    VkRect2D sca{{0, 0}, {uint32_t(sw), uint32_t(sh)}}, scb{{0, 0}, {uint32_t(v.w), uint32_t(v.h)}};
    SolidPipeOpts o;
    o.rp = c.rp_clear;
    o.static_viewport = &vpa;
    o.static_scissor = &sca;
    pa = SolidPipeline(g, c.layout, o);
    o.static_viewport = &vpb;
    o.static_scissor = &scb;
    pb = SolidPipeline(g, c.layout, o);
  }
  const bool dynamic = v.mode != CutMode::StaticState;

  VkCommandBuffer cb1 = g.Begin();
  // Passo A: alvo pequeno.
  if (v.mode == CutMode::DynRendering) {
    auto begin = [&](VkCommandBuffer cb, const Image& im, const float clear[4]) {
      g.Barrier(cb, im.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
      VkRenderingAttachmentInfo att{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
      att.imageView = im.view;
      att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      memcpy(att.clearValue.color.float32, clear, 16);
      VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
      ri.renderArea = {{0, 0}, {im.w, im.h}};
      ri.layerCount = 1;
      ri.colorAttachmentCount = 1;
      ri.pColorAttachments = &att;
      vk.CmdBeginRendering(cb, &ri);
    };
    begin(cb1, a, kBg);
    SetViewportScissor(cb1, ra, ra);
    Draw(c, cb1, c.pipe_dr, kSmall);
    vk.CmdEndRendering(cb1);
    begin(cb1, b, kBlack);
    SetViewportScissor(cb1, rb, rb);
    Draw(c, cb1, c.pipe_dr, kGreen);
    vk.CmdEndRendering(cb1);
    Buffer buf = g.RecordReadback(cb1, b, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    auto px = Finish(g, {cb1}, buf);
    Coverage cov = Measure(px, v.w, v.h, Pack(kGreen), rb);
    CoverageJson(info, "covered", cov);
    info.kv("png", t.Png(std::string("cut/") + v.name, px.data(), v.w, v.h, v.w * 4, c.png_scale));
    info.k("target").arr().v(v.w).v(v.h).end().kv("variant", v.what);
    return cov.count == cov.total ? St::Pass : St::Fail;
  }

  VkFramebuffer fa = g.Framebuffer(c.rp_clear, a), fb = g.Framebuffer(c.rp_clear, b);
  BeginPass(cb1, c.rp_clear, fa, ra, kBg);
  if (dynamic) SetViewportScissor(cb1, ra, ra);
  Draw(c, cb1, pa, kSmall);
  vk.CmdEndRenderPass(cb1);

  const bool separate = v.mode == CutMode::SeparateCb || v.mode == CutMode::SeparateSubmit;
  VkCommandBuffer cb2 = separate ? g.Begin() : cb1;
  if (v.mode == CutMode::StateBeforeBegin) SetViewportScissor(cb2, rb, rb);  // antes do vkCmdBeginRenderPass
  BeginPass(cb2, c.rp_clear, fb, rb, kBlack);
  if (dynamic && v.mode != CutMode::StateBeforeBegin) SetViewportScissor(cb2, rb, c.inject_bug ? ra : rb);
  Draw(c, cb2, pb, kGreen);
  vk.CmdEndRenderPass(cb2);
  Buffer buf = g.RecordReadback(cb2, b, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  std::vector<VkCommandBuffer> cbs = {cb1};
  if (separate) cbs.push_back(cb2);
  auto px = Finish(g, cbs, buf, v.mode == CutMode::SeparateSubmit);
  Coverage cov = Measure(px, v.w, v.h, Pack(kGreen), rb);
  CoverageJson(info, "covered", cov);
  // Se faltar algo, diz tambem quanto ficou do tamanho do passo pequeno (o sintoma tipico).
  if (cov.count != cov.total) {
    Coverage in_small = Measure(px, v.w, v.h, Pack(kGreen), ra);
    CoverageJson(info, "coveredInsideSmallRect", in_small);
  }
  info.kv("png", t.Png(std::string("cut/") + v.name, px.data(), v.w, v.h, v.w * 4, c.png_scale));
  info.k("target").arr().v(v.w).v(v.h).end().kv("variant", v.what);
  return cov.count == cov.total ? St::Pass : St::Fail;
}

//-----------------------------------------------------------------------------
// viewport_scissor / minimap
//-----------------------------------------------------------------------------
enum class MiniMode { AreaFull, AreaEqScissorLoad, AreaEqScissorClear, MultiScissor, AfterFullPass, ViewportSmall };

struct MiniVariant {
  const char* name;
  MiniMode mode;
  Rect rect;
  const char* what;
};

St RunMinimap(Ctx& c, Test& t, const MiniVariant& v, int W, int H, Json& info) {
  Gpu& g = *c.g;
  Scope scope(g);
  Image im = g.MakeImage(W, H, VK_FORMAT_R8G8B8A8_UNORM, kTargetUsage);
  const Rect full{0, 0, W, H};
  VkCommandBuffer cb = g.Begin();
  struct Expect { Rect r; const float* color; };
  std::vector<Expect> expect;
  bool grade_outside = true;
  Rect area = full;

  if (v.mode == MiniMode::AfterFullPass) {
    // O passo da cena (ecra inteiro) noutro alvo, no mesmo command buffer, antes do HUD.
    Image scene = g.MakeImage(W, H, VK_FORMAT_R8G8B8A8_UNORM, kTargetUsage);
    VkFramebuffer fs = g.Framebuffer(c.rp_clear, scene);
    BeginPass(cb, c.rp_clear, fs, full, kBlack);
    SetViewportScissor(cb, full, full);
    Draw(c, cb, c.pipe, kYellow);
    vk.CmdEndRenderPass(cb);
  }
  if (v.mode == MiniMode::AreaEqScissorLoad) {
    g.Clear(cb, im, VK_IMAGE_LAYOUT_UNDEFINED, kBg, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    area = v.rect;
    BeginPass(cb, c.rp_load, g.Framebuffer(c.rp_load, im), area, kBg);
  } else if (v.mode == MiniMode::AreaEqScissorClear) {
    // Fora da renderArea o conteudo nao e avaliado (so reportado).
    g.Clear(cb, im, VK_IMAGE_LAYOUT_UNDEFINED, kBg, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    area = v.rect;
    grade_outside = false;
    BeginPass(cb, c.rp_clear_keep, g.Framebuffer(c.rp_clear_keep, im), area, kBg);
  } else {
    BeginPass(cb, c.rp_clear, g.Framebuffer(c.rp_clear, im), full, kBg);
  }

  if (v.mode == MiniMode::MultiScissor) {
    // Tres retangulos no mesmo passo: desenha, muda o scissor, desenha...
    const Rect rs[3] = {v.rect, {W / 2 - 37, H / 2 - 41, 75, 83}, {13, H - 113, 250, 100}};
    const float* cols[3] = {kRed, kGreen, kBlue};
    for (int i = 0; i < 3; ++i) {
      SetViewportScissor(cb, full, rs[i]);
      Draw(c, cb, c.pipe, cols[i]);
      expect.push_back({rs[i], cols[i]});
    }
  } else {
    SetViewportScissor(cb, v.mode == MiniMode::ViewportSmall ? v.rect : full,
                       c.inject_bug ? Rect{v.rect.x, v.rect.y, std::min(v.rect.w + 8, W - v.rect.x),
                                           std::min(v.rect.h + 8, H - v.rect.y)}
                                    : v.rect);
    Draw(c, cb, c.pipe, kMagenta);
    expect.push_back({v.rect, kMagenta});
  }
  vk.CmdEndRenderPass(cb);
  Buffer buf = g.RecordReadback(cb, im, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  auto px = Finish(g, {cb}, buf);

  bool ok = true;
  info.kv("variant", v.what);
  RectJson(info, "renderArea", area);
  info.k("rects").arr();
  for (auto& e : expect) {
    Coverage in = Measure(px, W, H, Pack(e.color), e.r);
    info.obj();
    RectJson(info, "scissor", e.r);
    CoverageJson(info, "inside", in);
    info.end();
    ok &= in.count == in.total;
  }
  info.end();
  // Fora de todos os retangulos: tem de continuar o fundo.
  std::vector<uint8_t> masked = px;
  uint32_t* p = reinterpret_cast<uint32_t*>(masked.data());
  for (auto& e : expect)
    for (int y = e.r.y; y < e.r.y + e.r.h; ++y)
      for (int x = e.r.x; x < e.r.x + e.r.w; ++x) p[size_t(y) * W + x] = Pack(kBg);
  Coverage out = Measure(masked, W, H, Pack(kBg), full, true, false);  // pixels != fundo
  info.k("outsideWritten").obj().kv("count", out.count).kv("graded", grade_outside);
  info.k("bbox").arr().v(out.x0).v(out.y0).v(out.x1 - out.x0).v(out.y1 - out.y0).end().end();
  if (grade_outside) ok &= out.count == 0;
  info.kv("png", t.Png(std::string("minimap/") + v.name, px.data(), W, H, W * 4, c.png_scale));
  return ok ? St::Pass : St::Fail;
}

bool ParseSize(const std::string& s, int& w, int& h) {
  return sscanf(s.c_str(), "%dx%d", &w, &h) == 2 && w > 0 && h > 0 && w <= 8192 && h <= 8192;
}

}  // namespace

void TestViewportScissor(Test& t) {
  Gpu g(t);
  Ctx c;
  c.g = &g;
  c.png_scale = int(t.opt.num("png_scale", 2));
  c.inject_bug = t.opt.num("inject_bug", 0) != 0;
  int W = 2400, H = 1080;
  ParseSize(t.opt.str("screen", "2400x1080"), W, H);
  if (t.Step("setup", [&](Json& info) {
        g.CreateInstance();
        DeviceReq req;
        req.dynamicRendering = true;
        g.CreateDevice(req);
        c.layout = SolidLayout(g);
        c.rp_clear = g.RenderPass(VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_UNDEFINED);
        c.rp_load = g.RenderPass(VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_LOAD,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        c.rp_clear_keep = g.RenderPass(VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        SolidPipeOpts o;
        o.rp = c.rp_clear;
        c.pipe = SolidPipeline(g, c.layout, o);
        if (g.hasDynRendering) {
          o.rp = VK_NULL_HANDLE;
          c.pipe_dr = SolidPipeline(g, c.layout, o);
        }
        info.kv("device", g.props.deviceName).kv("dynamicRendering", g.hasDynRendering)
            .k("screen").arr().v(W).v(H).end();
        return St::Pass;
      }) != St::Pass) {
    t.message = "setup falhou";
    return;
  }

  const CutVariant cuts[] = {
      {"same_cb", CutMode::SameCb, W, H, "A (960x540) e B no mesmo command buffer, estado dinamico dentro de cada passo"},
      {"same_cb_2712", CutMode::SameCb, 2712, 1220, "igual a same_cb com B 2712x1220"},
      {"separate_cb", CutMode::SeparateCb, W, H, "B noutro command buffer (mesmo vkQueueSubmit)"},
      {"separate_submit", CutMode::SeparateSubmit, W, H, "B noutro command buffer e noutro vkQueueSubmit"},
      {"state_before_begin", CutMode::StateBeforeBegin, W, H, "viewport/scissor de B definidos antes do vkCmdBeginRenderPass"},
      {"dynamic_rendering", CutMode::DynRendering, W, H, "vkCmdBeginRendering em vez de render pass"},
      {"static_state", CutMode::StaticState, W, H, "pipelines com viewport/scissor estaticos"},
  };
  std::string failed;
  int ran = 0, ok = 0;
  t.details.k("cut").obj();
  for (const CutVariant& v : cuts) {
    St st = t.Step(std::string("cut/") + v.name, [&](Json& info) { return RunCut(c, t, v, info); });
    t.details.kv(v.name, StName(st));
    if (st != St::Skip) ran++;
    if (st == St::Pass) ok++;
    if (st == St::Fail) failed += std::string(failed.empty() ? "" : ",") + "cut/" + v.name;
  }
  t.details.end();

  const MiniVariant minis[] = {
      {"area_full", MiniMode::AreaFull, {W - 204, 24, 180, 180}, "renderArea = alvo inteiro, scissor 180x180 no canto"},
      {"rect_top_left", MiniMode::AreaFull, {0, 0, 180, 180}, "scissor 180x180 em (0,0)"},
      {"rect_bottom_right", MiniMode::AreaFull, {W - 180, H - 180, 180, 180}, "scissor 180x180 no canto inferior direito"},
      {"rect_odd", MiniMode::AreaFull, {17, 33, 181, 179}, "scissor nao alinhado a tiles (17,33 181x179)"},
      {"rect_aligned64", MiniMode::AreaFull, {1024, 512, 64, 64}, "scissor alinhado a 64 (1024,512 64x64)"},
      {"area_eq_scissor", MiniMode::AreaEqScissorLoad, {W - 204, 24, 180, 180}, "renderArea == scissor (loadOp LOAD)"},
      {"area_eq_scissor_clear", MiniMode::AreaEqScissorClear, {W - 204, 24, 180, 180},
       "renderArea == scissor (loadOp CLEAR; fora so reportado)"},
      {"viewport_small", MiniMode::ViewportSmall, {W - 204, 24, 180, 180}, "viewport == scissor == 180x180"},
      {"multi_scissor", MiniMode::MultiScissor, {W - 204, 24, 180, 180}, "3 scissors no mesmo passo (draw, scissor, draw)"},
      {"after_full_pass", MiniMode::AfterFullPass, {W - 204, 24, 180, 180},
       "passo de ecra inteiro noutro alvo antes, mesmo command buffer"},
  };
  t.details.k("minimap").obj();
  for (const MiniVariant& v : minis) {
    St st = t.Step(std::string("minimap/") + v.name, [&](Json& info) { return RunMinimap(c, t, v, W, H, info); });
    t.details.kv(v.name, StName(st));
    if (st != St::Skip) ran++;
    if (st == St::Pass) ok++;
    if (st == St::Fail) failed += std::string(failed.empty() ? "" : ",") + "minimap/" + v.name;
  }
  t.details.end();
  t.details.k("screen").arr().v(W).v(H).end().kv("variantsRun", ran).kv("variantsPassed", ok);
  t.message = std::to_string(ok) + "/" + std::to_string(ran) + " variantes ok" +
              (failed.empty() ? "" : "; falharam: " + failed);
}

//-----------------------------------------------------------------------------
// clear_loadop
//-----------------------------------------------------------------------------
void TestClearLoadOp(Test& t) {
  Gpu g(t);
  Ctx c;
  c.g = &g;
  c.png_scale = int(t.opt.num("png_scale", 2));
  const int W = 1000, H = 600;
  const Rect full{0, 0, W, H};
  VkFormat depth = VK_FORMAT_UNDEFINED;
  VkRenderPass rp_depth_clear = VK_NULL_HANDLE, rp_depth_load = VK_NULL_HANDLE;
  VkPipeline pipe_depth = VK_NULL_HANDLE;
  if (t.Step("setup", [&](Json& info) {
        g.CreateInstance();
        g.CreateDevice({});
        c.layout = SolidLayout(g);
        c.rp_clear = g.RenderPass(VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_UNDEFINED);
        c.rp_load = g.RenderPass(VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_LOAD,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        c.rp_clear_keep = g.RenderPass(VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        SolidPipeOpts o;
        o.rp = c.rp_clear;
        c.pipe = SolidPipeline(g, c.layout, o);
        depth = PickDepthFormat(g);
        rp_depth_clear = g.RenderPass(VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_UNDEFINED,
                                      depth, VK_ATTACHMENT_LOAD_OP_CLEAR);
        rp_depth_load = g.RenderPass(VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_UNDEFINED,
                                     depth, VK_ATTACHMENT_LOAD_OP_LOAD);
        o.rp = rp_depth_clear;
        o.depth_format = depth;
        o.depth_test = true;
        pipe_depth = SolidPipeline(g, c.layout, o);
        info.kv("device", g.props.deviceName).kv("depthFormat", int(depth));
        return St::Pass;
      }) != St::Pass) {
    t.message = "setup falhou";
    return;
  }
  const bool stencil = depth == VK_FORMAT_D24_UNORM_S8_UINT || depth == VK_FORMAT_D32_SFLOAT_S8_UINT;
  const VkImageAspectFlags daspect = VK_IMAGE_ASPECT_DEPTH_BIT | (stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
  t.details.kv("depthFormat", int(depth)).k("target").arr().v(W).v(H).end();

  auto finish = [&](VkCommandBuffer cb, const Image& im) {
    Buffer buf = g.RecordReadback(cb, im, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    return Finish(g, {cb}, buf);
  };
  // Verifica retangulos com cores e o resto com 'rest'; grade_rest=false so reporta o resto.
  struct Exp { Rect r; const float* color; };
  auto check = [&](Json& info, const std::string& name, const std::vector<uint8_t>& px, const std::vector<Exp>& rects,
                   const float* rest, bool grade_rest) {
    bool ok = true;
    info.k("rects").arr();
    std::vector<uint8_t> masked = px;
    uint32_t* p = reinterpret_cast<uint32_t*>(masked.data());
    for (auto& e : rects) {
      Coverage in = Measure(px, W, H, Pack(e.color), e.r);
      info.obj();
      RectJson(info, "rect", e.r);
      CoverageJson(info, "inside", in);
      info.end();
      ok &= in.count == in.total;
      for (int y = e.r.y; y < e.r.y + e.r.h; ++y)
        for (int x = e.r.x; x < e.r.x + e.r.w; ++x) p[size_t(y) * W + x] = Pack(rest);
    }
    info.end();
    Coverage out = Measure(masked, W, H, Pack(rest), full);
    CoverageJson(info, grade_rest ? "rest" : "restReportedOnly", out);
    if (grade_rest) ok &= out.count == out.total;
    info.kv("png", t.Png(name, px.data(), W, H, W * 4, c.png_scale));
    return ok ? St::Pass : St::Fail;
  };
  auto color_target = [&] { return g.MakeImage(W, H, VK_FORMAT_R8G8B8A8_UNORM, kTargetUsage); };
  auto depth_target = [&] {
    return g.MakeImage(W, H, depth, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                       daspect);
  };
  auto depth_fb = [&](VkRenderPass rp, const Image& col, const Image& d) { return g.Framebuffer(rp, col, &d); };

  t.Step("clear/render_area", [&](Json& info) {
    // loadOp CLEAR com renderArea menor que a imagem; fora da area: so reportado (a spec nao garante).
    Scope s(g);
    Image im = color_target();
    VkCommandBuffer cb = g.Begin();
    g.Clear(cb, im, VK_IMAGE_LAYOUT_UNDEFINED, kBlue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const Rect area{100, 60, 333, 211};
    BeginPass(cb, c.rp_clear_keep, g.Framebuffer(c.rp_clear_keep, im), area, kYellow);
    vk.CmdEndRenderPass(cb);
    auto px = finish(cb, im);
    RectJson(info, "renderArea", area);
    return check(info, "clear/render_area", px, {{area, kYellow}}, kBlue, false);
  });

  t.Step("clear/load_after_clear", [&](Json& info) {
    Scope s(g);
    Image im = color_target();
    VkCommandBuffer cb = g.Begin();
    BeginPass(cb, c.rp_clear, g.Framebuffer(c.rp_clear, im), full, kGreen);
    vk.CmdEndRenderPass(cb);
    const Rect r{200, 100, 300, 200};
    BeginPass(cb, c.rp_load, g.Framebuffer(c.rp_load, im), full, kBlack);
    SetViewportScissor(cb, full, r);
    Draw(c, cb, c.pipe, kRed);
    vk.CmdEndRenderPass(cb);
    auto px = finish(cb, im);
    return check(info, "clear/load_after_clear", px, {{r, kRed}}, kGreen, true);
  });

  t.Step("clear/clear_attachments", [&](Json& info) {
    // Desenho branco no ecra inteiro e depois vkCmdClearAttachments em varios retangulos (ordem no passo).
    Scope s(g);
    Image im = color_target();
    VkCommandBuffer cb = g.Begin();
    BeginPass(cb, c.rp_clear, g.Framebuffer(c.rp_clear, im), full, kBlack);
    SetViewportScissor(cb, full, full);
    Draw(c, cb, c.pipe, kWhite);
    const Rect r1{10, 10, 100, 50}, r2{500, 300, 123, 77}, r3{700, 50, 64, 64};
    VkClearAttachment ca{VK_IMAGE_ASPECT_COLOR_BIT, 0, {}};
    memcpy(ca.clearValue.color.float32, kRed, 16);
    VkClearRect cr[2] = {{{{r1.x, r1.y}, {uint32_t(r1.w), uint32_t(r1.h)}}, 0, 1},
                         {{{r2.x, r2.y}, {uint32_t(r2.w), uint32_t(r2.h)}}, 0, 1}};
    vk.CmdClearAttachments(cb, 1, &ca, 2, cr);
    memcpy(ca.clearValue.color.float32, kBlue, 16);
    VkClearRect cr3{{{r3.x, r3.y}, {uint32_t(r3.w), uint32_t(r3.h)}}, 0, 1};
    vk.CmdClearAttachments(cb, 1, &ca, 1, &cr3);
    vk.CmdEndRenderPass(cb);
    auto px = finish(cb, im);
    return check(info, "clear/clear_attachments", px, {{r1, kRed}, {r2, kRed}, {r3, kBlue}}, kWhite, true);
  });

  t.Step("clear/clear_color_image", [&](Json& info) {
    Scope s(g);
    Image im = color_target();
    VkCommandBuffer cb = g.Begin();
    const float col[4] = {0.2f, 0.4f, 0.6f, 0.8f};  // 51, 102, 153, 204
    g.Clear(cb, im, VK_IMAGE_LAYOUT_UNDEFINED, col, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    auto px = finish(cb, im);
    return check(info, "clear/clear_color_image", px, {}, col, true);
  });

  t.Step("depth/loadop_clear", [&](Json& info) {
    // Profundidade limpa a 0.5: z=0.75 falha, z=0.25 passa, z=0.4 falha => tudo verde.
    Scope s(g);
    Image im = color_target(), d = depth_target();
    VkCommandBuffer cb = g.Begin();
    VkClearDepthStencilValue dv{0.5f, 0};
    BeginPass(cb, rp_depth_clear, depth_fb(rp_depth_clear, im, d), full, kBlack, &dv);
    SetViewportScissor(cb, full, full);
    Draw(c, cb, pipe_depth, kRed, 0.75f);
    Draw(c, cb, pipe_depth, kGreen, 0.25f);
    Draw(c, cb, pipe_depth, kBlue, 0.4f);
    vk.CmdEndRenderPass(cb);
    auto px = finish(cb, im);
    return check(info, "depth/loadop_clear", px, {}, kGreen, true);
  });

  t.Step("depth/clear_attachments", [&](Json& info) {
    // Depth 1.0; verde a 0.5; vkCmdClearAttachments(depth=0) num retangulo; vermelho a 0.25:
    // vermelho fora do retangulo, verde dentro.
    Scope s(g);
    Image im = color_target(), d = depth_target();
    VkCommandBuffer cb = g.Begin();
    VkClearDepthStencilValue dv{1.0f, 0};
    BeginPass(cb, rp_depth_clear, depth_fb(rp_depth_clear, im, d), full, kBlack, &dv);
    SetViewportScissor(cb, full, full);
    Draw(c, cb, pipe_depth, kGreen, 0.5f);
    const Rect r{100, 100, 200, 150};
    VkClearAttachment ca{VK_IMAGE_ASPECT_DEPTH_BIT, 0, {}};
    ca.clearValue.depthStencil = {0.0f, 0};
    VkClearRect cr{{{r.x, r.y}, {uint32_t(r.w), uint32_t(r.h)}}, 0, 1};
    vk.CmdClearAttachments(cb, 1, &ca, 1, &cr);
    Draw(c, cb, pipe_depth, kRed, 0.25f);
    vk.CmdEndRenderPass(cb);
    auto px = finish(cb, im);
    return check(info, "depth/clear_attachments", px, {{r, kGreen}}, kRed, true);
  });

  t.Step("depth/clear_image", [&](Json& info) {
    // vkCmdClearDepthStencilImage a 0.3 e passo com depth LOAD: z=0.5 falha, z=0.2 passa => verde.
    Scope s(g);
    Image im = color_target(), d = depth_target();
    VkCommandBuffer cb = g.Begin();
    g.Barrier(cb, d.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
              daspect);
    VkClearDepthStencilValue dv{0.3f, 0};
    VkImageSubresourceRange range{daspect, 0, 1, 0, 1};
    vk.CmdClearDepthStencilImage(cb, d.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &dv, 1, &range);
    g.Barrier(cb, d.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
              VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, daspect);
    BeginPass(cb, rp_depth_load, depth_fb(rp_depth_load, im, d), full, kBlack, &dv);
    SetViewportScissor(cb, full, full);
    Draw(c, cb, pipe_depth, kRed, 0.5f);
    Draw(c, cb, pipe_depth, kGreen, 0.2f);
    vk.CmdEndRenderPass(cb);
    auto px = finish(cb, im);
    return check(info, "depth/clear_image", px, {}, kGreen, true);
  });

  t.message = std::to_string(t.passes - 1) + " ok, " + std::to_string(t.fails) + " falharam, " +
              std::to_string(t.skips) + " skip";
}

}  // namespace zdiag
