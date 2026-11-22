// ReallyZero (Mode::kReallyZero), the dense flow: RC5 seed 2 from the ZeroFG laboratory, the Eco form of its `df` profile
// (testbed/src/core/df_runner.cc on the algorithm repository), written from the published algorithms (DIS, Kroeger et al. 2016; the Super SloMo
// midpoint flow) and the laboratory's own cleanup:
//
//   down    the luma pyramid of A and B (level 1 straight from the Reals, the coarser levels from the level above), R16F textures
//   up      the starting flow of a level, both directions: twice the coarser level's flow
//   patch   the DIS inverse search, 8 x 8 patches every 4 px, two Gauss-Newton steps, both directions in one dispatch: eight patches per
//           64-wide wave (8 lanes each, shuffle sums) where the device shuffles (the Adreno pays a fixed cost per wave), one 64-wide
//           subgroup per patch where it only adds, the workgroup form otherwise (Compat)
//   dense   the densification: each pixel the residual-weighted mean of the patches covering it
//   final   the middle picture from the midpoint flow (the Reals through the clamped five-tap Catmull-Rom kernel), and the doubt indicator
//   clean   the doubt density over 9 x 9 and the 5 x 5 blend where it rises, into the output
//
// The flow is solved down to half resolution. Optional starts retain last pair's flow; optional pyramid retention uses Image::sequence to identify the same Real. Phase 0.5 only.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "algorithm.h"
#include "vulkan_buffer.h"
#include "vulkan_dispatch.h"
#include "vulkan_image.h"

#include "df_clean_t.h"
#include "df_clist.h"
#include "df_cut.h"
#include "df_dense.h"
#include "df_down.h"
#include "df_final.h"
#include "df_patch_q.h"
#include "df_patch_q8.h"
#include "df_patch_sg.h"
#include "df_patch_wg.h"
#include "df_up.h"
#include "rc3_crop.h"

namespace zerofg {
namespace {

// The laboratory's Eco defaults (2026-10-28).
constexpr int32_t kPatch = 8, kStride = 4, kIterations = 2;
constexpr uint32_t kFinest = 1;                    // the flow is solved down to level 1 (half resolution)
constexpr size_t kMaxLevels = 16;                  // 16384 px halves to the 24 px floor in 10 levels
constexpr float kSigma = 1.5f;                     // the loop-error weight of the two warps
constexpr float kDisagreement = 35.0f;             // luma (0..255) the two warps may disagree by before a pixel is doubtful
constexpr float kLoop = 1.0f;                      // px the forward-backward loop may miss by
constexpr float kCleanLow = 0.30f, kCleanHigh = 0.70f;  // the doubt density ramp of the blend
// RC5 as a product. A character the camera follows stands still on a running world: the search also starts from standing still and from last pair's answer, on the finest level and three above it, when
// that costs at most this share of the level's flow. The flow is solved on the first level no wider than kFlowMaxWidth, and the loop's limit is for 640 wide and follows the picture.
constexpr int kStartLevels = 3;
constexpr float kStartShare = 0.8f;
constexpr uint32_t kFlowMaxWidth = 400;
constexpr float kLoopAt640 = 1.5f;
constexpr int kStillBar = 20;      // thousandths: where the moving samples disagree and A and B agree in place within this, the place is right
constexpr int kCutCorrelation = 300;  // thousandths: under this co-located correlation of a small pyramid level the pair is a cut and shows B whole
constexpr uint32_t kCutLevelWidth = 128;
// Standalone RZ only: direct cleanup args, grouped cut, lazy sharp samples,
// identity-store elision and texture-only fine flow. Pyramid retention is separate.
constexpr int kProductOpts = 31;

uint32_t Groups(uint32_t extent, uint32_t local) { return (extent + local - 1) / local; }

ActiveRect ResolveRect(const Image& image) {
  if (image.active_rect.width == 0 || image.active_rect.height == 0) {
    return {0, 0, image.width, image.height};
  }
  return image.active_rect;
}

struct Pass {
  VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkDescriptorPool pool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> sets;
  uint32_t stride = 0;  // the passes that bind the Reals or the output: this many sets per frame context, one copy for each
};

enum class Stage : uint32_t { kCrop, kDown, kDownB, kUp, kPatch, kPatch1, kDense, kDense1, kFinal, kClean, kCList, kCut, kCount };

// The push block of every df pass (shaders/df/df_common.glsl).
struct Push {
  int32_t a[4] = {}, b[4] = {}, c[4] = {};
  float p[4] = {};
};
static_assert(sizeof(Push) == 64);

struct Level {
  uint32_t w = 0, h = 0, nx = 1, ny = 1;
  int32_t pyr_a = 0, pyr_b = 0, flow0 = 0, flow1 = 0;
};

class DfContext final : public AlgorithmContext, public DenseFlowSource {
 public:
  explicit DfContext(const CreateInfo& create_info, uint32_t finest = kFinest, bool starts = false, bool product = false, bool retain_pyramid = false)
      : vulkan_(create_info.vulkan), capabilities_(create_info.capabilities), backend_(create_info.backend), finest_(finest), starts_(starts), product_(product), retain_pyramid_(retain_pyramid), frame_contexts_(std::max(1u, create_info.frame_context_count)) {}

  // ---- the RC6 proposal source (DenseFlowSource) ----
  Status Resize(uint32_t width, uint32_t height, VkFormat input_format) override {
    return Resize(width, height, input_format, VK_FORMAT_A2B10G10R10_UNORM_PACK32);
  }
  VkBuffer flow_buffer() const override { return flow_.buffer(); }
  uint32_t flow_offset() const override { return levels_[finest_].flow0; }
  uint32_t flow_width() const override { return levels_[finest_].w; }
  uint32_t flow_height() const override { return levels_[finest_].h; }
  Status RecordFlow(VkCommandBuffer command_buffer, const Image& previous, const Image& current) override {
    return Record(command_buffer, previous, current, nullptr);
  }
  Status RecordPicture(VkCommandBuffer command_buffer, const Image& previous, const Image& current, const Image& output) override {
    return Interpolate(command_buffer, previous, current, 0.5f, output);
  }
  void SetPictureSize(uint32_t width, uint32_t height) override {
    picture_w_ = width;
    picture_h_ = height;
  }
  Status RecordPictureFrom(VkCommandBuffer command_buffer, const Image& previous, const Image& current, const Image& picture_previous, const Image& picture_current, const Image& output) override {
    if (output.image == VK_NULL_HANDLE || output.view == VK_NULL_HANDLE || output.width != PictureW() || output.height != PictureH() || output.format != output_format_ ||
        output.layout == VK_IMAGE_LAYOUT_UNDEFINED || picture_previous.view == VK_NULL_HANDLE || picture_current.view == VK_NULL_HANDLE ||
        picture_previous.layout == VK_IMAGE_LAYOUT_UNDEFINED || picture_current.layout == VK_IMAGE_LAYOUT_UNDEFINED) {
      InvalidatePyramidCache();
      return Status::kInvalidArgument;
    }
    return Record(command_buffer, previous, current, &output, &picture_previous, &picture_current);
  }

  ~DfContext() override { DestroyWorking(); }

  bool Initialize() {
    return dispatch_.Load(vulkan_.instance, vulkan_.device, vulkan_.get_instance_proc_addr) &&
           dispatch_.cmd_pipeline_barrier_2 != nullptr && dispatch_.cmd_blit_image != nullptr &&
           dispatch_.create_sampler != nullptr && LoadIndirect();
  }

  Status Resize(uint32_t width, uint32_t height, VkFormat input_format, VkFormat output_format) override {
    InvalidatePyramidCache();
    if (width < 64 || height < 64 || width > 16384 || height > 16384) {
      return Status::kInvalidArgument;
    }
    if (!capabilities_.shader_storage_image_extended_formats_enabled ||
        capabilities_.effective_api_version < VK_API_VERSION_1_3 || !capabilities_.synchronization2_enabled) {
      return Status::kUnsupported;
    }
    const VkFormatFeatureFlags work = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                      VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    if (!HasOptimalFeatures(input_format, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ||
        !HasOptimalFeatures(VK_FORMAT_R16_SFLOAT, work) || !HasOptimalFeatures(VK_FORMAT_R16G16_SFLOAT, work) ||
        !HasOptimalFeatures(VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
      return Status::kUnsupported;
    }
    const bool direct = output_format == VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    if (!direct && !HasOptimalFeatures(output_format, VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
      return Status::kUnsupported;
    }

    DestroyWorking();
    width_ = width;
    height_ = height;
    input_format_ = input_format;
    output_format_ = output_format;
    direct_output_ = direct;
    const uint32_t needed = VK_SUBGROUP_FEATURE_ARITHMETIC_BIT;
    use_subgroup_ = backend_ != Backend::kCompat && capabilities_.subgroup_size >= 64 &&
                    (capabilities_.subgroup_min_size == 0 || capabilities_.subgroup_min_size >= 64) &&
                    (capabilities_.subgroup_supported_operations & needed) == needed &&
                    (capabilities_.subgroup_supported_stages & VK_SHADER_STAGE_COMPUTE_BIT) != 0;
    const uint32_t shuffle = VK_SUBGROUP_FEATURE_SHUFFLE_BIT;
    use_quad_ = backend_ != Backend::kCompat && capabilities_.subgroup_size >= 8 &&
                (capabilities_.subgroup_min_size == 0 || capabilities_.subgroup_min_size >= 8) &&
                (capabilities_.subgroup_supported_operations & shuffle) == shuffle &&
                (capabilities_.subgroup_supported_stages & VK_SHADER_STAGE_COMPUTE_BIT) != 0;
    PlanLevels();

    Status status = CreateWorkingResources();
    if (status == Status::kSuccess) status = CreatePasses();
    if (status != Status::kSuccess) {
      DestroyWorking();
      return status;
    }
    BindStaticDescriptors();
    initialized_ = true;
    return Status::kSuccess;
  }

  Status Interpolate(VkCommandBuffer command_buffer, const Image& previous, const Image& current, float phase,
                     const Image& output) override {
    if (output.image == VK_NULL_HANDLE || output.view == VK_NULL_HANDLE || output.width != width_ || output.height != height_ ||
        output.format != output_format_ || output.layout == VK_IMAGE_LAYOUT_UNDEFINED || output.image == previous.image ||
        output.image == current.image || !std::isfinite(phase)) {
      InvalidatePyramidCache();
      return Status::kInvalidArgument;
    }
    if (std::abs(phase - 0.5f) > 0.0001f) {
      InvalidatePyramidCache();
      return Status::kUnsupported;
    }
    return Record(command_buffer, previous, current, &output);
  }

  // The frame, or with no output (RC6) only the flow.
  Status Record(VkCommandBuffer command_buffer, const Image& previous, const Image& current, const Image* output_or_null, const Image* picture_a = nullptr, const Image* picture_b = nullptr) {
    const bool pyramid_was_valid = pyramid_valid_;
    // No failed or partially recorded call may advertise a newly prepared B.
    pyramid_valid_ = pyramid_reuse_ = false;
    if (!initialized_ || command_buffer == VK_NULL_HANDLE || previous.image == VK_NULL_HANDLE ||
        previous.view == VK_NULL_HANDLE || current.image == VK_NULL_HANDLE || current.view == VK_NULL_HANDLE ||
        previous.width == 0 || previous.height == 0 || current.width != previous.width ||
        current.height != previous.height || previous.format != input_format_ || current.format != input_format_ ||
        (previous.usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0 || (current.usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0 ||
        previous.layout == VK_IMAGE_LAYOUT_UNDEFINED || current.layout == VK_IMAGE_LAYOUT_UNDEFINED) {
      return Status::kInvalidArgument;
    }
    const ActiveRect rect = ResolveRect(previous), current_rect = ResolveRect(current);
    if (rect.width != width_ || rect.height != height_ || current_rect.x != rect.x || current_rect.y != rect.y ||
        current_rect.width != rect.width || current_rect.height != rect.height || rect.x > previous.width ||
        rect.y > previous.height || rect.width > previous.width - rect.x || rect.height > previous.height - rect.y) {
      return Status::kInvalidArgument;
    }
    // A physical image larger than the picture, or a picture that does not start at (0, 0), is cropped into a tight copy first (the
    // texture reads clamp to the image, not to the rectangle).
    const bool crop = rect.x != 0 || rect.y != 0 || previous.width != width_ || previous.height != height_;
    if (crop) {
      if (input_format_ != VK_FORMAT_A2B10G10R10_UNORM_PACK32) return Status::kUnsupported;
      const Status status = EnsureCropImages();
      if (status != Status::kSuccess) return status;
    }
    const bool write_direct = output_or_null && direct_output_ && (output_or_null->usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0;
    if (output_or_null && !write_direct && (output_or_null->usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) {
      return Status::kInvalidArgument;
    }

    pyramid_reuse_ = retain_pyramid_ && pyramid_was_valid && previous.sequence != 0 && current.sequence != 0 &&
                     previous.sequence == pyramid_identity_.sequence && previous.width == pyramid_identity_.width &&
                     previous.height == pyramid_identity_.height && previous.format == pyramid_identity_.format &&
                     rect.x == pyramid_identity_.rect.x && rect.y == pyramid_identity_.rect.y &&
                     rect.width == pyramid_identity_.rect.width && rect.height == pyramid_identity_.rect.height;
    pyramid_a_ = pyramid_reuse_ ? pyramid_b_ : 0u;
    pyramid_b_ = 1u - pyramid_a_;

    // The record before may still be running: this one takes the next copy of the sets it writes.
    slot_ = (slot_ + 1) % frame_contexts_;
    const VkImageView view_a = crop ? tight_[0].view() : previous.view;
    const VkImageView view_b = crop ? tight_[1].view() : current.view;
    const VkImageLayout layout_a = crop ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : previous.layout;
    const VkImageLayout layout_b = crop ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : current.layout;
    for (Stage stage : {Stage::kDown, Stage::kDownB}) {
      if (stage == Stage::kDownB && !retain_pyramid_) continue;
      for (uint32_t level = 0; level < uint32_t(levels_.size()); ++level) {
        const uint32_t set = Copy(stage, PyramidSet(level));
        BindSampled(stage, set, 13, view_a, layout_a);
        BindSampled(stage, set, 14, view_b, layout_b);
      }
    }
    BindSampled(Stage::kFinal, Copy(Stage::kFinal, 0), 13, picture_a ? picture_a->view : view_a, picture_a ? picture_a->layout : layout_a);
    BindSampled(Stage::kFinal, Copy(Stage::kFinal, 0), 14, picture_b ? picture_b->view : view_b, picture_b ? picture_b->layout : layout_b);
    if (output_or_null) {
      BindStorageImage(Stage::kClean, Copy(Stage::kClean, 0), 20, write_direct ? output_or_null->view : fallback_output_.view());
      BindStorageImage(Stage::kFinal, Copy(Stage::kFinal, 0), 20, write_direct ? output_or_null->view : fallback_output_.view());
    }
    if (crop) {
      BindSampled(Stage::kCrop, Copy(Stage::kCrop, 0), 0, previous.view, previous.layout);
      BindStorageImage(Stage::kCrop, Copy(Stage::kCrop, 0), 1, tight_[0].view());
      BindSampled(Stage::kCrop, Copy(Stage::kCrop, 1), 0, current.view, current.layout);
      BindStorageImage(Stage::kCrop, Copy(Stage::kCrop, 1), 1, tight_[1].view());
    }

    // What the earlier submissions of this queue wrote is visible to this record, and this record's writes do not overtake their reads.
    MemoryBarrier(command_buffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT);
    if (crop) RecordCrop(command_buffer, rect);
    RecordFrame(command_buffer, output_or_null, write_direct);
    // A can be unknown on a miss (RC7 forwards 0 unless its full source-geometry
    // check certified continuity); a known, freshly prepared B still seeds the next call.
    if (retain_pyramid_ && current.sequence != 0) {
      pyramid_identity_ = {current.sequence, current.width, current.height, current.format, current_rect};
      pyramid_valid_ = true;
    }
    return Status::kSuccess;
  }

 private:
  uint32_t PictureW() const { return picture_w_ ? picture_w_ : width_; }
  uint32_t PictureH() const { return picture_h_ ? picture_h_ : height_; }
  // RC6/RC7 consume flow_buffer() as FP32, so their source contexts must
  // neither omit that store nor skip a cleanup summary its shader expects.
  int ProductOpts() const { return product_ ? kProductOpts : 0; }
  void InvalidatePyramidCache() {
    pyramid_valid_ = pyramid_reuse_ = false;
    pyramid_a_ = 0;
    pyramid_b_ = 1;
    pyramid_identity_ = {};
  }
  static bool PyramidStage(Stage stage) {
    return stage == Stage::kDown || stage == Stage::kDownB || stage == Stage::kPatch || stage == Stage::kPatch1 ||
           stage == Stage::kDense || stage == Stage::kDense1 || stage == Stage::kCut;
  }
  uint32_t PyramidSet(uint32_t level) const {
    return level + (retain_pyramid_ ? pyramid_a_ * uint32_t(levels_.size()) : 0u);
  }
  const OwnedImage& PyramidBank(uint32_t bank, uint32_t level) const { return bank == 0 ? pyr_a_[level] : pyr_b_[level]; }
  bool LoadIndirect() {
    cmd_dispatch_indirect_ = reinterpret_cast<PFN_vkCmdDispatchIndirect>(dispatch_.get_device_proc_addr(vulkan_.device, "vkCmdDispatchIndirect"));
    return cmd_dispatch_indirect_ != nullptr;
  }

  // ---- capabilities ------------------------------------------------------------------------------------------------------------------------
  bool HasOptimalFeatures(VkFormat format, VkFormatFeatureFlags features) const {
    VkFormatProperties properties{};
    dispatch_.get_physical_device_format_properties(vulkan_.physical_device, format, &properties);
    return (properties.optimalTilingFeatures & features) == features;
  }

  // ---- geometry ----------------------------------------------------------------------------------------------------------------------------
  // The pyramid halves until the shorter side is at most 2 P + 8 (the laboratory's rule); level 0 (the full-resolution luma) is never built:
  // level 1 comes from the Reals. The flow buffers hold the solved levels (finest_ and coarser), both directions next to each other.
  void PlanLevels() {
    levels_.clear();
    Level l0;
    l0.w = width_;
    l0.h = height_;
    levels_.push_back(l0);
    while (std::min(levels_.back().w, levels_.back().h) > uint32_t(2 * kPatch + 8) && levels_.size() < kMaxLevels) {
      Level l;
      l.w = (levels_.back().w + 1) / 2;
      l.h = (levels_.back().h + 1) / 2;
      levels_.push_back(l);
    }
    have_last_ = false;
    if (product_) {
      finest_ = 1;
      while (finest_ < 3 && (width_ >> finest_) > kFlowMaxWidth) ++finest_;
      finest_ = std::min<uint32_t>(finest_, uint32_t(levels_.size()) - 1);
    }
    int32_t pyr = 0, flow = 0;
    max_patches_ = 1;
    for (size_t i = 1; i < levels_.size(); ++i) {
      Level& l = levels_[i];
      l.pyr_a = pyr;
      l.pyr_b = pyr + int32_t(l.w * l.h);
      pyr += int32_t(2 * l.w * l.h);
      l.nx = l.w >= uint32_t(kPatch) ? (l.w - kPatch + kStride - 1) / kStride + 1 : 1;
      l.ny = l.h >= uint32_t(kPatch) ? (l.h - kPatch + kStride - 1) / kStride + 1 : 1;
      if (i >= finest_) {
        l.flow0 = flow;
        l.flow1 = flow + int32_t(l.w * l.h);
        flow += int32_t(2 * l.w * l.h);
        max_patches_ = std::max(max_patches_, l.nx * l.ny);
      }
    }
    pyramid_floats_ = uint32_t(pyr);
    flow_vectors_ = uint32_t(flow);
  }

  // ---- resources ---------------------------------------------------------------------------------------------------------------------------
  Status CreateWorkingResources() {
    const VkImageUsageFlags work = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    for (size_t l = 0; l < levels_.size(); ++l) {
      // Level 0 is a 1 x 1 stand-in: level 1's pass binds it as its finer level but reads the Reals.
      const uint32_t w = l == 0 ? 1 : levels_[l].w, h = l == 0 ? 1 : levels_[l].h;
      Status status = pyr_a_[l].Create(vulkan_, dispatch_, w, h, VK_FORMAT_R16_SFLOAT, work);
      if (status != Status::kSuccess) return status;
      status = pyr_b_[l].Create(vulkan_, dispatch_, w, h, VK_FORMAT_R16_SFLOAT, work);
      if (status != Status::kSuccess) return status;
    }
    const Level& finest = levels_[finest_];
    for (OwnedImage& image : flow_image_) {
      const Status status = image.Create(vulkan_, dispatch_, finest.w, finest.h, VK_FORMAT_R16G16_SFLOAT, work);
      if (status != Status::kSuccess) return status;
    }
    Status status = fallback_output_.Create(vulkan_, dispatch_, width_, height_, VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                                            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    if (status != Status::kSuccess) return status;

    const VkBufferUsageFlags storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const VkDeviceSize pixels = VkDeviceSize(PictureW()) * PictureH();
    const std::array<std::pair<OwnedBuffer*, VkDeviceSize>, 5> buffers = {{
        {&pyramid_, VkDeviceSize(std::max(1u, pyramid_floats_)) * 4},
        {&flow_, VkDeviceSize(std::max(1u, flow_vectors_)) * 8},
        {&patch_flow_, VkDeviceSize(max_patches_) * 16},
        {&middle_, pixels * 4},
        {&dummy_, 256},  // the bindings the Eco form never reads (the laboratory's buffer forms)
    }};
    for (const auto& [buffer, size] : buffers) {
      status = buffer->Create(vulkan_, dispatch_, size, storage);
      if (status != Status::kSuccess) return status;
    }
    status = clean_.Create(vulkan_, dispatch_, 64 + VkDeviceSize(Groups(PictureW(), 16)) * Groups(PictureH(), 16) * 2 * 4, storage | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    if (status != Status::kSuccess) return status;

    VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_info.addressModeU = sampler_info.addressModeV = sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;
    if (dispatch_.create_sampler(vulkan_.device, &sampler_info, vulkan_.allocator, &sampler_) != VK_SUCCESS) {
      return Status::kVulkanError;
    }
    return Status::kSuccess;
  }

  Status EnsureCropImages() {
    if (tight_[0].image() != VK_NULL_HANDLE) return Status::kSuccess;
    for (OwnedImage& image : tight_) {
      const Status status = image.Create(vulkan_, dispatch_, width_, height_, VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                                         VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
      if (status != Status::kSuccess) return status;
    }
    return Status::kSuccess;
  }

  // ---- passes ------------------------------------------------------------------------------------------------------------------------------
  bool MakePass(Pass* pass, const uint32_t* code, size_t bytes, const std::vector<VkDescriptorType>& bindings, uint32_t push_bytes,
                uint32_t set_count, int dir = 0, int opts = 0) {
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
    if (dispatch_.create_descriptor_set_layout(vulkan_.device, &dsl, vulkan_.allocator, &pass->set_layout) != VK_SUCCESS) {
      return false;
    }
    VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, push_bytes};
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &pass->set_layout;
    pl.pushConstantRangeCount = push_bytes ? 1 : 0;
    pl.pPushConstantRanges = &range;
    if (dispatch_.create_pipeline_layout(vulkan_.device, &pl, vulkan_.allocator, &pass->layout) != VK_SUCCESS) {
      return false;
    }
    VkShaderModuleCreateInfo smi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smi.codeSize = bytes;
    smi.pCode = code;
    VkShaderModule module = VK_NULL_HANDLE;
    if (dispatch_.create_shader_module(vulkan_.device, &smi, vulkan_.allocator, &module) != VK_SUCCESS) {
      return false;
    }
    VkComputePipelineCreateInfo cpi{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpi.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpi.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpi.stage.module = module;
    cpi.stage.pName = "main";
    const int spec_values[3] = {dir, opts, ProductOpts()};
    const VkSpecializationMapEntry spec_entries[3] = {{0, 0, 4}, {1, 4, 4}, {2, 8, 4}};
    const VkSpecializationInfo spec_info{3, spec_entries, sizeof(spec_values), spec_values};
    cpi.stage.pSpecializationInfo = &spec_info;
    cpi.layout = pass->layout;
    const VkResult result =
        dispatch_.create_compute_pipelines(vulkan_.device, VK_NULL_HANDLE, 1, &cpi, vulkan_.allocator, &pass->pipeline);
    dispatch_.destroy_shader_module(vulkan_.device, module, vulkan_.allocator);
    if (result != VK_SUCCESS) {
      return false;
    }
    std::vector<VkDescriptorPoolSize> sizes;
    for (VkDescriptorType type : bindings) {
      auto it = std::find_if(sizes.begin(), sizes.end(), [&](const VkDescriptorPoolSize& s) { return s.type == type; });
      if (it == sizes.end()) sizes.push_back({type, set_count});
      else it->descriptorCount += set_count;
    }
    VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpi.maxSets = set_count;
    dpi.poolSizeCount = uint32_t(sizes.size());
    dpi.pPoolSizes = sizes.data();
    if (dispatch_.create_descriptor_pool(vulkan_.device, &dpi, vulkan_.allocator, &pass->pool) != VK_SUCCESS) {
      return false;
    }
    std::vector<VkDescriptorSetLayout> layouts(set_count, pass->set_layout);
    pass->sets.assign(set_count, VK_NULL_HANDLE);
    VkDescriptorSetAllocateInfo dsa{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsa.descriptorPool = pass->pool;
    dsa.descriptorSetCount = set_count;
    dsa.pSetLayouts = layouts.data();
    return dispatch_.allocate_descriptor_sets(vulkan_.device, &dsa, pass->sets.data()) == VK_SUCCESS;
  }

  // The df passes share one layout (shaders/df/df_common.glsl): 0..8 and 19 buffers, 9 and 10 the level's textures, 11 and 12 its storage views,
  // 13 and 14 the Reals, 15 and 16 the flow textures, 17 and 18 their storage views; the final pass and the cleanup add 20, the output image, and 25, the cleanup's tiles.
  static std::vector<VkDescriptorType> DfBindings(bool with_output, bool with_tiles = false) {
    const VkDescriptorType kSampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    const VkDescriptorType kImage = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    const VkDescriptorType kBuffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    std::vector<VkDescriptorType> b(9, kBuffer);
    b.insert(b.end(), {kSampler, kSampler, kImage, kImage, kSampler, kSampler, kSampler, kSampler, kImage, kImage, kBuffer});
    if (with_output || with_tiles) b.push_back(with_output ? kImage : kBuffer);
    if (with_tiles) b.insert(b.end(), {kBuffer, kBuffer, kBuffer, kBuffer, kBuffer});
    return b;
  }

  Status CreatePasses() {
    // Both physical A/B orientations are immutable descriptor tables. Only the
    // Down tables' source Reals are updated, in their existing per-flight copies.
    const uint32_t sets = uint32_t(levels_.size()) * (retain_pyramid_ ? 2u : 1u);
    const VkDescriptorType kSampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    const VkDescriptorType kImage = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    const uint32_t* patch_code = use_quad_ ? zerofg_df_patch_q8_spv : use_subgroup_ ? zerofg_df_patch_sg_spv : zerofg_df_patch_wg_spv;
    const size_t patch_bytes = use_quad_ ? sizeof(zerofg_df_patch_q8_spv)
                               : use_subgroup_ ? sizeof(zerofg_df_patch_sg_spv) : sizeof(zerofg_df_patch_wg_spv);
    // Known when a pipeline is built: the direction (the eight-patch search and the densification, one pipeline each), the final pass's form (the texture form, the sharp colour, the compact words, the output written here
    // and the cleanup only around doubts), the densification's weight exponent, the listed cleanup.
    const int final_opts = 1 | 2 | 1024 | 2048 | (starts_ ? 64 : 0) | (product_ ? 4096 | 8192 : 0);
    const bool ok =
        MakePass(&passes_[uint32_t(Stage::kCrop)], zerofg_rc3_crop_spv, sizeof(zerofg_rc3_crop_spv), {kSampler, kImage}, 24, 2 * frame_contexts_) &&
        MakePass(&passes_[uint32_t(Stage::kDown)], zerofg_df_down_spv, sizeof(zerofg_df_down_spv), DfBindings(false), 64, sets * frame_contexts_) &&
        (!retain_pyramid_ || MakePass(&passes_[uint32_t(Stage::kDownB)], zerofg_df_down_spv, sizeof(zerofg_df_down_spv), DfBindings(false), 64, sets * frame_contexts_, 0, 2)) &&
        MakePass(&passes_[uint32_t(Stage::kUp)], zerofg_df_up_spv, sizeof(zerofg_df_up_spv), DfBindings(false), 64, 1) &&
        MakePass(&passes_[uint32_t(Stage::kPatch)], patch_code, patch_bytes, DfBindings(false), 64, sets, 0) &&
        (!use_quad_ || MakePass(&passes_[uint32_t(Stage::kPatch1)], patch_code, patch_bytes, DfBindings(false), 64, sets, 1)) &&
        MakePass(&passes_[uint32_t(Stage::kDense)], zerofg_df_dense_spv, sizeof(zerofg_df_dense_spv), DfBindings(false), 64, sets, 0, 1) &&
        MakePass(&passes_[uint32_t(Stage::kDense1)], zerofg_df_dense_spv, sizeof(zerofg_df_dense_spv), DfBindings(false), 64, sets, 1, 1) &&
        MakePass(&passes_[uint32_t(Stage::kFinal)], zerofg_df_final_spv, sizeof(zerofg_df_final_spv), DfBindings(true, true), 64, frame_contexts_, 0, final_opts) &&
        MakePass(&passes_[uint32_t(Stage::kClean)], zerofg_df_clean_t_spv, sizeof(zerofg_df_clean_t_spv), DfBindings(true, true), 64, frame_contexts_, 0, 1) &&
        MakePass(&passes_[uint32_t(Stage::kCList)], zerofg_df_clist_spv, sizeof(zerofg_df_clist_spv), DfBindings(false, true), 64, 1) &&
        MakePass(&passes_[uint32_t(Stage::kCut)], zerofg_df_cut_spv, sizeof(zerofg_df_cut_spv), DfBindings(false, true), 64, sets);
    P(Stage::kCrop).stride = 2;
    P(Stage::kDown).stride = sets;
    if (retain_pyramid_) P(Stage::kDownB).stride = sets;
    P(Stage::kFinal).stride = P(Stage::kClean).stride = 1;
    return ok ? Status::kSuccess : Status::kVulkanError;
  }

  // ---- descriptors -------------------------------------------------------------------------------------------------------------------------
  Pass& P(Stage stage) { return passes_[uint32_t(stage)]; }
  uint32_t Copy(Stage stage, uint32_t set) { return set + P(stage).stride * slot_; }

  void BindBuffer(Stage stage, uint32_t set, uint32_t binding, const OwnedBuffer& buffer) {
    VkDescriptorBufferInfo info{buffer.buffer(), 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = P(stage).sets[set];
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &info;
    dispatch_.update_descriptor_sets(vulkan_.device, 1, &write, 0, nullptr);
  }

  void BindSampled(Stage stage, uint32_t set, uint32_t binding, VkImageView view, VkImageLayout layout) {
    VkDescriptorImageInfo info{sampler_, view, layout};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = P(stage).sets[set];
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &info;
    dispatch_.update_descriptor_sets(vulkan_.device, 1, &write, 0, nullptr);
  }

  void BindStorageImage(Stage stage, uint32_t set, uint32_t binding, VkImageView view) {
    VkDescriptorImageInfo info{VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = P(stage).sets[set];
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = &info;
    dispatch_.update_descriptor_sets(vulkan_.device, 1, &write, 0, nullptr);
  }

  // Everything but the Reals and the output, for every set of every df pass: set l binds level l's textures (the pyramid pass, its finer
  // level's) and level l's storage views. The internal images stay in GENERAL (written and sampled).
  void BindStaticDescriptors() {
    const VkImageLayout general = VK_IMAGE_LAYOUT_GENERAL;
    for (Stage stage : {Stage::kDown, Stage::kDownB, Stage::kUp, Stage::kPatch, Stage::kPatch1, Stage::kDense, Stage::kDense1, Stage::kFinal, Stage::kClean, Stage::kCList, Stage::kCut}) {
      const uint32_t per_copy = P(stage).stride ? P(stage).stride : uint32_t(P(stage).sets.size());
      for (uint32_t set = 0; set < uint32_t(P(stage).sets.size()); ++set) {
        for (uint32_t binding : {0u, 4u, 5u, 6u, 7u, 8u}) BindBuffer(stage, set, binding, dummy_);
        BindBuffer(stage, set, 1, pyramid_);
        BindBuffer(stage, set, 2, flow_);
        BindBuffer(stage, set, 3, patch_flow_);
        BindBuffer(stage, set, 19, middle_);
        const uint32_t entry = set % per_copy;
        const bool oriented = retain_pyramid_ && PyramidStage(stage);
        const uint32_t one = oriented ? entry % uint32_t(levels_.size()) : entry;
        const uint32_t a_bank = oriented ? entry / uint32_t(levels_.size()) : 0u;
        const uint32_t level = std::max(one, 1u);
        const uint32_t in = stage == Stage::kDown || stage == Stage::kDownB ? one - (one > 0 ? 1 : 0) : level;
        BindSampled(stage, set, 9, PyramidBank(a_bank, in).view(), general);
        BindSampled(stage, set, 10, PyramidBank(1u - a_bank, in).view(), general);
        BindStorageImage(stage, set, 11, PyramidBank(a_bank, level).view());
        BindStorageImage(stage, set, 12, PyramidBank(1u - a_bank, level).view());
        BindSampled(stage, set, 15, flow_image_[0].view(), general);
        BindSampled(stage, set, 16, flow_image_[1].view(), general);
        BindStorageImage(stage, set, 17, flow_image_[0].view());
        BindStorageImage(stage, set, 18, flow_image_[1].view());
        // The Reals' bindings get valid placeholders until the first Interpolate writes them (passes that never read them keep these).
        BindSampled(stage, set, 13, PyramidBank(a_bank, level).view(), general);
        BindSampled(stage, set, 14, PyramidBank(1u - a_bank, level).view(), general);
      }
    }
    for (Stage stage : {Stage::kFinal, Stage::kClean, Stage::kCList})
      for (uint32_t set = 0; set < uint32_t(P(stage).sets.size()); ++set) BindBuffer(stage, set, 25, clean_);
    for (uint32_t set = 0; set < uint32_t(P(Stage::kCut).sets.size()); ++set) BindBuffer(Stage::kCut, set, 25, clean_);
    for (Stage stage : {Stage::kFinal, Stage::kClean})
      for (uint32_t set = 0; set < uint32_t(P(stage).sets.size()); ++set) BindStorageImage(stage, set, 20, fallback_output_.view());
  }

  // ---- recording ---------------------------------------------------------------------------------------------------------------------------
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

  // Between two compute passes: the broad SHADER_WRITE as the source (the storage-only bit leaves some drivers' writes unordered; the
  // laboratory found it twice on the AMD Windows driver, the second time in this very generator's patch search).
  void ComputeBarrier(VkCommandBuffer cmd) {
    MemoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);
  }

  void ImageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout,
                    VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage,
                    VkAccessFlags2 dst_access) {
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

  void Dispatch(VkCommandBuffer cmd, Stage stage, uint32_t set, const void* push, uint32_t push_bytes, uint32_t gx, uint32_t gy,
                uint32_t gz = 1) {
    Pass& pass = P(stage);
    dispatch_.cmd_bind_pipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pass.pipeline);
    dispatch_.cmd_bind_descriptor_sets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pass.layout, 0, 1, &pass.sets[set + pass.stride * slot_], 0, nullptr);
    if (push_bytes) dispatch_.cmd_push_constants(cmd, pass.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, push_bytes, push);
    dispatch_.cmd_dispatch(cmd, std::max(1u, gx), std::max(1u, gy), gz);
  }

  void RecordCrop(VkCommandBuffer cmd, const ActiveRect& rect) {
    const VkPipelineStageFlags2 kCompute = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    struct CropPush {
      int32_t origin[2], extent[2], picture[2];
    } push{{int32_t(rect.x), int32_t(rect.y)}, {int32_t(width_), int32_t(height_)}, {int32_t(width_), int32_t(height_)}};
    static_assert(sizeof(CropPush) == 24);
    for (uint32_t s = 0; s < 2; ++s) {
      ImageBarrier(cmd, tight_[s].image(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
                   kCompute, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
      Dispatch(cmd, Stage::kCrop, s, &push, sizeof(push), Groups(width_, 8), Groups(height_, 8));
      ImageBarrier(cmd, tight_[s].image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kCompute,
                   VK_ACCESS_2_SHADER_WRITE_BIT, kCompute, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    }
  }

  void RecordFrame(VkCommandBuffer cmd, const Image* output_or_null, bool write_direct) {
    const VkPipelineStageFlags2 kCompute = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    // Normally every internal image is rewritten. On a cache hit, A is the exact
    // former B and stays GENERAL; only the other pyramid bank is discarded.
    std::vector<VkImage> internal;
    for (size_t l = 0; l < levels_.size(); ++l) {
      if (!pyramid_reuse_ || pyramid_a_ != 0) internal.push_back(pyr_a_[l].image());
      if (!pyramid_reuse_ || pyramid_a_ != 1) internal.push_back(pyr_b_[l].image());
    }
    internal.push_back(flow_image_[0].image());
    internal.push_back(flow_image_[1].image());
    for (VkImage image : internal) {
      ImageBarrier(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0, kCompute,
                   VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    }

    // The pyramid: level 1 from the Reals (mode 2), each coarser level from the one above (mode 1, four bilinear fetches).
    for (size_t l = 1; l < levels_.size(); ++l) {
      const Level& c = levels_[l];
      const Level& f = levels_[l - 1];
      Push q;
      q.a[0] = int32_t(c.w); q.a[1] = int32_t(c.h); q.a[2] = c.pyr_a; q.a[3] = c.pyr_b;
      q.b[0] = int32_t(f.w); q.b[1] = int32_t(f.h); q.b[2] = f.pyr_a; q.b[3] = f.pyr_b;
      q.c[0] = l == 1 ? 2 : 1;
      Dispatch(cmd, pyramid_reuse_ ? Stage::kDownB : Stage::kDown, PyramidSet(uint32_t(l)), &q, sizeof(q), Groups(c.w, 8), Groups(c.h, 8));
      ComputeBarrier(cmd);
    }
    // Coarse to fine: the starting flow, the patch search and the densification, both directions at once.
    const int top = int(levels_.size()) - 1;
    for (int l = top; l >= int(finest_); --l) {
      const Level& L = levels_[size_t(l)];
      Push up;
      up.a[0] = int32_t(L.w); up.a[1] = int32_t(L.h); up.a[2] = L.flow0; up.a[3] = L.flow1;
      if (l < top) {
        const Level& C = levels_[size_t(l + 1)];
        up.b[0] = int32_t(C.w); up.b[1] = int32_t(C.h); up.b[2] = C.flow0; up.b[3] = C.flow1;
      }
      up.c[0] = l == top ? 1 : 0;
      Dispatch(cmd, Stage::kUp, 0, &up, sizeof(up), Groups(L.w, 8), Groups(L.h, 8));
      ComputeBarrier(cmd);
      Push patch;
      patch.a[0] = int32_t(L.w); patch.a[1] = int32_t(L.h);
      patch.b[0] = int32_t(L.nx); patch.b[1] = int32_t(L.ny); patch.b[2] = L.flow0; patch.b[3] = kPatch;
      patch.c[0] = kStride; patch.c[1] = kIterations; patch.c[2] = 1;
      if (starts_ && l <= int(finest_) + kStartLevels) {
        patch.p[0] = 1.0f;
        patch.p[1] = have_last_ ? 1.0f : 0.0f;
        patch.p[2] = float(L.w) / float(levels_[finest_].w);
        patch.p[3] = kStartShare;
      }
      if (use_quad_) {
        Dispatch(cmd, Stage::kPatch, PyramidSet(uint32_t(l)), &patch, sizeof(patch), (L.nx * L.ny + 7) / 8, 1);
        Dispatch(cmd, Stage::kPatch1, PyramidSet(uint32_t(l)), &patch, sizeof(patch), (L.nx * L.ny + 7) / 8, 1);
      } else {
        Dispatch(cmd, Stage::kPatch, PyramidSet(uint32_t(l)), &patch, sizeof(patch), L.nx, L.ny, 2);
      }
      ComputeBarrier(cmd);
      Push dense;
      dense.a[0] = int32_t(L.w); dense.a[1] = int32_t(L.h);
      dense.b[0] = int32_t(L.nx); dense.b[1] = int32_t(L.ny); dense.b[2] = L.flow0; dense.b[3] = kPatch;
      dense.c[0] = kStride; dense.c[1] = l == int(finest_) ? 1 : 0; dense.c[2] = 1;
      Dispatch(cmd, Stage::kDense, PyramidSet(uint32_t(l)), &dense, sizeof(dense), Groups(L.w, 8), Groups(L.h, 8));
      Dispatch(cmd, Stage::kDense1, PyramidSet(uint32_t(l)), &dense, sizeof(dense), Groups(L.w, 8), Groups(L.h, 8));
      ComputeBarrier(cmd);
    }

    have_last_ = true;
    if (!output_or_null) return;  // RC6: only the flow
    const Image& output = *output_or_null;
    // The output (or the internal image that is blitted to it): the final pass writes it, the cleanup goes back over the tiles with a doubt within reach.
    VkImage target = write_direct ? output.image : fallback_output_.image();
    if (write_direct) {
      ImageBarrier(cmd, target, output.layout, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                   VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, kCompute, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    } else {
      ImageBarrier(cmd, target, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0, kCompute,
                   VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
    dispatch_.cmd_fill_buffer(cmd, clean_.buffer(), 0, 64 + VkDeviceSize(Groups(PictureW(), 16)) * Groups(PictureH(), 16) * 4, 0);
    MemoryBarrier(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, kCompute, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);
    if (product_) {
      uint32_t level = uint32_t(levels_.size()) - 1;
      for (uint32_t k = 1; k < uint32_t(levels_.size()); ++k) {
        if (levels_[k].w <= kCutLevelWidth) {
          level = k;
          break;
        }
      }
      Push cut;
      cut.a[0] = int32_t(levels_[level].w); cut.a[1] = int32_t(levels_[level].h);
      cut.b[0] = kCutCorrelation;
      Dispatch(cmd, Stage::kCut, PyramidSet(level), &cut, sizeof(cut), Groups(levels_[level].w, 8), Groups(levels_[level].h, 8));
      ComputeBarrier(cmd);
      cut.a[2] = 1;
      Dispatch(cmd, Stage::kCut, PyramidSet(level), &cut, sizeof(cut), 1, 1);
      ComputeBarrier(cmd);
    }
    // The middle picture (the texture form, the sharp colour, the compact words for the cleanup).
    const Level& F = levels_[finest_];
    Push fin;
    // A larger picture than the flow's Reals: the same flow, its vectors that much longer, and the loop's limit with them.
    const int picture_scale = int(PictureW() / width_);
    fin.a[0] = int32_t(PictureW()); fin.a[1] = int32_t(PictureH()); fin.a[2] = 1; fin.a[3] = (1 << finest_) * picture_scale;
    fin.b[0] = int32_t(F.w); fin.b[1] = int32_t(F.h); fin.b[2] = F.flow0; fin.b[3] = F.flow1;
    fin.c[1] = kStillBar;
    fin.c[3] = 1 | 2;
    fin.p[0] = kSigma; fin.p[1] = kDisagreement; fin.p[2] = product_ ? kLoopAt640 * float(width_) / 640.0f : kLoop * float(picture_scale); fin.p[3] = 1.0f;
    Dispatch(cmd, Stage::kFinal, 0, &fin, sizeof(fin), Groups(PictureW(), 8), Groups(PictureH(), 8));
    ComputeBarrier(cmd);

    Push tiles;
    tiles.a[0] = int32_t(PictureW()); tiles.a[1] = int32_t(PictureH());
    Dispatch(cmd, Stage::kCList, 0, &tiles, sizeof(tiles), Groups(Groups(PictureW(), 16), 8), Groups(Groups(PictureH(), 16), 8));
    if ((ProductOpts() & 1) == 0) {
      ComputeBarrier(cmd);
      tiles.a[2] = 1;
      Dispatch(cmd, Stage::kCList, 0, &tiles, sizeof(tiles), 1, 1);
    }
    MemoryBarrier(cmd, kCompute, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | kCompute,
                  VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);
    Push clean;
    clean.a[0] = int32_t(PictureW()); clean.a[1] = int32_t(PictureH()); clean.a[2] = 1;
    clean.p[0] = kCleanLow; clean.p[1] = kCleanHigh;
    {
      Pass& pass = P(Stage::kClean);
      dispatch_.cmd_bind_pipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pass.pipeline);
      dispatch_.cmd_bind_descriptor_sets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pass.layout, 0, 1, &pass.sets[pass.stride * slot_], 0, nullptr);
      dispatch_.cmd_push_constants(cmd, pass.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(clean), &clean);
      cmd_dispatch_indirect_(cmd, clean_.buffer(), 0);
    }

    if (write_direct) {
      ImageBarrier(cmd, output.image, VK_IMAGE_LAYOUT_GENERAL, output.layout, kCompute, VK_ACCESS_2_SHADER_WRITE_BIT,
                   VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
    } else {
      ImageBarrier(cmd, fallback_output_.image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, kCompute,
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
      dispatch_.cmd_blit_image(cmd, fallback_output_.image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, output.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
      ImageBarrier(cmd, output.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, output.layout, VK_PIPELINE_STAGE_2_BLIT_BIT,
                   VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                   VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
    }
  }

  // ---- teardown ----------------------------------------------------------------------------------------------------------------------------
  void DestroyPass(Pass* pass) {
    if (pass->pool != VK_NULL_HANDLE) dispatch_.destroy_descriptor_pool(vulkan_.device, pass->pool, vulkan_.allocator);
    if (pass->pipeline != VK_NULL_HANDLE) dispatch_.destroy_pipeline(vulkan_.device, pass->pipeline, vulkan_.allocator);
    if (pass->layout != VK_NULL_HANDLE) dispatch_.destroy_pipeline_layout(vulkan_.device, pass->layout, vulkan_.allocator);
    if (pass->set_layout != VK_NULL_HANDLE) {
      dispatch_.destroy_descriptor_set_layout(vulkan_.device, pass->set_layout, vulkan_.allocator);
    }
    *pass = {};
  }

  void DestroyWorking() {
    initialized_ = false;
    InvalidatePyramidCache();
    if (vulkan_.device == VK_NULL_HANDLE || dispatch_.get_device_proc_addr == nullptr) return;
    for (Pass& pass : passes_) DestroyPass(&pass);
    for (OwnedImage& image : pyr_a_) image.Destroy(vulkan_, dispatch_);
    for (OwnedImage& image : pyr_b_) image.Destroy(vulkan_, dispatch_);
    for (OwnedImage* image : {&flow_image_[0], &flow_image_[1], &fallback_output_, &tight_[0], &tight_[1]}) {
      image->Destroy(vulkan_, dispatch_);
    }
    for (OwnedBuffer* buffer : {&pyramid_, &flow_, &patch_flow_, &middle_, &dummy_, &clean_}) buffer->Destroy(vulkan_, dispatch_);
    if (sampler_ != VK_NULL_HANDLE) dispatch_.destroy_sampler(vulkan_.device, sampler_, vulkan_.allocator);
    sampler_ = VK_NULL_HANDLE;
  }

  VulkanContext vulkan_;
  VulkanDispatch dispatch_;
  Capabilities capabilities_;
  Backend backend_;
  uint32_t finest_ = kFinest;  // the finest solved level (RC6 proposals: 2)
  bool starts_ = false;        // the search's other starts and the agreement test: RC5 and RC7
  uint32_t picture_w_ = 0, picture_h_ = 0;  // the picture's size when it is not the flow's Reals' size (RC7)
  bool product_ = false;       // RC5 itself: the flow's grid and the loop's limit follow the resolution
  bool have_last_ = false;     // the flow images hold last pair's answer
  bool retain_pyramid_ = false;  // enabled explicitly by standalone RZ and the RC7 source owner
  struct PyramidIdentity {
    uint64_t sequence = 0;
    uint32_t width = 0, height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    ActiveRect rect{};
  } pyramid_identity_;
  bool pyramid_valid_ = false, pyramid_reuse_ = false;
  uint32_t pyramid_a_ = 0, pyramid_b_ = 1;

  bool initialized_ = false;
  bool use_subgroup_ = false;
  bool use_quad_ = false;
  bool direct_output_ = false;
  uint32_t width_ = 0, height_ = 0;
  VkFormat input_format_ = VK_FORMAT_UNDEFINED;
  VkFormat output_format_ = VK_FORMAT_UNDEFINED;
  std::vector<Level> levels_;
  uint32_t pyramid_floats_ = 0, flow_vectors_ = 0, max_patches_ = 1;

  std::array<Pass, size_t(Stage::kCount)> passes_{};
  std::array<OwnedImage, kMaxLevels> pyr_a_, pyr_b_;  // the luma pyramid per level (level 0: a 1 x 1 stand-in)
  OwnedImage flow_image_[2];               // the finest solved flow per direction (RG16F)
  OwnedImage fallback_output_;             // the output when it cannot be written directly (then blitted)
  OwnedImage tight_[2];                    // cropped copies of A and B (allocated on first use)
  OwnedBuffer pyramid_, flow_, patch_flow_, middle_, dummy_, clean_;
  PFN_vkCmdDispatchIndirect cmd_dispatch_indirect_ = nullptr;
  VkSampler sampler_ = VK_NULL_HANDLE;
  uint32_t frame_contexts_ = 1, slot_ = 0;
};

}  // namespace

std::unique_ptr<AlgorithmContext> CreateDfContext(const CreateInfo& create_info, Status* status) {
  auto context = std::make_unique<DfContext>(create_info, kFinest, /*starts=*/true, /*product=*/true, /*retain_pyramid=*/true);
  if (!context->Initialize()) {
    if (status) *status = Status::kUnsupported;
    return nullptr;
  }
  if (status) *status = Status::kSuccess;
  return context;
}

std::unique_ptr<DenseFlowSource> CreateDenseFlowSource(const CreateInfo& create_info, uint32_t finest, Status* status, bool starts, bool retain_pyramid) {
  auto context = std::make_unique<DfContext>(create_info, finest, starts, false, retain_pyramid);
  if (!context->Initialize()) {
    if (status) *status = Status::kUnsupported;
    return nullptr;
  }
  if (status) *status = Status::kSuccess;
  return context;
}

}  // namespace zerofg
