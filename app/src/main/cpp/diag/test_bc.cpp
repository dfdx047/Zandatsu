// zdiag: bc_decode — (i) o shader de compute do Renut (bc_decode_cs.hlsl) contra um descodificador de
// referencia na CPU, byte a byte; (ii) a descodificacao BC nativa do driver (imagem BC lida com texelFetch).
//
// Referencia: regras BCn da Vulkan Data Format spec ("S3TC" e "RGTC"), com as mesmas formulas inteiras do
// shader do Renut (expansao 565 por replicacao de bits, (2a+b+1)/3, (a+b)/2 truncado no modo de 3 cores,
// alfa (k*a0+j*a1+3)/7 e +2)/5). BC2/BC3 usam sempre o modo de 4 cores.
#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "gpu.h"

namespace zdiag {
namespace {

constexpr int kBlocksW = 64, kBlocksH = 16, kBlocks = kBlocksW * kBlocksH;
constexpr int kTexW = kBlocksW * 4, kTexH = kBlocksH * 4;
const char* kFmtName[6] = {"", "bc1", "bc2", "bc3", "bc4", "bc5"};
const VkFormat kVkFmt[6] = {VK_FORMAT_UNDEFINED, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_FORMAT_BC2_UNORM_BLOCK,
                            VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_BC4_UNORM_BLOCK, VK_FORMAT_BC5_UNORM_BLOCK};

int BlockBytes(int f) { return f == 1 || f == 4 ? 8 : 16; }
int TexelBytes(int f) { return f == 4 ? 1 : f == 5 ? 2 : 4; }

struct Rng {
  uint64_t s;
  uint32_t next() {
    s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
    return uint32_t((s * 0x2545F4914F6CDD1Dull) >> 32);
  }
};

uint16_t Rd16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t Rd32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
void Wr16(uint8_t* p, uint16_t v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); }

// Casos limite nos primeiros blocos; o resto e aleatorio (os dois modos aparecem ~50/50).
void EdgeColor(uint8_t* b, int i, Rng& r) {
  uint16_t c0 = Rd16(b), c1 = Rd16(b + 2);
  switch (i % 8) {
    case 0: if (c0 <= c1) { std::swap(c0, c1); if (c0 == c1) c0++; } break;   // c0 > c1: 4 cores
    case 1: if (c0 >= c1) { std::swap(c0, c1); if (c0 == c1) c1++; } break;   // c0 < c1: 3 cores + transparente
    case 2: c1 = c0; break;                                                   // c0 == c1
    case 3: c0 = 0xFFFF; c1 = 0; break;
    case 4: c0 = 0; c1 = 0xFFFF; break;
    case 5: if (c0 > c1) std::swap(c0, c1); memset(b + 4, 0xFF, 4); break;    // indice 3 em modo de 3 cores
    case 6: memset(b + 4, 0x00, 4); break;
    default: (void)r; break;
  }
  Wr16(b, c0);
  Wr16(b + 2, c1);
}

void EdgeAlpha(uint8_t* b, int i) {
  uint8_t& a0 = b[0];
  uint8_t& a1 = b[1];
  switch (i % 8) {
    case 0: if (a0 <= a1) { std::swap(a0, a1); if (a0 == a1) a0 = a0 == 255 ? 255 : a0 + 1, a1 = a0 - 1; } break;
    case 1: if (a0 > a1) std::swap(a0, a1); break;  // a0 <= a1: 6 valores + 0 e 255
    case 2: a1 = a0; break;
    case 3: a0 = 255; a1 = 0; break;
    case 4: a0 = 0; a1 = 255; break;
    case 5: if (a0 > a1) std::swap(a0, a1); memset(b + 2, 0xFF, 6); break;  // indice 7 => 255
    case 6: {  // indice 6 em todos (110b) => 0 no modo a0 <= a1
      if (a0 > a1) std::swap(a0, a1);
      uint64_t bits = 0;
      for (int k = 0; k < 16; ++k) bits |= uint64_t(6) << (3 * k);
      for (int k = 0; k < 6; ++k) b[2 + k] = uint8_t(bits >> (8 * k));
      break;
    }
    default: break;
  }
}

std::vector<uint8_t> GenBlocks(int f) {
  Rng r{0x9E3779B97F4A7C15ull ^ uint64_t(f) * 0x1234567ull};
  const int bb = BlockBytes(f);
  std::vector<uint8_t> v(size_t(kBlocks) * bb);
  for (auto& x : v) x = uint8_t(r.next());
  for (int i = 0; i < 64; ++i) {
    uint8_t* b = &v[size_t(i) * bb];
    if (f == 1) EdgeColor(b, i, r);
    if (f == 2 || f == 3) EdgeColor(b + 8, i, r);
    if (f == 3 || f == 4 || f == 5) EdgeAlpha(b, i);
    if (f == 5) EdgeAlpha(b + 8, i + 3);
  }
  return v;
}

void Rgb565(uint16_t c, int o[3]) {
  int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  o[0] = (r << 3) | (r >> 2);
  o[1] = (g << 2) | (g >> 4);
  o[2] = (b << 3) | (b >> 2);
}

void DecodeColor(const uint8_t* b, bool opaque, uint8_t out[16][4]) {
  const uint16_t c0 = Rd16(b), c1 = Rd16(b + 2);
  int a[3], c[3], pal[4][4];
  Rgb565(c0, a);
  Rgb565(c1, c);
  for (int k = 0; k < 3; ++k) {
    pal[0][k] = a[k];
    pal[1][k] = c[k];
    if (opaque || c0 > c1) {
      pal[2][k] = (2 * a[k] + c[k] + 1) / 3;
      pal[3][k] = (a[k] + 2 * c[k] + 1) / 3;
    } else {
      pal[2][k] = (a[k] + c[k]) / 2;
      pal[3][k] = 0;
    }
  }
  pal[0][3] = pal[1][3] = pal[2][3] = 255;
  pal[3][3] = (opaque || c0 > c1) ? 255 : 0;
  const uint32_t idx = Rd32(b + 4);
  for (int i = 0; i < 16; ++i)
    for (int k = 0; k < 4; ++k) out[i][k] = uint8_t(pal[(idx >> (2 * i)) & 3][k]);
}

void DecodeAlpha(const uint8_t* b, uint8_t out[16]) {
  const int a0 = b[0], a1 = b[1];
  uint64_t bits = 0;
  for (int k = 0; k < 6; ++k) bits |= uint64_t(b[2 + k]) << (8 * k);
  for (int i = 0; i < 16; ++i) {
    const int idx = int((bits >> (3 * i)) & 7);
    int v;
    if (idx == 0) v = a0;
    else if (idx == 1) v = a1;
    else if (a0 > a1) v = ((8 - idx) * a0 + (idx - 1) * a1 + 3) / 7;
    else if (idx == 6) v = 0;
    else if (idx == 7) v = 255;
    else v = ((6 - idx) * a0 + (idx - 1) * a1 + 2) / 5;
    out[i] = uint8_t(v);
  }
}

// Referencia no layout de saida do shader: linhas de texels, TexelBytes(f) por texel.
std::vector<uint8_t> Reference(int f, const std::vector<uint8_t>& blocks) {
  const int tb = TexelBytes(f), pitch = kTexW * tb, bb = BlockBytes(f);
  std::vector<uint8_t> out(size_t(pitch) * kTexH);
  for (int blk = 0; blk < kBlocks; ++blk) {
    const uint8_t* b = &blocks[size_t(blk) * bb];
    const int bx = blk % kBlocksW, by = blk / kBlocksW;
    uint8_t rgba[16][4] = {};
    if (f <= 3) {
      DecodeColor(f == 1 ? b : b + 8, f != 1, rgba);
      if (f == 2) {
        for (int i = 0; i < 16; ++i) rgba[i][3] = uint8_t(((b[i / 2] >> (4 * (i & 1))) & 15) * 17);
      } else if (f == 3) {
        uint8_t a[16];
        DecodeAlpha(b, a);
        for (int i = 0; i < 16; ++i) rgba[i][3] = a[i];
      }
    } else {
      uint8_t r[16], g[16] = {};
      DecodeAlpha(b, r);
      if (f == 5) DecodeAlpha(b + 8, g);
      for (int i = 0; i < 16; ++i) { rgba[i][0] = r[i]; rgba[i][1] = g[i]; }
    }
    for (int i = 0; i < 16; ++i) {
      uint8_t* d = &out[size_t(by * 4 + i / 4) * pitch + size_t(bx * 4 + i % 4) * tb];
      memcpy(d, rgba[i], tb);
    }
  }
  return out;
}

// Para o PNG e a comparacao com a amostragem nativa: RGBA8 como o shader veria (BC4 = r,0,0,1; BC5 = r,g,0,1).
std::vector<uint8_t> ToRgba(int f, const std::vector<uint8_t>& texels) {
  const int tb = TexelBytes(f);
  std::vector<uint8_t> out(size_t(kTexW) * kTexH * 4);
  for (size_t i = 0; i < size_t(kTexW) * kTexH; ++i) {
    uint8_t* d = &out[i * 4];
    d[0] = d[1] = d[2] = 0;
    d[3] = 255;
    memcpy(d, &texels[i * tb], tb);
  }
  return out;
}

void MismatchJson(Json& j, int f, int x, int y, const uint8_t* got, const uint8_t* exp, int n) {
  (void)f;
  j.k("first").obj().kv("block", (y / 4) * kBlocksW + x / 4).kv("x", x).kv("y", y);
  j.k("got").arr();
  for (int k = 0; k < n; ++k) j.v(got[k]);
  j.end().k("expected").arr();
  for (int k = 0; k < n; ++k) j.v(exp[k]);
  j.end().end();
}

}  // namespace

void TestBcDecode(Test& t) {
  Gpu g(t);
  if (t.Step("setup", [&](Json& info) {
        g.CreateInstance();
        DeviceReq req;
        req.bda = true;
        req.bc = true;
        g.CreateDevice(req);
        info.kv("device", g.props.deviceName).kv("bufferDeviceAddress", g.hasBda).kv("shaderInt64", g.hasInt64)
            .kv("textureCompressionBC", g.hasBc).kv("renutShader", BcDecodeShader() != nullptr);
        return St::Pass;
      }) != St::Pass) {
    t.message = "setup falhou";
    return;
  }
  std::vector<uint8_t> blocks[6], ref[6];
  t.Step("generate", [&](Json& info) {
    for (int f = 1; f <= 5; ++f) {
      blocks[f] = GenBlocks(f);
      ref[f] = Reference(f, blocks[f]);
    }
    info.kv("blocksPerFormat", kBlocks).kv("edgeCaseBlocks", 64);
    return St::Pass;
  });
  t.details.kv("blocksW", kBlocksW).kv("blocksH", kBlocksH)
      .kv("reference", "Vulkan Data Format spec com as formulas inteiras do bc_decode_cs.hlsl");

  // (i) shader do Renut
  std::string summary;
  t.details.k("compute").obj();
  std::string reason;
  if (!BcDecodeShader()) reason = "build sem bc_decode_cs.spirv.h";
  else if (!g.hasBda) reason = "sem bufferDeviceAddress";
  else if (!g.hasInt64) reason = "sem shaderInt64";
  t.details.kv("available", reason.empty());
  if (!reason.empty()) {
    t.details.kv("reason", reason);
    for (int f = 1; f <= 5; ++f) t.SkipStep(std::string("compute/") + kFmtName[f], reason);
    summary = "compute: skip (" + reason + ")";
  } else {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipe = VK_NULL_HANDLE;
    t.Step("compute/pipeline", [&](Json& info) {
      layout = g.Layout({}, 48, VK_SHADER_STAGE_COMPUTE_BIT);
      const Spirv* sp = BcDecodeShader();
      pipe = g.ComputePipeline(g.ShaderFromWords(sp->code, sp->bytes), layout);
      info.kv("spirvBytes", sp->bytes);
      return St::Pass;
    });
    int total_bad = 0;
    for (int f = 1; f <= 5 && pipe; ++f) {
      St st = t.Step(std::string("compute/") + kFmtName[f], [&](Json& info) {
        Scope s(g);
        const int tb = TexelBytes(f), pitch = kTexW * tb;
        Buffer src = g.MakeBuffer(blocks[f].size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, true);
        memcpy(src.map, blocks[f].data(), blocks[f].size());
        if (!src.coherent) {
          VkMappedMemoryRange r{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, src.mem, 0, VK_WHOLE_SIZE};
          vk.FlushMappedMemoryRanges(g.dev, 1, &r);
        }
        Buffer dst = g.MakeBuffer(ref[f].size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  true, true, true);
        VkCommandBuffer cb = g.Begin();
        vk.CmdFillBuffer(cb, dst.buf, 0, VK_WHOLE_SIZE, 0xCDCDCDCDu);
        g.MemoryBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
        struct {
          uint64_t src, dst;
          uint32_t blocksW, blocksH, blockCount, format, srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch;
        } pc{src.addr, dst.addr, kBlocksW, kBlocksH, kBlocks, uint32_t(f), uint32_t(kBlocksW * BlockBytes(f)),
             uint32_t(blocks[f].size()), uint32_t(pitch), uint32_t(ref[f].size())};
        static_assert(sizeof(pc) == 48);
        vk.CmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
        vk.CmdPushConstants(cb, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 48, &pc);
        vk.CmdDispatch(cb, (kBlocks + 63) / 64, 1, 1);
        g.MemoryBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                        VK_ACCESS_HOST_READ_BIT);
        g.Submit({cb});
        auto got = g.ReadMapped(dst);
        int bad_blocks = 0, bad_texels = 0;
        bool first = true;
        std::vector<bool> bad(kBlocks, false);
        for (int y = 0; y < kTexH; ++y)
          for (int x = 0; x < kTexW; ++x) {
            const size_t o = size_t(y) * pitch + size_t(x) * tb;
            if (memcmp(&got[o], &ref[f][o], tb)) {
              bad_texels++;
              const int blk = (y / 4) * kBlocksW + x / 4;
              if (!bad[blk]) { bad[blk] = true; bad_blocks++; }
              if (first) { MismatchJson(info, f, x, y, &got[o], &ref[f][o], tb); first = false; }
            }
          }
        info.kv("blocks", kBlocks).kv("mismatchBlocks", bad_blocks).kv("mismatchTexels", bad_texels)
            .kv("gpuMs", std::round(g.lastSubmitCpuMs * 100) / 100);
        auto rgba = ToRgba(f, got);
        info.kv("png", t.Png(std::string("compute/") + kFmtName[f], rgba.data(), kTexW, kTexH, kTexW * 4, 1, f <= 3));
        t.details.k(kFmtName[f]).obj().kv("mismatchBlocks", bad_blocks).kv("mismatchTexels", bad_texels).end();
        total_bad += bad_blocks;
        return bad_blocks ? St::Fail : St::Pass;
      });
      (void)st;
    }
    summary = "compute: " + std::to_string(total_bad) + " blocos errados";
  }
  t.details.end();

  // (ii) descodificacao nativa do driver
  const int tol = int(t.opt.num("bc_tol", 4));
  t.details.k("native").obj().kv("tolerance", tol).kv("textureCompressionBC", g.hasBc);
  int native_bad = 0, native_run = 0;
  if (!g.hasBc) {
    for (int f = 1; f <= 5; ++f) t.SkipStep(std::string("native/") + kFmtName[f], "sem textureCompressionBC");
  } else {
    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipe = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    t.Step("native/pipeline", [&](Json&) {
      VkDescriptorSetLayoutBinding b[2] = {
          {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
          {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
      VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
      ci.bindingCount = 2;
      ci.pBindings = b;
      VKC(vk.CreateDescriptorSetLayout(g.dev, &ci, nullptr, &dsl));
      VkDevice dev = g.dev;
      VkDescriptorSetLayout d = dsl;
      g.Defer([dev, d] { vk.DestroyDescriptorSetLayout(dev, d, nullptr); });
      layout = g.Layout({dsl}, 8, VK_SHADER_STAGE_COMPUTE_BIT);
      pipe = g.ComputePipeline(g.Shader("bc_fetch.comp"), layout);
      VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
      si.magFilter = si.minFilter = VK_FILTER_NEAREST;
      si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      si.maxLod = 0.0f;
      VKC(vk.CreateSampler(g.dev, &si, nullptr, &sampler));
      VkSampler smp = sampler;
      g.Defer([dev, smp] { vk.DestroySampler(dev, smp, nullptr); });
      return St::Pass;
    });
    for (int f = 1; f <= 5 && pipe; ++f) {
      VkFormatProperties fp;
      vk.GetPhysicalDeviceFormatProperties(g.pd, kVkFmt[f], &fp);
      if (!(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
        t.SkipStep(std::string("native/") + kFmtName[f], "formato sem SAMPLED_IMAGE (optimal)");
        t.details.k(kFmtName[f]).obj().kv("supported", false).end();
        continue;
      }
      native_run++;
      t.Step(std::string("native/") + kFmtName[f], [&](Json& info) {
        Scope s(g);
        Image im = g.MakeImage(kTexW, kTexH, kVkFmt[f], VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        Buffer stage = g.MakeBuffer(blocks[f].size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true);
        memcpy(stage.map, blocks[f].data(), blocks[f].size());
        if (!stage.coherent) {
          VkMappedMemoryRange r{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, stage.mem, 0, VK_WHOLE_SIZE};
          vk.FlushMappedMemoryRanges(g.dev, 1, &r);
        }
        Buffer out = g.MakeBuffer(size_t(kTexW) * kTexH * 4,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, false, true);
        VkDescriptorPoolSize ps[2] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};
        VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pci.maxSets = 1;
        pci.poolSizeCount = 2;
        pci.pPoolSizes = ps;
        VkDescriptorPool pool;
        VKC(vk.CreateDescriptorPool(g.dev, &pci, nullptr, &pool));
        VkDevice dev = g.dev;
        g.Defer([dev, pool] { vk.DestroyDescriptorPool(dev, pool, nullptr); });
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &dsl;
        VkDescriptorSet set;
        VKC(vk.AllocateDescriptorSets(g.dev, &ai, &set));
        VkDescriptorImageInfo ii{sampler, im.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorBufferInfo bi{out.buf, 0, VK_WHOLE_SIZE};
        VkWriteDescriptorSet w[2]{};
        w[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &ii};
        w[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bi};
        vk.UpdateDescriptorSets(g.dev, 2, w, 0, nullptr);

        VkCommandBuffer cb = g.Begin();
        g.Barrier(cb, im.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        VkBufferImageCopy c{};
        c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        c.imageExtent = {uint32_t(kTexW), uint32_t(kTexH), 1};
        vk.CmdCopyBufferToImage(cb, stage.buf, im.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c);
        g.Barrier(cb, im.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                  VK_ACCESS_SHADER_READ_BIT);
        vk.CmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
        vk.CmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, nullptr);
        uint32_t pc[2] = {uint32_t(kTexW), uint32_t(kTexH)};
        vk.CmdPushConstants(cb, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 8, pc);
        vk.CmdDispatch(cb, (kTexW + 7) / 8, (kTexH + 7) / 8, 1);
        g.MemoryBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                        VK_ACCESS_HOST_READ_BIT);
        g.Submit({cb});
        auto got = g.ReadMapped(out);
        auto exp = ToRgba(f, ref[f]);
        int exact = 0, over = 0, max_diff = 0, bad_blocks = 0;
        std::vector<bool> bad(kBlocks, false);
        bool first = true;
        for (int i = 0; i < kTexW * kTexH; ++i) {
          int d = 0;
          for (int k = 0; k < 4; ++k) d = std::max(d, std::abs(int(got[i * 4 + k]) - int(exp[i * 4 + k])));
          if (!d) continue;
          exact++;
          max_diff = std::max(max_diff, d);
          if (d > tol) {
            over++;
            const int x = i % kTexW, y = i / kTexW, blk = (y / 4) * kBlocksW + x / 4;
            if (!bad[blk]) { bad[blk] = true; bad_blocks++; }
            if (first) { MismatchJson(info, f, x, y, &got[i * 4], &exp[i * 4], 4); first = false; }
          }
        }
        info.kv("format", int(kVkFmt[f])).kv("inexactTexels", exact).kv("maxDiff", max_diff)
            .kv("overToleranceTexels", over).kv("mismatchBlocks", bad_blocks);
        if (exact) info.kv("png", t.Png(std::string("native/") + kFmtName[f], got.data(), kTexW, kTexH, kTexW * 4, 1, f <= 3));
        t.details.k(kFmtName[f]).obj().kv("supported", true).kv("inexactTexels", exact).kv("maxDiff", max_diff)
            .kv("overToleranceTexels", over).kv("mismatchBlocks", bad_blocks).end();
        if (bad_blocks) native_bad++;
        return bad_blocks ? St::Fail : St::Pass;
      });
    }
  }
  t.details.end();
  summary += "; nativo: " + (g.hasBc ? std::to_string(native_run - native_bad) + "/" + std::to_string(native_run) +
                                           " formatos ok"
                                     : std::string("sem BC"));
  t.message = summary;
}

}  // namespace zdiag
