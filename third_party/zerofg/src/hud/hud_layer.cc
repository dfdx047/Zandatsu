// The HUD layer: see hud_layer.h. Three compute passes around the engine, in the host's command buffer:
//
//   occupancy  which 16 x 16 tiles hold a HUD, from one plane sample in every 4 x 4 block    shaders/hud/hud_occupancy.comp
//   prep       one tile per workgroup. Far from any HUD: the Reals copied. Near one: its flags in shared memory with a 2 px apron,
//              keep = a HUD pixel (A or B) with nothing to leave within 2 px (the HUD differs between A and B, or a pair of planes cannot
//              explain its Real); the HUD-free picture there, the Real elsewhere, for A and B; the tiles with a kept pixel listed for the
//              compose pass                                                                  shaders/hud/hud_prep.comp
//   (the engine, once, on the two pictures, straight into the output)
//   compose    dispatched indirectly over the listed tiles only: B's HUD back on the kept pixels, in place  shaders/hud/hud_compose.comp
//
// The planes: p = the HUD drawn over black, w = over white, so Q = w - p (per channel), at the Reals' size (their own active rects). The
// HUD-free pictures go to the engine with sequence 0: they are this pair's pictures only. The cost is the copy of the Reals into them
// (bandwidth: on the Adreno 840 about 0.18 ms per 1024 x 896 picture), the planes' reads and the compose pass over the HUD's tiles. When a
// pair starts at the Real the one before ended on (Image::sequence and the geometry), the picture made for that Real as B is this pair's A:
// only the tiles where it differs from the Real are copied again, so the steady state copies one picture per pair, not two. A host gives
// no planes for a frame with no HUD.

#include "hud/hud_layer.h"

#include <algorithm>
#include <array>
#include <vector>

#include "vulkan_buffer.h"
#include "vulkan_dispatch.h"
#include "vulkan_image.h"

#include "hud_compose.h"
#include "hud_occupancy.h"
#include "hud_prep.h"

namespace zerofg {
namespace {

constexpr VkFormat kWorldFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
constexpr VkPipelineStageFlags2 kCompute = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
constexpr uint32_t kTile = 16;

uint32_t Groups(uint32_t extent, uint32_t local) { return (extent + local - 1) / local; }

ActiveRect ResolveRect(const Image& image) {
  if (image.active_rect.width == 0 || image.active_rect.height == 0) {
    return {0, 0, image.width, image.height};
  }
  return image.active_rect;
}

// shaders/hud/hud_common.glsl
struct HudPush {
  int32_t real_origin[2], extent[2], p_a_origin[2], w_a_origin[2], p_b_origin[2], w_b_origin[2];
  int32_t reuse_a;
};
static_assert(sizeof(HudPush) == 52);

// What the picture made for a pair's B was made from: the next pair may start from it only if its A is the same Real.
struct RealIdentity {
  uint64_t sequence = 0;
  uint32_t width = 0, height = 0;
  VkFormat format = VK_FORMAT_UNDEFINED;
  ActiveRect rect;
};

struct Pass {
  VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkDescriptorPool pool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> sets;  // one per frame context
};

enum Stage : uint32_t { kOccupancy, kPrep, kCompose, kStages };

}  // namespace

class HudLayer::Impl {
 public:
  explicit Impl(const CreateInfo& create_info)
      : vulkan_(create_info.vulkan), capabilities_(create_info.capabilities), slots_(std::max(1u, create_info.frame_context_count)) {}

  ~Impl() { Destroy(); }

  bool Initialize() {
    if (!dispatch_.Load(vulkan_.instance, vulkan_.device, vulkan_.get_instance_proc_addr) || !dispatch_.cmd_pipeline_barrier_2 ||
        !dispatch_.cmd_blit_image || !dispatch_.create_sampler || !dispatch_.cmd_fill_buffer) {
      return false;
    }
    dispatch_indirect_ =
        reinterpret_cast<PFN_vkCmdDispatchIndirect>(dispatch_.get_device_proc_addr(vulkan_.device, "vkCmdDispatchIndirect"));
    if (!dispatch_indirect_) return false;
    VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_info.addressModeU = sampler_info.addressModeV = sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (dispatch_.create_sampler(vulkan_.device, &sampler_info, vulkan_.allocator, &sampler_) != VK_SUCCESS) {
      return false;
    }
    const VkDescriptorType kSampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    const VkDescriptorType kImage = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    const VkDescriptorType kBuffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    std::vector<VkDescriptorType> prep(6, kSampler);
    prep.insert(prep.end(), {kImage, kImage, kImage, kBuffer, kBuffer, kBuffer, kBuffer});
    return MakePass(&passes_[kOccupancy], zerofg_hud_occupancy_spv, sizeof(zerofg_hud_occupancy_spv),
                    {kSampler, kSampler, kSampler, kSampler, kBuffer}) &&
           MakePass(&passes_[kPrep], zerofg_hud_prep_spv, sizeof(zerofg_hud_prep_spv), prep) &&
           MakePass(&passes_[kCompose], zerofg_hud_compose_spv, sizeof(zerofg_hud_compose_spv), {kSampler, kSampler, kImage, kImage, kBuffer});
  }

  Status Resize(uint32_t width, uint32_t height, VkFormat input_format, VkFormat output_format) {
    DestroyResources();
    ready_ = false;
    // The HUD-free pictures are the engine's Reals, and the compose pass works on the engine's output in place: both are rgb10_a2.
    if (input_format != kWorldFormat || output_format != kWorldFormat || !capabilities_.shader_storage_image_extended_formats_enabled) {
      return Status::kUnsupported;
    }
    const VkFormatFeatureFlags work = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if (!HasOptimalFeatures(VK_FORMAT_R8_UNORM, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) || !HasOptimalFeatures(kWorldFormat, work)) {
      return Status::kUnsupported;
    }
    width_ = width;
    height_ = height;
    input_format_ = input_format;
    const VkImageUsageFlags storage_sampled = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    Status status = keep_.Create(vulkan_, dispatch_, width, height, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    for (OwnedImage& world : world_) {
      if (status == Status::kSuccess) status = world.Create(vulkan_, dispatch_, width, height, kWorldFormat, storage_sampled);
    }
    const VkDeviceSize tiles = VkDeviceSize(Groups(width, kTile)) * Groups(height, kTile);
    if (status == Status::kSuccess) {
      status = tiles_.Create(vulkan_, dispatch_, 16 + tiles * 4,
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }
    if (status == Status::kSuccess) status = occupancy_.Create(vulkan_, dispatch_, tiles * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    for (OwnedBuffer& listed : listed_) {
      if (status == Status::kSuccess) status = listed.Create(vulkan_, dispatch_, tiles * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    }
    if (status != Status::kSuccess) {
      DestroyResources();
      return status;
    }
    ready_ = true;
    return Status::kSuccess;
  }

  Status Record(VkCommandBuffer cmd, const Image& previous, const Image& current, const Interpolator::HudPlanes& previous_hud,
                const Interpolator::HudPlanes& current_hud, const Image& output, const Generate& generate) {
    if (!ready_ || cmd == VK_NULL_HANDLE) return Status::kUnsupported;
    const ActiveRect rect = ResolveRect(previous), rect_b = ResolveRect(current);
    const auto usable = [](const Image& image) {
      return image.image != VK_NULL_HANDLE && image.view != VK_NULL_HANDLE && image.layout != VK_IMAGE_LAYOUT_UNDEFINED &&
             (image.usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0;
    };
    const auto plane_fits = [&](const Image& image) {
      const ActiveRect r = ResolveRect(image);
      return usable(image) && r.width == width_ && r.height == height_ && r.x <= image.width - r.width && r.y <= image.height - r.height;
    };
    if (!usable(previous) || !usable(current) || !plane_fits(previous_hud.p) || !plane_fits(previous_hud.w) || !plane_fits(current_hud.p) ||
        !plane_fits(current_hud.w) || previous.format != input_format_ || current.format != input_format_ || rect.width != width_ ||
        rect.height != height_ || rect_b.x != rect.x || rect_b.y != rect.y || rect_b.width != rect.width || rect_b.height != rect.height ||
        output.image == VK_NULL_HANDLE || output.view == VK_NULL_HANDLE || output.width != width_ || output.height != height_ ||
        output.format != kWorldFormat || output.layout == VK_IMAGE_LAYOUT_UNDEFINED) {
      return Status::kUnsupported;
    }
    // The compose pass writes the output in place: it needs storage. An output without it gets the engine's picture in an internal image,
    // composed there and blitted (the engine's own fallback needs TRANSFER_DST on the output just the same).
    const bool in_place = (output.usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0;
    if (!in_place) {
      if ((output.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) return Status::kUnsupported;
      if (picture_.image() == VK_NULL_HANDLE &&
          picture_.Create(vulkan_, dispatch_, width_, height_, kWorldFormat,
                          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != Status::kSuccess) {
        return Status::kUnsupported;
      }
    }
    const ActiveRect pa = ResolveRect(previous_hud.p), wa = ResolveRect(previous_hud.w);
    const ActiveRect pb = ResolveRect(current_hud.p), wb = ResolveRect(current_hud.w);

    // This pair's A is the Real the pair before ended on: the picture made for it then is this pair's A (hud_prep.comp, reuse_a).
    const bool reuse_a = has_b_ && previous.sequence != 0 && previous.sequence == b_identity_.sequence &&
                         previous.width == b_identity_.width && previous.height == b_identity_.height &&
                         previous.format == b_identity_.format && rect.x == b_identity_.rect.x && rect.y == b_identity_.rect.y &&
                         rect.width == b_identity_.rect.width && rect.height == b_identity_.rect.height;
    const uint32_t ia = reuse_a ? b_index_ : 0u, ib = 1u - ia;
    has_b_ = false;  // set again only when this record succeeds

    // The record before may still be running: this one takes the next copy of the sets.
    slot_ = (slot_ + 1) % slots_;
    BindSampled(kOccupancy, 0, previous_hud.p.view, previous_hud.p.layout);
    BindSampled(kOccupancy, 1, previous_hud.w.view, previous_hud.w.layout);
    BindSampled(kOccupancy, 2, current_hud.p.view, current_hud.p.layout);
    BindSampled(kOccupancy, 3, current_hud.w.view, current_hud.w.layout);
    BindBuffer(kOccupancy, 4, occupancy_.buffer());
    BindSampled(kPrep, 0, previous.view, previous.layout);
    BindSampled(kPrep, 1, current.view, current.layout);
    BindSampled(kPrep, 2, previous_hud.p.view, previous_hud.p.layout);
    BindSampled(kPrep, 3, previous_hud.w.view, previous_hud.w.layout);
    BindSampled(kPrep, 4, current_hud.p.view, current_hud.p.layout);
    BindSampled(kPrep, 5, current_hud.w.view, current_hud.w.layout);
    BindStorage(kPrep, 6, world_[ia].view());
    BindStorage(kPrep, 7, world_[ib].view());
    BindStorage(kPrep, 8, keep_.view());
    BindBuffer(kPrep, 9, tiles_.buffer());
    BindBuffer(kPrep, 10, occupancy_.buffer());
    BindBuffer(kPrep, 11, listed_[ia].buffer());  // what the pair before marked in world_[ia], when it was its B
    BindBuffer(kPrep, 12, listed_[ib].buffer());
    BindSampled(kCompose, 0, current_hud.p.view, current_hud.p.layout);
    BindSampled(kCompose, 1, current_hud.w.view, current_hud.w.layout);
    BindStorage(kCompose, 2, keep_.view());
    BindStorage(kCompose, 3, in_place ? output.view : picture_.view());
    BindBuffer(kCompose, 4, tiles_.buffer());

    const HudPush push{{int32_t(rect.x), int32_t(rect.y)}, {int32_t(width_), int32_t(height_)}, {int32_t(pa.x), int32_t(pa.y)},
                       {int32_t(wa.x), int32_t(wa.y)},     {int32_t(pb.x), int32_t(pb.y)},     {int32_t(wb.x), int32_t(wb.y)},
                       reuse_a ? 1 : 0};

    // What the earlier submissions wrote (and read) is settled before this record rewrites its images and the tile list.
    MemoryBarrier(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                  kCompute | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT);
    dispatch_.cmd_fill_buffer(cmd, tiles_.buffer(), 0, 4, 0u);  // no tile listed
    dispatch_.cmd_fill_buffer(cmd, tiles_.buffer(), 4, 8, 1u);  // y = z = 1
    MemoryBarrier(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, kCompute,
                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);
    for (VkImage image : {keep_.image(), world_[ib].image()}) {
      ImageBarrier(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0, kCompute,
                   VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
    if (reuse_a) {  // its content stays: from where the pair before left it
      ImageBarrier(cmd, world_[ia].image(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                   VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, kCompute,
                   VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    } else {
      ImageBarrier(cmd, world_[ia].image(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
                   kCompute, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
    Dispatch(cmd, kOccupancy, push, Groups(Groups(width_, kTile), 4), Groups(Groups(height_, kTile), 4));
    MemoryBarrier(cmd, kCompute, VK_ACCESS_2_SHADER_WRITE_BIT, kCompute, VK_ACCESS_2_SHADER_READ_BIT);
    Dispatch(cmd, kPrep, push, Groups(width_, kTile), Groups(height_, kTile));
    MemoryBarrier(cmd, kCompute, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
    // The engine reads the HUD-free pictures many times: read-only layout, as the Reals it would otherwise get.
    for (VkImage image : {world_[0].image(), world_[1].image()}) {
      ImageBarrier(cmd, image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kCompute, VK_ACCESS_2_SHADER_WRITE_BIT,
                   VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    }

    // The engine, once, on the HUD-free pictures, into the output (or the internal picture).
    Image world_a{};
    world_a.image = world_[ia].image();
    world_a.view = world_[ia].view();
    world_a.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    world_a.format = kWorldFormat;
    world_a.width = width_;
    world_a.height = height_;
    world_a.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    Image world_b = world_a;
    world_b.image = world_[ib].image();
    world_b.view = world_[ib].view();
    Image target = output;
    if (!in_place) {
      ImageBarrier(cmd, picture_.image(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
                   VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);
      target = world_a;
      target.layout = VK_IMAGE_LAYOUT_GENERAL;
      target.image = picture_.image();
      target.view = picture_.view();
      target.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    Image screen{};
    screen.image = keep_.image();
    screen.view = keep_.view();
    screen.layout = VK_IMAGE_LAYOUT_GENERAL;
    screen.format = VK_FORMAT_R8_UNORM;
    screen.width = width_;
    screen.height = height_;
    screen.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    const Status status = generate(world_a, world_b, target, screen);
    if (status != Status::kSuccess) return status;
    // world_[ib] is current's picture from here on (both images end this record read-only).
    has_b_ = current.sequence != 0;
    b_index_ = ib;
    b_identity_ = {current.sequence, current.width, current.height, current.format, rect_b};

    // B's HUD back on the kept pixels of the listed tiles, in place.
    const VkImage picture = in_place ? output.image : picture_.image();
    const VkImageLayout picture_layout = in_place ? output.layout : VK_IMAGE_LAYOUT_GENERAL;
    ImageBarrier(cmd, picture, picture_layout, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                 VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, kCompute | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                 VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
    {
      Pass& pass = passes_[kCompose];
      dispatch_.cmd_bind_pipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pass.pipeline);
      dispatch_.cmd_bind_descriptor_sets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pass.layout, 0, 1, &pass.sets[slot_], 0, nullptr);
      dispatch_.cmd_push_constants(cmd, pass.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
      dispatch_indirect_(cmd, tiles_.buffer(), 0);
    }
    if (in_place) {
      ImageBarrier(cmd, output.image, VK_IMAGE_LAYOUT_GENERAL, output.layout, kCompute, VK_ACCESS_2_SHADER_WRITE_BIT,
                   VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
    } else {
      ImageBarrier(cmd, picture_.image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, kCompute,
                   VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
      ImageBarrier(cmd, output.image, output.layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                   VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
      VkImageBlit blit{};
      blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.srcSubresource.layerCount = 1;
      blit.srcOffsets[1] = {int32_t(width_), int32_t(height_), 1};
      blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.dstSubresource.layerCount = 1;
      blit.dstOffsets[1] = {int32_t(width_), int32_t(height_), 1};
      dispatch_.cmd_blit_image(cmd, picture_.image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, output.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
      ImageBarrier(cmd, output.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, output.layout, VK_PIPELINE_STAGE_2_BLIT_BIT,
                   VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                   VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
    }
    return Status::kSuccess;
  }

 private:
  bool HasOptimalFeatures(VkFormat format, VkFormatFeatureFlags features) const {
    VkFormatProperties properties{};
    dispatch_.get_physical_device_format_properties(vulkan_.physical_device, format, &properties);
    return (properties.optimalTilingFeatures & features) == features;
  }

  bool MakePass(Pass* pass, const uint32_t* code, size_t bytes, const std::vector<VkDescriptorType>& bindings) {
    std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
    for (uint32_t i = 0; i < bindings.size(); ++i) {
      VkDescriptorSetLayoutBinding b{};
      b.binding = i;
      b.descriptorType = bindings[i];
      b.descriptorCount = 1;
      b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
      layout_bindings.push_back(b);
    }
    VkDescriptorSetLayoutCreateInfo dsl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dsl.bindingCount = uint32_t(layout_bindings.size());
    dsl.pBindings = layout_bindings.data();
    if (dispatch_.create_descriptor_set_layout(vulkan_.device, &dsl, vulkan_.allocator, &pass->set_layout) != VK_SUCCESS) return false;
    VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(HudPush)};
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &pass->set_layout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &range;
    if (dispatch_.create_pipeline_layout(vulkan_.device, &pl, vulkan_.allocator, &pass->layout) != VK_SUCCESS) return false;
    VkShaderModuleCreateInfo smi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smi.codeSize = bytes;
    smi.pCode = code;
    VkShaderModule module = VK_NULL_HANDLE;
    if (dispatch_.create_shader_module(vulkan_.device, &smi, vulkan_.allocator, &module) != VK_SUCCESS) return false;
    VkComputePipelineCreateInfo cpi{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpi.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpi.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpi.stage.module = module;
    cpi.stage.pName = "main";
    cpi.layout = pass->layout;
    const VkResult result = dispatch_.create_compute_pipelines(vulkan_.device, VK_NULL_HANDLE, 1, &cpi, vulkan_.allocator, &pass->pipeline);
    dispatch_.destroy_shader_module(vulkan_.device, module, vulkan_.allocator);
    if (result != VK_SUCCESS) return false;
    std::vector<VkDescriptorPoolSize> sizes;
    for (VkDescriptorType type : bindings) {
      auto it = std::find_if(sizes.begin(), sizes.end(), [&](const VkDescriptorPoolSize& s) { return s.type == type; });
      if (it == sizes.end()) sizes.push_back({type, slots_});
      else it->descriptorCount += slots_;
    }
    VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpi.maxSets = slots_;
    dpi.poolSizeCount = uint32_t(sizes.size());
    dpi.pPoolSizes = sizes.data();
    if (dispatch_.create_descriptor_pool(vulkan_.device, &dpi, vulkan_.allocator, &pass->pool) != VK_SUCCESS) return false;
    std::vector<VkDescriptorSetLayout> layouts(slots_, pass->set_layout);
    pass->sets.assign(slots_, VK_NULL_HANDLE);
    VkDescriptorSetAllocateInfo dsa{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsa.descriptorPool = pass->pool;
    dsa.descriptorSetCount = slots_;
    dsa.pSetLayouts = layouts.data();
    return dispatch_.allocate_descriptor_sets(vulkan_.device, &dsa, pass->sets.data()) == VK_SUCCESS;
  }

  void BindSampled(Stage stage, uint32_t binding, VkImageView view, VkImageLayout layout) {
    VkDescriptorImageInfo info{sampler_, view, layout};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = passes_[stage].sets[slot_];
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &info;
    dispatch_.update_descriptor_sets(vulkan_.device, 1, &write, 0, nullptr);
  }

  void BindStorage(Stage stage, uint32_t binding, VkImageView view) {
    VkDescriptorImageInfo info{VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = passes_[stage].sets[slot_];
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = &info;
    dispatch_.update_descriptor_sets(vulkan_.device, 1, &write, 0, nullptr);
  }

  void BindBuffer(Stage stage, uint32_t binding, VkBuffer buffer) {
    VkDescriptorBufferInfo info{buffer, 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = passes_[stage].sets[slot_];
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &info;
    dispatch_.update_descriptor_sets(vulkan_.device, 1, &write, 0, nullptr);
  }

  void MemoryBarrier(VkCommandBuffer cmd, VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage,
                     VkAccessFlags2 dst_access) {
    VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    barrier.srcStageMask = src_stage;
    barrier.srcAccessMask = src_access;
    barrier.dstStageMask = dst_stage;
    barrier.dstAccessMask = dst_access;
    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency.memoryBarrierCount = 1;
    dependency.pMemoryBarriers = &barrier;
    dispatch_.cmd_pipeline_barrier_2(cmd, &dependency);
  }

  void ImageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, VkPipelineStageFlags2 src_stage,
                    VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access) {
    VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.srcStageMask = src_stage;
    barrier.srcAccessMask = src_access;
    barrier.dstStageMask = dst_stage;
    barrier.dstAccessMask = dst_access;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;
    dispatch_.cmd_pipeline_barrier_2(cmd, &dependency);
  }

  void Dispatch(VkCommandBuffer cmd, Stage stage, const HudPush& push, uint32_t gx, uint32_t gy) {
    Pass& pass = passes_[stage];
    dispatch_.cmd_bind_pipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pass.pipeline);
    dispatch_.cmd_bind_descriptor_sets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pass.layout, 0, 1, &pass.sets[slot_], 0, nullptr);
    dispatch_.cmd_push_constants(cmd, pass.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
    dispatch_.cmd_dispatch(cmd, std::max(1u, gx), std::max(1u, gy), 1);
  }

  void DestroyResources() {
    for (OwnedImage* image : {&keep_, &world_[0], &world_[1], &picture_}) image->Destroy(vulkan_, dispatch_);
    tiles_.Destroy(vulkan_, dispatch_);
    occupancy_.Destroy(vulkan_, dispatch_);
    for (OwnedBuffer& listed : listed_) listed.Destroy(vulkan_, dispatch_);
    has_b_ = false;
  }

  void Destroy() {
    if (vulkan_.device == VK_NULL_HANDLE || !dispatch_.destroy_pipeline) return;
    DestroyResources();
    for (Pass& pass : passes_) {
      if (pass.pool != VK_NULL_HANDLE) dispatch_.destroy_descriptor_pool(vulkan_.device, pass.pool, vulkan_.allocator);
      if (pass.pipeline != VK_NULL_HANDLE) dispatch_.destroy_pipeline(vulkan_.device, pass.pipeline, vulkan_.allocator);
      if (pass.layout != VK_NULL_HANDLE) dispatch_.destroy_pipeline_layout(vulkan_.device, pass.layout, vulkan_.allocator);
      if (pass.set_layout != VK_NULL_HANDLE) dispatch_.destroy_descriptor_set_layout(vulkan_.device, pass.set_layout, vulkan_.allocator);
      pass = Pass{};
    }
    if (sampler_ != VK_NULL_HANDLE) dispatch_.destroy_sampler(vulkan_.device, sampler_, vulkan_.allocator);
    sampler_ = VK_NULL_HANDLE;
  }

  VulkanContext vulkan_;
  Capabilities capabilities_;
  VulkanDispatch dispatch_;
  PFN_vkCmdDispatchIndirect dispatch_indirect_ = nullptr;
  uint32_t slots_ = 1;
  uint32_t slot_ = 0;
  VkSampler sampler_ = VK_NULL_HANDLE;
  std::array<Pass, kStages> passes_{};
  OwnedImage keep_, picture_;
  std::array<OwnedImage, 2> world_;
  OwnedBuffer tiles_, occupancy_;
  std::array<OwnedBuffer, 2> listed_;  // listed_[i]: the tiles where world_[i] differs from its Real
  bool has_b_ = false;                 // world_[b_index_] holds the picture of the Real b_identity_ names
  uint32_t b_index_ = 0;
  RealIdentity b_identity_;
  uint32_t width_ = 0, height_ = 0;
  VkFormat input_format_ = VK_FORMAT_UNDEFINED;
  bool ready_ = false;
};

std::unique_ptr<HudLayer> HudLayer::Create(const CreateInfo& create_info) {
  auto impl = std::make_unique<Impl>(create_info);
  if (!impl->Initialize()) return nullptr;
  return std::unique_ptr<HudLayer>(new HudLayer(std::move(impl)));
}

HudLayer::HudLayer(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
HudLayer::~HudLayer() = default;

Status HudLayer::Resize(uint32_t width, uint32_t height, VkFormat input_format, VkFormat output_format) {
  return impl_->Resize(width, height, input_format, output_format);
}

Status HudLayer::Record(VkCommandBuffer command_buffer, const Image& previous, const Image& current,
                        const Interpolator::HudPlanes& previous_hud, const Interpolator::HudPlanes& current_hud, const Image& output,
                        const Generate& generate) {
  return impl_->Record(command_buffer, previous, current, previous_hud, current_hud, output, generate);
}

}  // namespace zerofg
