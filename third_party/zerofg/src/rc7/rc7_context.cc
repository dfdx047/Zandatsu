// Zero (Mode::kZero, RC7), the lean layered form of the laboratory's `rc7m` (testbed/src/core/rc7_gpu.cc on the algorithm repository): the pair is explained as surfaces, a front, a screen and what only one picture
// has, on top of the RC5 picture. Everything is recorded into the host's command buffer; nothing stops and nothing is read back. Which surfaces there are changes slowly: the search runs in the background, a few steps
// per pair along with the layers' passes, on the GPU. How each one moves changes every pair: this pair's flow brings them up to its motion. Wide passes with short loops. Phase 0.5 only.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

#include "algorithm.h"
#include "vulkan_buffer.h"
#include "vulkan_dispatch.h"
#include "vulkan_image.h"

#include "rc3_crop.h"
#include "rc7_alpha.h"
#include "rc7_block.h"
#include "rc7_cons.h"
#include "rc7_cost.h"
#include "rc7_extend.h"
#include "rc7_fine.h"
#include "rc7_flist.h"
#include "rc7_front.h"
#include "rc7_fsmooth.h"
#include "rc7_unfold.h"
#include "rc7_hole.h"
#include "rc7_ghost.h"
#include "rc7_guide.h"
#include "rc7_half.h"
#include "rc7_quarter.h"
#include "rc7_hud.h"
#include "rc7_hudrule.h"
#include "rc7_hudgs.h"
#include "rc7_jump.h"
#include "rc7_mix.h"
#include "rc7_morph.h"
#include "rc7_out.h"
#include "rc7_raw.h"
#include "rc7_render.h"
#include "rc7_scell.h"
#include "rc7_secho.h"
#include "rc7_sdone.h"
#include "rc7_seed.h"
#include "rc7_sgate.h"
#include "rc7_shyp.h"
#include "rc7_sinl.h"
#include "rc7_slist.h"
#include "rc7_soft.h"
#include "rc7_sorder.h"
#include "rc7_sfit.h"
#include "rc7_srefit.h"
#include "rc7_sstart.h"
#include "rc7_stand.h"
#include "rc7_stats.h"
#include "rc7_tblock.h"
#include "rc7_tfit.h"
#include "rc7_tguide.h"
#include "rc7_tile.h"
#include "rc7_trust.h"

namespace zerofg {
namespace {

// The laboratory's numbers for rc7m (2026-11-01).
constexpr int kFineRounds = 1, kFrontRoundsBend = 1, kFrontRoundsSample = 2, kPeels = 3, kPatch = 3, kFloorGate = 30;
constexpr uint32_t kFlowLevel = 1;   // the dense flow runs on half-size Reals, solved down to a quarter of the picture. Its picture is full size: what falls back to it must not turn soft from one pair to the next, and a flow from full-size Reals brings the shards on repeating bars back through the surfaces
constexpr uint32_t kFieldSide = 64;  // the long side of the cell field
constexpr int kPlans = 18;
constexpr VkDeviceSize kRefitOffset = 15376, kFollowOffset = 16528, kCutOffset = 16840;  // in the search's buffer: r_H, t_H, s_cut
// When the base takes over, per cent of what a wrong match costs: soon in the background, where surfaces do not fit many depths and the two samples double up; late near the front, whose cutout is better from the layers.
constexpr int kBaseBar = 80, kBaseBarFront = 80;
#ifndef ZFG_RC7_BASE_FULL
#define ZFG_RC7_BASE_FULL 1
#endif
constexpr bool kBaseFullSize = ZFG_RC7_BASE_FULL != 0;  // the base's picture at the Reals' size, its flow still from the half-size ones: sharper where it is used, about a millisecond more on the Adreno at 3x
constexpr int kCutCorrelation = 300;  // per mille: under this co-located correlation of the softened pictures the pair is a cut and shows B whole
constexpr int kHudMemory = 0;  // percent of each pair in the screen's memory: what was the screen lately and still repeats in place stays the screen
constexpr int kHoleReach = 96;  // pixels: a point with matte around it in all four directions within this goes back to the matte
constexpr int kUnfoldBar = 70;  // hundredths of a pixel per pixel: a point where most around stretches more than this leaves the front's matte
constexpr uint32_t kHalfFrom = 800;  // from this wide the decisions read a quarter-size copy of the Reals: their far-apart samples stay in the cache, and only the render walks the whole picture
constexpr int P_DIST1 = 3, P_MASK = 4, P_TMP = 5, P_FCOST0 = 8, P_HUD = 11;
// Exact reuse and output delivery, qualified against the dense-disagreement reference.
constexpr int kFuseOutput = 8, kReusePrepared = 16, kReusePyramid = 256, kReuseStats = 512;
constexpr int kReuseOpts = kFuseOutput | kReusePrepared | kReusePyramid | kReuseStats;

uint32_t Groups(uint32_t extent, uint32_t local) { return (extent + local - 1) / local; }

ActiveRect ResolveRect(const Image& image) {
  if (image.active_rect.width == 0 || image.active_rect.height == 0) return {0, 0, image.width, image.height};
  return image.active_rect;
}

// The cell field's geometry, as RC3 runs a picture (the laboratory's tile=1).
VkExtent2D FieldExtent(uint32_t width, uint32_t height) {
  const bool landscape = width >= height;
  const uint32_t major = std::max(width, height), minor = std::min(width, height);
  uint32_t fitted = major ? uint32_t((uint64_t(minor) * kFieldSide + uint64_t(major) / 2) / major) : 0;
  fitted = std::clamp(((fitted + 2) / 4) * 4, 8u, kFieldSide);
  return landscape ? VkExtent2D{kFieldSide, fitted} : VkExtent2D{fitted, kFieldSide};
}

bool CellsTile(uint32_t width, uint32_t height) {
  const VkExtent2D l0 = FieldExtent(width, height);
  if (width % l0.width != 0 || height % l0.height != 0) return false;
  const uint32_t cell_w = width / l0.width, cell_h = height / l0.height, stride = (cell_w + 3) / 4;
  return 1 + stride * 3 < cell_w && 1 + stride * 3 < cell_h;
}

void TiledExtent(uint32_t width, uint32_t height, uint32_t* tiled_width, uint32_t* tiled_height) {
  *tiled_width = width;
  *tiled_height = height;
  uint64_t best_area = std::numeric_limits<uint64_t>::max();
  for (uint32_t h = height; h < height + 128 && h <= 16384; ++h)
    for (uint32_t w = width; w < width + 128 && w <= 16384; ++w) {
      const uint64_t area = uint64_t(w) * h;
      if (area >= best_area) break;
      if (CellsTile(w, h)) {
        best_area = area;
        *tiled_width = w;
        *tiled_height = h;
        break;
      }
    }
}

// As shaders/rc7/rc7_common.glsl declares it.
struct Params {
  int32_t dim[4], cfg[4], cfg2[4];
  float cell[4], gate[4], th0[4], th1[4], th2[4], th3[4], judge0[4], judge1[4], th4[4];
  int32_t cfg3[4];
  int32_t order[4], order2[4];
  float H[18][4], Hi[18][4], Mid[18][4];
};

struct Push {
  int32_t a[4] = {}, b[4] = {};
  float p[4] = {};
};

// A pass that reads the Reals has one pipeline per picture, and the costs one per picture and kind, the side known when it is built: a branch at every fetch costs more than the arithmetic of the sample on the Adreno.
// The render has the full one and a short one per surface, for the blocks with that surface and nothing else.
enum Pipe : uint32_t {
  kCrop, kHalf, kHalf1, kQuarter, kQuarter1, kFList, kSoft, kSoft1, kStats, kStats1, kHud, kHudRule, kCost, kCost1, kCost2, kCost3, kSeed, kJump, kJump1, kRaw, kBlockPass, kBlockPass1, kStand, kStand1, kGhost, kMorph, kFront, kFront1, kFSmooth, kFSmooth1, kUnfold, kUnfold1, kHole, kCons, kExtend, kAlpha,
  kRender, kRenderOne, kRenderOne5 = kRenderOne + 5, kTrust, kMix, kFine, kFine1, kGuide, kGuide1, kTile,
  kSCell, kSStart, kSList, kSHyp, kSFit, kSRefit, kSInl, kSEcho, kSDone, kSOrder, kSGate, kTBlock, kTFit, kTGuide, kOut, kStatsSharp, kHudGs, kPipeCount
};

class Rc7Context final : public AlgorithmContext {
 public:
  explicit Rc7Context(const CreateInfo& create_info) : create_info_(create_info), vulkan_(create_info.vulkan), capabilities_(create_info.capabilities) {}
  ~Rc7Context() override { DestroyWorking(); }

  bool Initialize() {
    if (!dispatch_.Load(vulkan_.instance, vulkan_.device, vulkan_.get_instance_proc_addr) || !dispatch_.cmd_pipeline_barrier_2 || !dispatch_.cmd_blit_image || !dispatch_.create_sampler) return false;
    cmd_update_buffer_ = reinterpret_cast<PFN_vkCmdUpdateBuffer>(dispatch_.get_device_proc_addr(vulkan_.device, "vkCmdUpdateBuffer"));
    cmd_dispatch_indirect_ = reinterpret_cast<PFN_vkCmdDispatchIndirect>(dispatch_.get_device_proc_addr(vulkan_.device, "vkCmdDispatchIndirect"));
    cmd_clear_color_image_ = reinterpret_cast<PFN_vkCmdClearColorImage>(dispatch_.get_device_proc_addr(vulkan_.device, "vkCmdClearColorImage"));
    return cmd_update_buffer_ && cmd_dispatch_indirect_ && cmd_clear_color_image_;
  }

  Status Resize(uint32_t width, uint32_t height, VkFormat input_format, VkFormat output_format) override {
    prep_valid_ = prep_reuse_ = false;
    if (width < 64 || height < 64 || width > 16384 || height > 16384) return Status::kInvalidArgument;
    if (!capabilities_.shader_storage_image_extended_formats_enabled || capabilities_.effective_api_version < VK_API_VERSION_1_3 || !capabilities_.synchronization2_enabled) return Status::kUnsupported;
    const VkFormatFeatureFlags work = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    if (!HasOptimalFeatures(input_format, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) || !HasOptimalFeatures(VK_FORMAT_R8G8B8A8_UNORM, work) ||
        !HasOptimalFeatures(VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT))
      return Status::kUnsupported;
    const bool direct = output_format == VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    if (!direct && !HasOptimalFeatures(output_format, VK_FORMAT_FEATURE_BLIT_DST_BIT)) return Status::kUnsupported;

    DestroyWorking();
    width_ = width;
    height_ = height;
    input_format_ = input_format;
    output_format_ = output_format;
    direct_output_ = direct;
    step_ = width >= 1200 ? 8 : width >= 800 ? 6 : 4;  // about the lattice of a 1x picture at any size: analysing finer because the game is drawn bigger costs and buys nothing
    lw_ = Groups(width, step_);
    lh_ = Groups(height, step_);
    block_ = std::max(8u, 32u / step_);
    gw_ = Groups(lw_, block_);
    gh_ = Groups(lh_, block_);
    uint32_t run_w = width, run_h = height;
    TiledExtent(width, height, &run_w, &run_h);
    const VkExtent2D field = FieldExtent(run_w, run_h);
    fw_ = field.width;
    fh_ = field.height;
    cell_w_ = float(double(run_w) / fw_);
    cell_h_ = float(double(run_h) / fh_);

    Status status = Status::kSuccess;
    dense_ = CreateDenseFlowSource(create_info_, kFlowLevel, &status, true, (kReuseOpts & kReusePyramid) != 0);
    if (!dense_) return status;
    if (kBaseFullSize) dense_->SetPictureSize(width, height);
    status = dense_->Resize(width / 2, height / 2, VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    if (status == Status::kSuccess) status = CreateResources();
    if (status == Status::kSuccess) status = CreatePipelines();
    if (status != Status::kSuccess) {
      DestroyWorking();
      return status;
    }
    BindStatic();
    initialized_ = true;
    return Status::kSuccess;
  }

  void SetScreenTruth(const Image* screen) override { screen_truth_ = screen ? *screen : Image{}; }

  Status Interpolate(VkCommandBuffer cmd, const Image& previous, const Image& current, float phase, const Image& output) override {
    // A failed record must not advertise a newly prepared Real to the next call.
    const bool prep_was_valid = prep_valid_;
    prep_valid_ = prep_reuse_ = false;
    if (!initialized_ || cmd == VK_NULL_HANDLE || output.image == VK_NULL_HANDLE || output.view == VK_NULL_HANDLE || output.width != width_ || output.height != height_ || output.format != output_format_ ||
        output.layout == VK_IMAGE_LAYOUT_UNDEFINED || output.image == previous.image || output.image == current.image || !std::isfinite(phase) || previous.image == VK_NULL_HANDLE ||
        previous.view == VK_NULL_HANDLE || current.image == VK_NULL_HANDLE || current.view == VK_NULL_HANDLE || current.width != previous.width || current.height != previous.height ||
        previous.format != input_format_ || current.format != input_format_ || (previous.usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0 || (current.usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0 ||
        previous.layout == VK_IMAGE_LAYOUT_UNDEFINED || current.layout == VK_IMAGE_LAYOUT_UNDEFINED)
      return Status::kInvalidArgument;
    if (std::abs(phase - 0.5f) > 0.0001f) return Status::kUnsupported;
    const ActiveRect rect = ResolveRect(previous), current_rect = ResolveRect(current);
    if (rect.width != width_ || rect.height != height_ || current_rect.x != rect.x || current_rect.y != rect.y || current_rect.width != rect.width || current_rect.height != rect.height ||
        rect.x > previous.width || rect.y > previous.height || rect.width > previous.width - rect.x || rect.height > previous.height - rect.y)
      return Status::kInvalidArgument;
    const bool crop = rect.x != 0 || rect.y != 0 || previous.width != width_ || previous.height != height_;
    if (crop) {
      if (input_format_ != VK_FORMAT_A2B10G10R10_UNORM_PACK32) return Status::kUnsupported;
      for (OwnedImage& image : tight_)
        if (image.image() == VK_NULL_HANDLE) {
          const Status status = image.Create(vulkan_, dispatch_, width_, height_, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
          if (status != Status::kSuccess) return status;
        }
    }
    const bool write_direct = direct_output_ && (output.usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0;
    if (!write_direct && (output.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) return Status::kInvalidArgument;

    // Two physical banks, logical A/B chosen for this record. Only a Real's
    // nonzero sequence and its geometry identify retained data, never a recycled VkImage.
    prep_reuse_ = (kReuseOpts & kReusePrepared) != 0 && prep_was_valid && previous.sequence != 0 && current.sequence != 0 &&
                  previous.sequence == prep_identity_.sequence && previous.width == prep_identity_.width && previous.height == prep_identity_.height &&
                  previous.format == prep_identity_.format && rect.x == prep_identity_.rect.x && rect.y == prep_identity_.rect.y &&
                  rect.width == prep_identity_.rect.width && rect.height == prep_identity_.rect.height;
    prep_a_ = prep_reuse_ ? prep_b_ : 0u;
    prep_b_ = 1u - prep_a_;

    cmd_ = cmd;
    // The record before may still be running: this one takes the next copy of the sets it writes.
    slot_ = (slot_ + 1) % uint32_t(sets_.size());
    const VkPipelineStageFlags2 kCompute = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    // What the earlier submissions of this queue wrote is visible to this record, and this record's writes do not overtake their reads.
    MemoryBarrier(kCompute | VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT, kCompute | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT);

    VkImageView view_a = previous.view, view_b = current.view;
    VkImageLayout layout_a = previous.layout, layout_b = current.layout;
    if (crop) {
      RecordCrop(previous, current, rect);
      view_a = tight_[0].view();
      view_b = tight_[1].view();
      layout_a = layout_b = VK_IMAGE_LAYOUT_GENERAL;
    }
    const VkDescriptorSet set_now = sets_[slot_], decide_now = decide_sets_[slot_];
    BindImage(set_now, 11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sampler_, view_a, layout_a);
    BindImage(set_now, 12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sampler_, view_b, layout_b);
    // The screen from the host (SetScreenTruth): this record's copy of the set only.
    screen_now_ = screen_truth_.view != VK_NULL_HANDLE;
    if (screen_now_) BindImage(set_now, 38, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sampler_, screen_truth_.view, screen_truth_.layout);
    const bool decide_half = width_ >= kHalfFrom;
    BindImage(decide_now, 11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sampler_, decide_half ? quarter_[prep_a_].view() : view_a, decide_half ? VK_IMAGE_LAYOUT_GENERAL : layout_a);
    BindImage(decide_now, 12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sampler_, decide_half ? quarter_[prep_b_].view() : view_b, decide_half ? VK_IMAGE_LAYOUT_GENERAL : layout_b);
    for (VkDescriptorSet set : {set_now, decide_now}) {
      if ((kReuseOpts & kReusePrepared) != 0) {
        // Only this flight's descriptor copies are rewritten. Earlier flights
        // keep their bank roles while queue barriers order reuse of the images.
        BindImage(set, 13, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sampler_, soft_[prep_a_].view(), VK_IMAGE_LAYOUT_GENERAL);
        BindImage(set, 14, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sampler_, soft_[prep_b_].view(), VK_IMAGE_LAYOUT_GENERAL);
        BindImage(set, 15, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, soft_[prep_a_].view(), VK_IMAGE_LAYOUT_GENERAL);
        BindImage(set, 16, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, soft_[prep_b_].view(), VK_IMAGE_LAYOUT_GENERAL);
        BindImage(set, 28, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, half_[prep_a_].view(), VK_IMAGE_LAYOUT_GENERAL);
        BindImage(set, 29, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, half_[prep_b_].view(), VK_IMAGE_LAYOUT_GENERAL);
        BindImage(set, 36, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, quarter_[prep_a_].view(), VK_IMAGE_LAYOUT_GENERAL);
        BindImage(set, 37, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, quarter_[prep_b_].view(), VK_IMAGE_LAYOUT_GENERAL);
      }
      BindImage(set, 30, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, cubic_sampler_ ? cubic_sampler_ : sampler_, view_a, layout_a);
      BindImage(set, 31, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, cubic_sampler_ ? cubic_sampler_ : sampler_, view_b, layout_b);
      BindImage(set, 33, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, write_direct ? output.view : fallback_.view(), VK_IMAGE_LAYOUT_GENERAL);
    }

    // The half-size Reals, then the base on them: the RC5 picture and its flow, into this context's own image.
    if (prep_reuse_) {
      // A stays GENERAL with the exact former B contents. Only the other bank
      // is overwritten, after the inter-pair execution/memory dependency above.
      for (const OwnedImage* image : {&half_[prep_b_], &quarter_[prep_b_], &soft_[prep_b_]})
        ImageBarrier(image->image(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0, kCompute, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    } else {
      for (const OwnedImage* image : {&half_[0], &half_[1], &quarter_[0], &quarter_[1], &soft_[0], &soft_[1]})
        ImageBarrier(image->image(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0, kCompute, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    }
    if (!prep_reuse_) Run(kHalf, Groups(width_ / 2, 8), Groups(height_ / 2, 8), 0);
    Run(kHalf1, Groups(width_ / 2, 8), Groups(height_ / 2, 8), 1);
    if (decide_half) {
      if (!prep_reuse_) Run(kQuarter, Groups(width_ / 4, 8), Groups(height_ / 4, 8), 0);
      Run(kQuarter1, Groups(width_ / 4, 8), Groups(height_ / 4, 8), 1);
    }
    Barrier();
    ImageBarrier(base_.image(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0, kCompute, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    Image half_images[2];
    for (uint32_t s = 0; s < 2; ++s) {
      const uint32_t bank = s == 0 ? prep_a_ : prep_b_;
      half_images[s].image = half_[bank].image();
      half_images[s].view = half_[bank].view();
      half_images[s].layout = VK_IMAGE_LAYOUT_GENERAL;
      half_images[s].format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
      half_images[s].width = width_ / 2;
      half_images[s].height = height_ / 2;
      half_images[s].usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
      if ((kReuseOpts & kReusePyramid) != 0) {
        // The RC7 identity check includes the source crop. Without that proof,
        // sequence zero prevents DF from reusing a differently cropped half Real.
        half_images[s].sequence = s == 0 ? (prep_reuse_ ? previous.sequence : 0) : current.sequence;
      }
    }
    Image reals[2];
    for (uint32_t s = 0; s < 2; ++s) {
      reals[s] = half_images[s];
      reals[s].image = crop ? tight_[s].image() : (s == 0 ? previous.image : current.image);
      reals[s].view = s == 0 ? view_a : view_b;
      reals[s].layout = s == 0 ? layout_a : layout_b;
      reals[s].width = width_;
      reals[s].height = height_;
    }
    Image base = reals[0];
    base.image = base_.image();
    base.view = base_.view();
    base.layout = VK_IMAGE_LAYOUT_GENERAL;
    if (!kBaseFullSize) {
      base.width = width_ / 2;
      base.height = height_ / 2;
    }
    const Status picture = kBaseFullSize ? dense_->RecordPictureFrom(cmd, half_images[0], half_images[1], reals[0], reals[1], base)
                                         : dense_->RecordPicture(cmd, half_images[0], half_images[1], base);
    if (picture != Status::kSuccess) return picture;

    dispatch_.cmd_fill_buffer(cmd, counter_.buffer(), 0, VK_WHOLE_SIZE, 0);
    WriteParams();
    if (!arrays_ready_) {
      // Arrays of images that stay in the general layout; the bends start at zero (the still surface never gets one).
      for (const OwnedImage* image : {&guide_, &bend_}) {
        VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        barrier.dstStageMask = kCompute | VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image->image();
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, VK_REMAINING_ARRAY_LAYERS};
        VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &barrier;
        dispatch_.cmd_pipeline_barrier_2(cmd, &dependency);
      }
      const VkClearColorValue zero{};
      const VkImageSubresourceRange all{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, VK_REMAINING_ARRAY_LAYERS};
      cmd_clear_color_image_(cmd, bend_.image(), VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &all);
      arrays_ready_ = true;
    }
    MemoryBarrier(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, kCompute, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_UNIFORM_READ_BIT);
    MemoryBarrier(kCompute, VK_ACCESS_2_SHADER_WRITE_BIT, kCompute, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);

    // The fused mix writes the target itself. The ordinary path prepares it later.
    VkImage target = write_direct ? output.image : fallback_.image();
    auto prepare_target = [&] {
      if (write_direct) {
        ImageBarrier(target, output.layout, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, kCompute, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
      } else {
        ImageBarrier(target, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0, kCompute, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
      }
    };
    Surfaces();
    if ((kReuseOpts & kFuseOutput) != 0) prepare_target();
    Layers();
    in_layers_ = false;
    Barrier();
    have_surfaces_ = true;

    // The picture leaves.
    if ((kReuseOpts & kFuseOutput) == 0) {
      prepare_target();
      Run(kOut, Groups(width_, 8), Groups(height_, 8));
    }
    if (write_direct) {
      ImageBarrier(output.image, VK_IMAGE_LAYOUT_GENERAL, output.layout, kCompute, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
    } else {
      ImageBarrier(fallback_.image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, kCompute, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
      ImageBarrier(output.image, output.layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
      VkImageBlit blit{};
      blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.srcSubresource.layerCount = 1;
      blit.srcOffsets[1] = {int32_t(width_), int32_t(height_), 1};
      blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.dstSubresource.layerCount = 1;
      blit.dstOffsets[1] = {int32_t(width_), int32_t(height_), 1};
      dispatch_.cmd_blit_image(cmd, fallback_.image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, output.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
      ImageBarrier(output.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, output.layout, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
    }
    if ((kReuseOpts & kReusePrepared) != 0 && current.sequence != 0) {
      prep_identity_ = {current.sequence, current.width, current.height, current.format, current_rect};
      prep_valid_ = true;
    }
    return Status::kSuccess;
  }

 private:
  bool HasOptimalFeatures(VkFormat format, VkFormatFeatureFlags features) const {
    VkFormatProperties properties{};
    dispatch_.get_physical_device_format_properties(vulkan_.physical_device, format, &properties);
    return (properties.optimalTilingFeatures & features) == features;
  }

  Status CreateResources() {
    const VkImageUsageFlags work = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    for (OwnedImage& image : soft_) {
      const Status status = image.Create(vulkan_, dispatch_, lw_, lh_, VK_FORMAT_R8G8B8A8_UNORM, work);
      if (status != Status::kSuccess) return status;
    }
    Status status = base_.Create(vulkan_, dispatch_, kBaseFullSize ? width_ : width_ / 2, kBaseFullSize ? height_ : height_ / 2, VK_FORMAT_A2B10G10R10_UNORM_PACK32, work);
    if (status != Status::kSuccess) return status;
    for (OwnedImage& image : half_) {
      status = image.Create(vulkan_, dispatch_, width_ / 2, height_ / 2, VK_FORMAT_A2B10G10R10_UNORM_PACK32, work);
      if (status != Status::kSuccess) return status;
    }
    for (OwnedImage& image : quarter_) {
      status = image.Create(vulkan_, dispatch_, std::max(1u, width_ / 4), std::max(1u, height_ / 4), VK_FORMAT_A2B10G10R10_UNORM_PACK32, work);
      if (status != Status::kSuccess) return status;
    }
    status = fallback_.Create(vulkan_, dispatch_, width_, height_, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    if (status != Status::kSuccess) return status;
    // The guided mattes' numbers and the surfaces' bends, read through the hardware's bilinear.
    status = guide_.Create(vulkan_, dispatch_, lw_, lh_, VK_FORMAT_R16G16_SFLOAT, work, 2 * (6 + 1));
    if (status != Status::kSuccess) return status;
    status = bend_.Create(vulkan_, dispatch_, std::max(1u, lw_ / 4), std::max(1u, lh_ / 4), VK_FORMAT_R16G16_SFLOAT, work | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 2 * 6);
    if (status != Status::kSuccess) return status;

    const VkBufferUsageFlags storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, dst = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const VkDeviceSize ln = VkDeviceSize(lw_) * lh_, n = VkDeviceSize(width_) * height_, planes = 6;
    const struct {
      OwnedBuffer* buffer;
      VkDeviceSize size;
      VkBufferUsageFlags usage;
    } buffers[] = {
        {&params_, sizeof(Params), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | dst},
        {&search_, 16896, storage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | dst},
        {&snap_, VkDeviceSize(fw_) * fh_ * 16, storage},
        {&cost_, ln * 4 * planes * 4, storage},
        {&label_, ln * 4 * 4, storage},
        {&plan_, ln * kPlans * 2 * 4, storage | dst},
        {&stat_, ln * 6 * 4, storage | ((kReuseOpts & kReuseStats) != 0 ? (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | dst) : 0u)},
        {&front_, ln * 5 * 8, storage | dst | VK_BUFFER_USAGE_TRANSFER_SRC_BIT},
        {&flist_, 192 + ln * 2 * 4 + VkDeviceSize(Groups(width_, 8)) * Groups(height_, 8) * 7 * 4, storage | dst | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT},
        {&palpha_, ln * 2 * planes * 4, storage},
        {&cells_, VkDeviceSize(fw_) * fh_ * 16, storage},
        {&out_, n * 4, storage},
        {&disagree_, n * 4, storage},
        {&counter_, 16, storage | dst},
        {&blocks_, VkDeviceSize(gw_) * gh_ * 2 * 16, storage},
        {&tiles_, VkDeviceSize(Groups(width_, 16)) * Groups(height_, 16) * 2 * 4, storage},
        {&thumb_, 256 + 24 * 16 + 4480 * (16 + 16) + 2 * 4 * VkDeviceSize(width_ / 8 + 2) * (height_ / 8 + 2), storage | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT},
        {&dummy_, 256, storage},
    };
    for (const auto& b : buffers) {
      status = b.buffer->Create(vulkan_, dispatch_, b.size, b.usage);
      if (status != Status::kSuccess) return status;
    }
    VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_info.addressModeU = sampler_info.addressModeV = sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (dispatch_.create_sampler(vulkan_.device, &sampler_info, vulkan_.allocator, &sampler_) != VK_SUCCESS) return Status::kVulkanError;
    // The render's sharp samples through the hardware's cubic filter, where the device has one and this is not the golden form.
    if (create_info_.backend != Backend::kCompat && capabilities_.filter_cubic_enabled && HasOptimalFeatures(input_format_, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT) &&
        (input_format_ == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || HasOptimalFeatures(VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT))) {
      VkSamplerCreateInfo cubic_info = sampler_info;
      cubic_info.magFilter = VK_FILTER_CUBIC_EXT;
      cubic_info.minFilter = VK_FILTER_CUBIC_EXT;
      if (dispatch_.create_sampler(vulkan_.device, &cubic_info, vulkan_.allocator, &cubic_sampler_) != VK_SUCCESS) cubic_sampler_ = VK_NULL_HANDLE;
    }
    return Status::kSuccess;
  }

  // One layout for every pass, bindings as in shaders/rc7/rc7_common.glsl, 33 the output. Two sets, which differ in the pictures only: the decisions may read the half-size ones.
  Status CreatePipelines() {
    const VkDescriptorType kS = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kI = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, kB = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    std::vector<VkDescriptorType> types(39, kB);
    types[0] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    types[9] = types[11] = types[12] = types[13] = types[14] = types[22] = types[23] = types[30] = types[31] = types[38] = kS;
    types[15] = types[16] = types[28] = types[29] = types[33] = types[34] = types[35] = types[36] = types[37] = kI;
    std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
    for (uint32_t i = 0; i < types.size(); ++i) {
      VkDescriptorSetLayoutBinding b{};
      b.binding = i;
      b.descriptorType = types[i];
      b.descriptorCount = 1;
      b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
      layout_bindings.push_back(b);
    }
    VkDescriptorSetLayoutCreateInfo dsl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dsl.bindingCount = uint32_t(layout_bindings.size());
    dsl.pBindings = layout_bindings.data();
    if (dispatch_.create_descriptor_set_layout(vulkan_.device, &dsl, vulkan_.allocator, &set_layout_) != VK_SUCCESS) return Status::kVulkanError;
    VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push)};
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &set_layout_;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &range;
    if (dispatch_.create_pipeline_layout(vulkan_.device, &pl, vulkan_.allocator, &layout_) != VK_SUCCESS) return Status::kVulkanError;
    // One copy of the sets per frame context: a record writes the Reals and the output into its own copy, while the record before may still be running.
    const uint32_t copies = std::max(1u, create_info_.frame_context_count);
    VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * copies}, {kS, 20 * copies}, {kI, 18 * copies}, {kB, 38 * copies}};
    VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpi.maxSets = 2 * copies;
    dpi.poolSizeCount = 4;
    dpi.pPoolSizes = sizes;
    if (dispatch_.create_descriptor_pool(vulkan_.device, &dpi, vulkan_.allocator, &pool_) != VK_SUCCESS) return Status::kVulkanError;
    VkDescriptorSetAllocateInfo dsa{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsa.descriptorPool = pool_;
    dsa.descriptorSetCount = 1;
    dsa.pSetLayouts = &set_layout_;
    sets_.assign(copies, VK_NULL_HANDLE);
    decide_sets_.assign(copies, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < copies; ++i) {
      if (dispatch_.allocate_descriptor_sets(vulkan_.device, &dsa, &sets_[i]) != VK_SUCCESS) return Status::kVulkanError;
      if (dispatch_.allocate_descriptor_sets(vulkan_.device, &dsa, &decide_sets_[i]) != VK_SUCCESS) return Status::kVulkanError;
    }

    // The crop has a layout of its own, as in the other contexts.
    {
      VkDescriptorSetLayoutBinding crop_bindings[2] = {};
      crop_bindings[0] = {0, kS, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
      crop_bindings[1] = {1, kI, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
      VkDescriptorSetLayoutCreateInfo cdsl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
      cdsl.bindingCount = 2;
      cdsl.pBindings = crop_bindings;
      if (dispatch_.create_descriptor_set_layout(vulkan_.device, &cdsl, vulkan_.allocator, &crop_set_layout_) != VK_SUCCESS) return Status::kVulkanError;
      VkPushConstantRange crange{VK_SHADER_STAGE_COMPUTE_BIT, 0, 24};
      VkPipelineLayoutCreateInfo cpl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
      cpl.setLayoutCount = 1;
      cpl.pSetLayouts = &crop_set_layout_;
      cpl.pushConstantRangeCount = 1;
      cpl.pPushConstantRanges = &crange;
      if (dispatch_.create_pipeline_layout(vulkan_.device, &cpl, vulkan_.allocator, &crop_layout_) != VK_SUCCESS) return Status::kVulkanError;
      VkDescriptorPoolSize csizes[] = {{kS, 2 * copies}, {kI, 2 * copies}};
      VkDescriptorPoolCreateInfo cdpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
      cdpi.maxSets = 2 * copies;
      cdpi.poolSizeCount = 2;
      cdpi.pPoolSizes = csizes;
      if (dispatch_.create_descriptor_pool(vulkan_.device, &cdpi, vulkan_.allocator, &crop_pool_) != VK_SUCCESS) return Status::kVulkanError;
      const std::vector<VkDescriptorSetLayout> crop_layouts(2 * copies, crop_set_layout_);
      crop_sets_.assign(2 * copies, VK_NULL_HANDLE);
      VkDescriptorSetAllocateInfo cdsa{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
      cdsa.descriptorPool = crop_pool_;
      cdsa.descriptorSetCount = 2 * copies;
      cdsa.pSetLayouts = crop_layouts.data();
      if (dispatch_.allocate_descriptor_sets(vulkan_.device, &cdsa, crop_sets_.data()) != VK_SUCCESS) return Status::kVulkanError;
    }

    struct Spec {
      uint32_t pipe;
      const uint32_t* code;
      size_t bytes;
      int side, kind, surface;
    };
#define RC7_SPEC(P, NAME) {P, zerofg_rc7_##NAME##_spv, sizeof(zerofg_rc7_##NAME##_spv), 0, 0, -1}
#define RC7_SIDES(P, NAME, KIND) {P, zerofg_rc7_##NAME##_spv, sizeof(zerofg_rc7_##NAME##_spv), 0, KIND, -1}, {P##1, zerofg_rc7_##NAME##_spv, sizeof(zerofg_rc7_##NAME##_spv), 1, KIND, -1}
#define RC7_COST(P, SIDE, KIND) {P, zerofg_rc7_cost_spv, sizeof(zerofg_rc7_cost_spv), SIDE, KIND, -1}
#define RC7_ONE(K) {kRenderOne + K, zerofg_rc7_render_spv, sizeof(zerofg_rc7_render_spv), 0, 0, K}
    // What the passes know when they are built: blocks, behind, two passes, the screen's hole, guided mattes, the cubic filter, the front always first, the groups from a list, the costs and the statistics over a checkerboard of the
    // patch, the guided window at its radius' step. KIND of the guided mattes: the softened copy is smaller than the Reals.
    const int render_opts = 1 | 2 | 4 | 8 | 16 | (cubic_sampler_ ? 32 : 0) | 64 | 256 | 512 | 1024 | 2048 | 4096 | 8192;
    const Spec specs[] = {{kCrop, zerofg_rc3_crop_spv, sizeof(zerofg_rc3_crop_spv), 0, 0, -1}, RC7_SIDES(kHalf, half, 0), RC7_SIDES(kQuarter, quarter, 0), RC7_SPEC(kFList, flist), RC7_SIDES(kSoft, soft, 0), RC7_SIDES(kStats, stats, 0), RC7_SPEC(kHud, hud), RC7_SPEC(kHudRule, hudrule),
                          RC7_COST(kCost, 0, 0), RC7_COST(kCost1, 1, 0), RC7_COST(kCost2, 0, 1), RC7_COST(kCost3, 1, 1),
                          RC7_SPEC(kSeed, seed), RC7_SIDES(kJump, jump, 0), RC7_SPEC(kRaw, raw), RC7_SIDES(kBlockPass, block, 0), RC7_SIDES(kStand, stand, 0), RC7_SPEC(kGhost, ghost),
                          RC7_SPEC(kMorph, morph), RC7_SIDES(kFront, front, 0), RC7_SIDES(kFSmooth, fsmooth, 0), RC7_SIDES(kUnfold, unfold, 0), RC7_SPEC(kHole, hole), RC7_SPEC(kCons, cons), RC7_SPEC(kExtend, extend), RC7_SPEC(kAlpha, alpha), RC7_SPEC(kRender, render),
                          RC7_ONE(0), RC7_ONE(1), RC7_ONE(2), RC7_ONE(3), RC7_ONE(4), RC7_ONE(5),
                          RC7_SPEC(kTrust, trust), RC7_SPEC(kMix, mix), RC7_SIDES(kFine, fine, 0), RC7_SIDES(kGuide, guide, 1), RC7_SPEC(kTile, tile), RC7_SPEC(kSCell, scell), RC7_SPEC(kSStart, sstart),
                          RC7_SPEC(kSList, slist), RC7_SPEC(kSHyp, shyp), RC7_SPEC(kSFit, sfit), RC7_SPEC(kSRefit, srefit), RC7_SPEC(kSInl, sinl), RC7_SPEC(kSEcho, secho),
                          RC7_SPEC(kSDone, sdone), RC7_SPEC(kSOrder, sorder), RC7_SPEC(kSGate, sgate), RC7_SPEC(kTBlock, tblock), RC7_SPEC(kTFit, tfit), RC7_SPEC(kTGuide, tguide), RC7_SPEC(kOut, out),
                          {kStatsSharp, zerofg_rc7_stats_spv, sizeof(zerofg_rc7_stats_spv), 0, 1, -1}, RC7_SPEC(kHudGs, hudgs)};
#undef RC7_ONE
#undef RC7_COST
#undef RC7_SIDES
#undef RC7_SPEC
    for (const Spec& s : specs) {
      if (s.pipe == kStatsSharp && (kReuseOpts & kReuseStats) == 0) continue;
      const int spec_values[7] = {s.side, s.kind, s.surface, kFrontRoundsBend, kFrontRoundsSample, render_opts, kReuseOpts};
      const VkSpecializationMapEntry spec_entries[7] = {{0, 0, 4}, {1, 4, 4}, {2, 8, 4}, {3, 12, 4}, {4, 16, 4}, {5, 20, 4}, {6, 24, 4}};
      const VkSpecializationInfo spec_info{7, spec_entries, sizeof(spec_values), spec_values};
      VkShaderModuleCreateInfo smi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
      smi.codeSize = s.bytes;
      smi.pCode = s.code;
      VkShaderModule module = VK_NULL_HANDLE;
      if (dispatch_.create_shader_module(vulkan_.device, &smi, vulkan_.allocator, &module) != VK_SUCCESS) return Status::kVulkanError;
      VkComputePipelineCreateInfo cpi{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
      cpi.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      cpi.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
      cpi.stage.module = module;
      cpi.stage.pName = "main";
      cpi.stage.pSpecializationInfo = &spec_info;
      cpi.layout = s.pipe == kCrop ? crop_layout_ : layout_;
      const VkResult result = dispatch_.create_compute_pipelines(vulkan_.device, VK_NULL_HANDLE, 1, &cpi, vulkan_.allocator, &pipelines_[s.pipe]);
      dispatch_.destroy_shader_module(vulkan_.device, module, vulkan_.allocator);
      if (result != VK_SUCCESS) return Status::kVulkanError;
    }
    return Status::kSuccess;
  }

  template <typename F>
  void ForEverySet(F f) {
    for (VkDescriptorSet set : sets_) f(set);
    for (VkDescriptorSet set : decide_sets_) f(set);
  }
  void BindBuffer(uint32_t binding, VkBuffer buffer, VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
    VkDescriptorBufferInfo info{buffer, 0, VK_WHOLE_SIZE};
    ForEverySet([&](VkDescriptorSet set) {
      VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      write.dstSet = set;
      write.dstBinding = binding;
      write.descriptorCount = 1;
      write.descriptorType = type;
      write.pBufferInfo = &info;
      dispatch_.update_descriptor_sets(vulkan_.device, 1, &write, 0, nullptr);
    });
  }

  void BindImage(VkDescriptorSet set, uint32_t binding, VkDescriptorType type, VkSampler sampler, VkImageView view, VkImageLayout layout) {
    VkDescriptorImageInfo info{sampler, view, layout};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = set;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &info;
    dispatch_.update_descriptor_sets(vulkan_.device, 1, &write, 0, nullptr);
  }
  void BindSampled(uint32_t binding, VkImageView view, VkImageLayout layout) {
    ForEverySet([&](VkDescriptorSet set) { BindImage(set, binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sampler_, view, layout); });
  }
  void BindStorageImage(uint32_t binding, VkImageView view) {
    ForEverySet([&](VkDescriptorSet set) { BindImage(set, binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_GENERAL); });
  }

  void BindStatic() {
    BindBuffer(0, params_.buffer(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    const struct {
      uint32_t binding;
      VkBuffer buffer;
    } buffers[] = {{1, cost_.buffer()},  {2, label_.buffer()},  {3, plan_.buffer()},    {4, stat_.buffer()},     {5, front_.buffer()},  {6, palpha_.buffer()},
                   {7, cells_.buffer()}, {8, out_.buffer()},    {10, disagree_.buffer()}, {17, dense_->flow_buffer()}, {18, counter_.buffer()}, {19, blocks_.buffer()},
                   {20, dummy_.buffer()}, {21, dummy_.buffer()}, {24, tiles_.buffer()}, {25, search_.buffer()},
                   {26, thumb_.buffer()}, {27, flist_.buffer()}, {32, snap_.buffer()}};
    for (const auto& b : buffers) BindBuffer(b.binding, b.buffer);
    const VkImageLayout general = VK_IMAGE_LAYOUT_GENERAL;
    BindSampled(9, base_.view(), general);
    BindSampled(11, base_.view(), general);  // until the first record
    BindSampled(12, base_.view(), general);
    BindSampled(30, base_.view(), general);
    BindSampled(31, base_.view(), general);
    BindSampled(13, soft_[0].view(), general);
    BindSampled(14, soft_[1].view(), general);
    BindSampled(22, bend_.view(), general);
    BindSampled(23, guide_.view(), general);
    BindSampled(38, base_.view(), general);  // the host's screen (SetScreenTruth), bound per record when there is one
    BindStorageImage(34, guide_.view());
    BindStorageImage(35, bend_.view());
    BindStorageImage(15, soft_[0].view());
    BindStorageImage(16, soft_[1].view());
    BindStorageImage(28, half_[0].view());
    BindStorageImage(29, half_[1].view());
    BindStorageImage(36, quarter_[0].view());
    BindStorageImage(37, quarter_[1].view());
    BindStorageImage(33, fallback_.view());
  }

  // ---- recording ---------------------------------------------------------------------------------------------------------------------------
  void MemoryBarrier(VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access) {
    VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    barrier.srcStageMask = src_stage;
    barrier.srcAccessMask = src_access;
    barrier.dstStageMask = dst_stage;
    barrier.dstAccessMask = dst_access;
    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency.memoryBarrierCount = 1;
    dependency.pMemoryBarriers = &barrier;
    dispatch_.cmd_pipeline_barrier_2(cmd_, &dependency);
  }

  // The broad SHADER_WRITE as the source: the storage-only bit leaves some drivers' writes unordered (the laboratory's lesson on the AMD Windows driver). The search's next step goes first in what comes after: the command
  // processor starts a dispatch only when the one before has all its waves out, so a narrow step after a wide pass would add its whole length.
  void Barrier(bool wide_next = false) {
    if (step_indirect_) {
      IndirectBarrier();
      step_indirect_ = false;
    } else {
      MemoryBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    }
    if (in_layers_) NextStep(wide_next);
  }

  // A long step waits for a wide pass to go along with, next to a short one it would only stretch it; and in the background one per pair is enough: the surfaces are followed in the meantime.
  void NextStep(bool wide_next) {
    step_now_us_ = 0.0;
    if (step_next_ >= steps_.size()) return;
    const bool is_long = steps_[step_next_].us >= 100.0;
    if (is_long && (!wide_next || (in_layers_ && long_done_))) return;
    if (is_long) long_done_ = true;
    const size_t step = step_next_++;
    steps_[step].run();
    step_indirect_ = steps_[step].indirect;
    step_now_us_ = steps_[step].us;
    if (step_next_ >= steps_.size()) search_fresh_ = true;
  }

  void AddStep(std::function<void()> run, bool indirect = false, double us = 20.0) { steps_.push_back({std::move(run), indirect, us}); }

  // A wide pass in rows of groups, cut to the length of the search's step that goes along. With no step to carry, all at once.
  void Sliced(uint32_t rows, double pass_us, const std::function<void(uint32_t y, uint32_t count)>& record) {
    uint32_t y = 0;
    while (y < rows) {
      uint32_t count = rows - y;
      if (in_layers_ && step_now_us_ > 0.0) count = std::min(count, std::max(1u, uint32_t(std::ceil(double(rows) * step_now_us_ / pass_us))));
      record(y, count);
      y += count;
      if (y < rows) Barrier(true);
    }
  }

  void IndirectBarrier() {
    MemoryBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
  }

  void ImageBarrier(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access) {
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
    dispatch_.cmd_pipeline_barrier_2(cmd_, &dependency);
  }

  void Bind(uint32_t pipe, const Push& push) {
    dispatch_.cmd_bind_pipeline(cmd_, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_[pipe]);
    const bool decides = pipe == kSoft || pipe == kSoft1 || pipe == kStats || pipe == kStats1 || pipe == kStatsSharp || pipe == kHud || (pipe >= kCost && pipe <= kCost3) || pipe == kBlockPass || pipe == kBlockPass1 || pipe == kStand || pipe == kStand1 ||
                         pipe == kFront || pipe == kFront1 || pipe == kFSmooth || pipe == kFSmooth1;
    dispatch_.cmd_bind_descriptor_sets(cmd_, VK_PIPELINE_BIND_POINT_COMPUTE, layout_, 0, 1, decides ? &decide_sets_[slot_] : &sets_[slot_], 0, nullptr);
    dispatch_.cmd_push_constants(cmd_, layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &push);
  }

  void Run(uint32_t pipe, uint32_t gx, uint32_t gy, int a0 = 0, int a1 = 0, int a2 = 0, int a3 = 0, int b0 = 0, int b1 = 0) {
    Push push;
    push.a[0] = a0;
    push.a[1] = a1;
    push.a[2] = a2;
    push.a[3] = a3;
    push.b[0] = b0;
    push.b[1] = b1;
    Bind(pipe, push);
    dispatch_.cmd_dispatch(cmd_, std::max(1u, gx), std::max(1u, gy), 1);
  }

  void Indirect(uint32_t pipe, VkDeviceSize offset, int a0) {
    Push push;
    push.a[0] = a0;
    Bind(pipe, push);
    cmd_dispatch_indirect_(cmd_, thumb_.buffer(), offset);
  }

  // A pass over the matte's points of one picture: as many groups of 64 as the list needs.
  void Listed(uint32_t pipe, int side, int a1 = 0, int a2 = 0, int a3 = 0, int b0 = 0, int b1 = 0) {
    Push push;
    push.a[0] = side;
    push.a[1] = a1;
    push.a[2] = a2;
    push.a[3] = a3;
    push.b[0] = b0;
    push.b[1] = b1;
    Bind(pipe, push);
    cmd_dispatch_indirect_(cmd_, flist_.buffer(), VkDeviceSize(side) * 16);
  }

  // On the lattice, and over the whole picture.
  void Lat(uint32_t pipe, int a0 = 0, int a1 = 0, int a2 = 0, int a3 = 0, int b0 = 0, int b1 = 0) { Run(pipe, Groups(lw_, 8), Groups(lh_, 8), a0, a1, a2, a3, b0, b1); }
  void Whole(uint32_t pipe, int a0 = 0, int a1 = 0, int a2 = 0, int a3 = 0) { Run(pipe, Groups(width_, 8), Groups(height_, 8), a0, a1, a2, a3); }

  void WriteParams() {
    Params p{};
    const int32_t dim[4] = {int32_t(width_), int32_t(height_), int32_t(lw_), int32_t(lh_)}, cfg[4] = {int32_t(step_), 6, kPatch, 1},
                  cfg2[4] = {int32_t(fw_), int32_t(fh_), int32_t(dense_->flow_width()), int32_t(dense_->flow_height())}, cfg3[4] = {int32_t(dense_->flow_offset()), 0, kUnfoldBar, kHudMemory};
    std::copy(dim, dim + 4, p.dim);
    std::copy(cfg, cfg + 4, p.cfg);
    std::copy(cfg2, cfg2 + 4, p.cfg2);
    std::copy(cfg3, cfg3 + 4, p.cfg3);
    const float cell[4] = {cell_w_, cell_h_, float(2u << kFlowLevel), float(std::max(1, int(width_) / 256))};
    std::copy(cell, cell + 4, p.cell);
    const float th0[4] = {8 * 0.001f, 3 * 0.001f, 50 * 0.01f, 30 * 0.01f}, th1[4] = {100 * 0.01f, 80 * 0.01f, 15 * 0.001f, 300 * 0.01f}, th2[4] = {0.0f, 25 * 0.01f, 100 * 0.001f, 20 * 0.1f},
                th3[4] = {kBaseBar * 0.01f, 1.0f, 1.0f, 1.0f}, th4[4] = {50 * 0.01f, 1.0f, 1.0f, 0.0f}, judge1[4] = {1.0f, 12.0f, 8.0f, 20 * 0.001f * 20 * 0.001f};
    std::copy(th0, th0 + 4, p.th0);
    std::copy(th1, th1 + 4, p.th1);
    std::copy(th2, th2 + 4, p.th2);
    std::copy(th3, th3 + 4, p.th3);
    p.judge0[2] = kBaseBarFront * 0.01f;
    std::copy(th4, th4 + 4, p.th4);
    std::copy(judge1, judge1 + 4, p.judge1);
    cmd_update_buffer_(cmd_, params_.buffer(), 0, sizeof(Params), &p);
  }

  void CopyToParams(VkDeviceSize from, VkDeviceSize to, VkDeviceSize bytes) {
    MemoryBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkBufferCopy region{from, to, bytes};
    dispatch_.cmd_copy_buffer(cmd_, search_.buffer(), params_.buffer(), 1, &region);
    MemoryBarrier(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_UNIFORM_READ_BIT);
  }

  void RecordCrop(const Image& previous, const Image& current, const ActiveRect& rect) {
    const VkPipelineStageFlags2 kCompute = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    struct CropPush {
      int32_t origin[2], extent[2], picture[2];
    } push{{int32_t(rect.x), int32_t(rect.y)}, {int32_t(width_), int32_t(height_)}, {int32_t(width_), int32_t(height_)}};
    static_assert(sizeof(CropPush) == 24);
    const Image* sources[2] = {&previous, &current};
    for (uint32_t s = 0; s < 2; ++s) {
      ImageBarrier(tight_[s].image(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0, kCompute, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
      VkDescriptorSet crop_set = crop_sets_[s + 2 * slot_];
      BindImage(crop_set, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sampler_, sources[s]->view, sources[s]->layout);
      BindImage(crop_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, tight_[s].view(), VK_IMAGE_LAYOUT_GENERAL);
      dispatch_.cmd_bind_pipeline(cmd_, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_[kCrop]);
      dispatch_.cmd_bind_descriptor_sets(cmd_, VK_PIPELINE_BIND_POINT_COMPUTE, crop_layout_, 0, 1, &crop_set, 0, nullptr);
      dispatch_.cmd_push_constants(cmd_, crop_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
      dispatch_.cmd_dispatch(cmd_, Groups(width_, 8), Groups(height_, 8), 1);
    }
    Barrier();
  }

  void CopySurfaces() {
    CopyToParams(0, offsetof(Params, order), 32 + 54 * 16);
    CopyToParams(912, offsetof(Params, cfg) + 4, 4);
  }

  void CopyInSearch(VkDeviceSize from, VkDeviceSize to) {
    MemoryBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkBufferCopy region{from, to, 18 * 16};
    dispatch_.cmd_copy_buffer(cmd_, search_.buffer(), search_.buffer(), 1, &region);
    MemoryBarrier(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
  }

  // The surfaces at this pair's motion. The first pair does its whole search now. After that the search goes on in the background, and this pair's flow brings the surfaces up to its motion.
  void Surfaces() {
    const uint32_t cx = Groups(fw_, 8), cy = Groups(fh_, 8);
    if (!have_surfaces_) {
      BuildSearch();
      Run(kSCell, cx, cy, 0, 1);
      Barrier();
      Run(kSCell, cx, cy, 1);
      Barrier();
      while (step_next_ < steps_.size()) {
        NextStep(true);
        Barrier();
      }
      CopySurfaces();
      return;
    }
    const bool start = step_next_ >= steps_.size();
    CopySurfaces();
    if (search_fresh_) {
      CopyInSearch(32, kFollowOffset);
      search_fresh_ = false;
    }
    Run(kSCell, cx, cy, 0, start ? 1 : 0);
    Run(kSRefit, 9, 1, 2);
    Barrier();
    Run(kSRefit, cx, cy, 0);
    Barrier();
    Run(kSRefit, 1, 1, 1);
    CopyToParams(kRefitOffset, offsetof(Params, H), 54 * 16);
    CopyInSearch(kRefitOffset, kFollowOffset);
    if (start) BuildSearch();
    in_layers_ = true;
    long_done_ = false;
    Run(kSCell, cx, cy, 1);
    NextStep(false);
  }

  // The surfaces, as a list of steps, a barrier after each. Each fit: a sample of what is left, all the trials at once, the best one and its rounds, the cells it explains, the test against the echoes.
  void BuildSearch() {
    steps_.clear();
    step_next_ = 0;
    const uint32_t cx = Groups(fw_, 8), cy = Groups(fh_, 8);
    const uint32_t echo_step = std::max(8u, width_ / 80), ex = Groups(width_ / echo_step, 8), ey = Groups(height_ / echo_step, 8);
    AddStep([=, this] { Run(kSStart, 1, 1); });
    for (int fit = 0; fit < kPeels; ++fit) {
      AddStep([=, this] { Run(kSList, 1, 1, 0); });
      AddStep([=, this] { Run(kSList, 4, 1, 1); });
      AddStep([=, this] {
        Run(kSHyp, 2, 1, 0, 0);
        Run(kSHyp, 1, 1, 0, 1);
      }, false, 130.0);
      AddStep([=, this] { Run(kSFit, 1, 1, 0, 0, 6, 1, 100); }, false, 300.0);
      AddStep([=, this] { Run(kSInl, cx, cy); });
      AddStep([=, this] { Run(kSEcho, ex, ey); });
      AddStep([=, this] { Run(kSDone, 1, 1); });
    }
    FastCameraSteps();
    AddStep([=, this] { Run(kSOrder, 1, 1); });
  }

  // Past the dense flow's reach. When the flow keeps up, the first step leaves every indirect dispatch at zero and the rest find nothing to do.
  void FastCameraSteps() {
    const uint32_t thumb_step = std::max(8u, (std::max(width_, height_) / 80) & ~1u), tw = width_ / thumb_step, th = height_ / thumb_step, bw = tw / 6, bh = th / 6;
    const int tolerance = int(thumb_step) * 50;
    const uint32_t pic_step = std::max(8u, width_ / 80), px = Groups(width_ / pic_step, 8), py = Groups(height_ / pic_step, 8), bx = Groups(bw, 8), by = Groups(bh, 8);
    AddStep([=, this] { Run(kTBlock, 1, 1, 0); }, true);
    AddStep([=, this] { Run(kTBlock, Groups(tw, 8), Groups(th, 8), 1); });
    AddStep([=, this] { Indirect(kTBlock, 0, 2); });
    AddStep([=, this] { Indirect(kTBlock, 16, 3); });
    AddStep([=, this] { Run(kTBlock, bx, by, 4); });
    AddStep([=, this] { Run(kTFit, 1, 1, 0); }, true);
    for (int fit = 0; fit < 2; ++fit) {
      AddStep([=, this] { Run(kTFit, bx, by, 1); });
      AddStep([=, this] { Run(kTFit, 1, 1, 2); });
      AddStep([=, this] { Run(kTFit, 1, 1, 3); });
      AddStep([=, this] { Run(kTFit, px, py, 4); });
      AddStep([=, this] { Run(kTFit, 1, 1, 5); });
      AddStep([=, this] { Run(kSFit, 1, 1, 0, 1, 8, 0, tolerance); });
      AddStep([=, this] { Run(kTFit, bx, by, 6); });
      AddStep([=, this] { Run(kSEcho, px, py, 1, int(bw), int(bh), int(6 * thumb_step)); });
      AddStep([=, this] { Run(kTFit, 1, 1, 7); });
      for (int level : {2, 4}) {
        const uint32_t gx = Groups(width_ / (thumb_step / level) / 8, 8), gy = Groups(height_ / (thumb_step / level) / 8, 8);
        const int level_tolerance = std::max(100, int(thumb_step / level) * 50);
        AddStep([=, this] { Run(kTGuide, 1, 1, 0, level); }, true);
        AddStep([=, this] { Run(kTGuide, gx, gy, 1); });
        AddStep([=, this] { Indirect(kTGuide, 32, 2); });
        AddStep([=, this] { Run(kTGuide, 1, 1, 3); });
        AddStep([=, this] { Run(kSFit, 1, 1, 0, 0, 8, 0, level_tolerance); });
      }
      AddStep([=, this] { Run(kTGuide, 1, 1, 4); });
    }
  }

  void Layers() {
    // The softened Reals, at the lattice's size.
    if (!prep_reuse_) Lat(kSoft, 0);
    Lat(kSoft1, 1);
    Barrier();
    // Everything each picture can say alone, all the surfaces at once.
    if ((kReuseOpts & kReuseStats) != 0 && prep_reuse_) {
      // The previous B is this A. Soft texture and wrong-match baseline depend
      // only on that Real, not the pair. Preserve their exact stored bits before
      // B's normal pipeline replaces them; A's sharp-only pipeline stays exact.
      const VkDeviceSize bytes = VkDeviceSize(lw_) * lh_ * sizeof(float);
      const VkBufferCopy copies[2] = {{3 * bytes, 2 * bytes, bytes}, {5 * bytes, 4 * bytes, bytes}};
      MemoryBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
      dispatch_.cmd_copy_buffer(cmd_, stat_.buffer(), stat_.buffer(), 2, copies);
      MemoryBarrier(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
      Lat(kStatsSharp, 0);
    } else {
      Lat(kStats, 0);
    }
    Lat(kStats1, 1);
    Barrier(true);
    {
      const uint32_t gx = Groups(lw_, 8);
      // About 0.5 ms on the Adreno at both sizes the lattice takes.
      Sliced(Groups(lh_, 8), 500.0, [&](uint32_t y, uint32_t count) {
        for (int kind = 0; kind < 2; ++kind)
          for (int side = 0; side < 2; ++side) Run(kCost + kind * 2 + side, gx, count, side, kind, 0, 0, int(y));
      });
    }
    Run(kSGate, 32, 1, 0);
    Barrier();
    if (!hud_memory_) {
      const VkDeviceSize points = VkDeviceSize(lw_) * lh_;
      dispatch_.cmd_fill_buffer(cmd_, plan_.buffer(), VkDeviceSize(P_HUD * 2 + 1) * points * 4, points * 4, 0);
      MemoryBarrier(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
      hud_memory_ = true;
    }
    // The screen and the gates: nothing of one reads anything of the other, so they go along, step by step. A screen the host knows (the
    // GS modes' HUD planes, SetScreenTruth) is added to the guess, never put in its place: the planes miss what the GS rule cannot see (a
    // cursor drawn with the character), the guess catches that. It packs into the steps already there: it goes along with the guess (it
    // reads only the host's code image) on the memory's side, and the rule adds it after its own decision; no step or barrier of its own.
    static_assert(kHudMemory == 0, "the host's screen is written on the screen's memory side: with the memory on it needs a plan of its own");
    Lat(kHud);
    if (screen_now_) Lat(kHudGs);
    Lat(kSeed, 0, 1);
    Lat(kSeed, 1, 1);
    Barrier();
    Lat(kHudRule, screen_now_ ? 1 : 0);
    Run(kSGate, Groups(std::max(1u, lw_ / 2), 8), Groups(std::max(1u, lh_ / 2), 8), 1);
    Run(kSGate, Groups(std::max(1u, lw_ / 4), 8), Groups(std::max(1u, lh_ / 4), 8), 3);
    Barrier();
    Lat(kMorph, 0, P_HUD, P_TMP, 2, 0);
    Run(kSGate, 1, 1, 2, 0, 0, 0, kFloorGate, kCutCorrelation);
    CopyToParams(896, offsetof(Params, gate), 16);
    CopyToParams(kCutOffset, offsetof(Params, cfg3) + 4, 4);
    Barrier();
    Lat(kMorph, 0, P_TMP, P_HUD, 2, 1);
    Barrier();
    // The labels: seeds, then jumps, every point reading the round before; the last one also fixes what is left.
    Lat(kSeed, 0);
    Lat(kSeed, 1);
    Barrier();
    int flip = 1;
    const int label_jumps[3] = {8, 2, 1};
    for (int j = 0; j < 3; ++j) {
      Lat(kJump, 0, label_jumps[j], flip, 0, j == 2 ? 1 : 0);
      Lat(kJump1, 1, label_jumps[j], flip, 0, j == 2 ? 1 : 0);
      Barrier();
      flip = 1 - flip;
    }
    // The surfaces' last fraction of a pixel, with the raw matte. The still surface does not bend.
    for (int side = 0; side < 2; ++side)
      for (int k = 0; k < 5; ++k) Run(kFine + side, Groups(std::max(1u, lw_ / 4), 8), Groups(std::max(1u, lh_ / 4), 8), side, k, 0);
    Lat(kRaw, 0);
    Lat(kRaw, 1);
    Barrier();
    // The mattes.
    Run(kBlockPass, Groups(gw_, 4), Groups(gh_, 4), 0, int(block_));
    Run(kBlockPass1, Groups(gw_, 4), Groups(gh_, 4), 1, int(block_));
    Barrier();
    Lat(kStand, 0, int(block_));
    Lat(kStand1, 1, int(block_));
    Barrier();
    Lat(kGhost, 0);
    Lat(kGhost, 1);
    Barrier();
    const int rc = std::max(2, 6 / int(step_));
    Lat(kMorph, 0, P_TMP, P_DIST1, rc, 0);
    Lat(kMorph, 1, P_TMP, P_DIST1, rc, 0);
    Barrier();
    Lat(kMorph, 0, P_DIST1, P_TMP, rc, 1);
    Lat(kMorph, 1, P_DIST1, P_TMP, rc, 1);
    Barrier();
    // A hole in the matte on one picture only reads as an occlusion, and the block that comes of it is a pop.
    const int reach = std::max(4, kHoleReach / int(step_));
    Lat(kHole, 0, reach);
    Lat(kHole, 1, reach);
    Barrier();
    // The front's field: both pictures in every round. Flips: first evaluation into 1, the rounds back to 1, two medians, the soft step into 0. Only the matte's points run; everywhere else it is cleared here.
    // B of the last pair is A of this one when the host says so: its field, kept before it goes, is a candidate of the first evaluation; a field of another scene is just a candidate that loses.
    const VkDeviceSize ln = VkDeviceSize(lw_) * lh_, field = ln * 8;
    if (have_field_) {
      MemoryBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT);
      const VkBufferCopy last{field, field * 4, field};
      dispatch_.cmd_copy_buffer(cmd_, front_.buffer(), front_.buffer(), 1, &last);
      MemoryBarrier(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
      dispatch_.cmd_fill_buffer(cmd_, front_.buffer(), 0, field * 4, 0);
    } else {
      dispatch_.cmd_fill_buffer(cmd_, front_.buffer(), 0, VK_WHOLE_SIZE, 0);
    }
    have_field_ = true;
    dispatch_.cmd_fill_buffer(cmd_, plan_.buffer(), VkDeviceSize(P_FCOST0 * 2) * ln * 4, 4 * ln * 4, 0x3f800000u);
    dispatch_.cmd_fill_buffer(cmd_, flist_.buffer(), 32, 8, 0);
    dispatch_.cmd_fill_buffer(cmd_, flist_.buffer(), 160, 28, 0);
    MemoryBarrier(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    Lat(kFList, 0, 0);
    Lat(kFList, 0, 1);
    Barrier();
    Run(kFList, 1, 1, 1);
    IndirectBarrier();
    Listed(kFront, 0, 0, 0, 0, 1, 1);
    Listed(kFront1, 1, 0, 0, 0, 1, 1);
    Barrier();
    flip = 1;
    int round = 0;
    for (int jump : {8, 2}) {
      Listed(kFront, 0, flip, jump, round);
      Listed(kFront1, 1, flip, jump, round);
      Barrier();
      flip = 1 - flip;
      ++round;
    }
    for (int pass = 0; pass < 2; ++pass) {
      Listed(kFSmooth, 0, flip, 0, 0, 0);
      Listed(kFSmooth1, 1, flip, 0, 0, 0);
      Barrier();
      flip = 1 - flip;
    }
    Listed(kFSmooth, 0, flip, 0, 0, 1);
    Listed(kFSmooth1, 1, flip, 0, 0, 1);
    Barrier();
    flip = 1 - flip;
    // A field that folds is no motion: on a fence each point matches a different bar and the bars come out as waves.
    Listed(kUnfold, 0, flip, 0, 0, 0);
    Listed(kUnfold1, 1, flip, 0, 0, 0);
    Barrier();
    Listed(kUnfold, 0, flip, 1, 0, 0);
    Listed(kUnfold1, 1, flip, 1, 0, 0);
    Barrier();
    // These read what is done and write different things, so they go along: what comes back, the field's first step, the soft mattes; then the guided mattes and the blocks' clear with the field's next.
    Lat(kCons, 0);
    Lat(kCons, 1);
    Lat(kExtend, 0, flip, 0);
    Lat(kExtend, 1, flip, 0);
    Lat(kAlpha, 0);
    Lat(kAlpha, 1);
    Barrier();
    flip = 1 - flip;
    const uint32_t tx = Groups(Groups(width_, 16), 8), ty = Groups(Groups(height_, 16), 8);
    for (int jump : {6, 2, 1}) {
      Lat(kExtend, 0, flip, jump);
      Lat(kExtend, 1, flip, jump);
      if (jump == 6) {
        Lat(kGuide, 0);
        Lat(kGuide1, 1);
        Run(kTile, tx, ty, 0);
      }
      Barrier();
      flip = 1 - flip;
    }
    // Which layers exist in each block of the picture, each group of the picture into the list of its class, and the picture.
    Lat(kTile, 1, 0);
    Lat(kTile, 1, 1);
    Barrier();
    Run(kTile, tx, ty, 2);
    Barrier();
    Run(kTile, Groups(Groups(width_, 8), 8), Groups(Groups(height_, 8), 8), 3);
    Barrier();
    Run(kTile, 1, 1, 4);
    step_indirect_ = true;
    Barrier(true);
    // By class: the few long groups first, because the command processor starts a dispatch only when the one before has all its waves out; no barrier between them, they write different pixels.
    for (int c = 6; c >= 0; --c) {
      Push push;
      Bind(c == 6 ? uint32_t(kRender) : uint32_t(kRenderOne) + uint32_t(c), push);
      cmd_dispatch_indirect_(cmd_, flist_.buffer(), 48 + VkDeviceSize(c) * 16);
    }
    Barrier();
    Lat(kTrust, 0);
    Barrier();
    Whole(kMix, 0);
    Barrier();
  }

  void DestroyWorking() {
    initialized_ = false;
    steps_.clear();
    step_next_ = 0;
    have_surfaces_ = search_fresh_ = in_layers_ = step_indirect_ = arrays_ready_ = have_field_ = false;
    prep_valid_ = prep_reuse_ = false;
    prep_a_ = 0;
    prep_b_ = 1;
    prep_identity_ = {};
    if (vulkan_.device == VK_NULL_HANDLE || dispatch_.get_device_proc_addr == nullptr) return;
    dense_.reset();
    for (VkPipeline& p : pipelines_) {
      if (p != VK_NULL_HANDLE) dispatch_.destroy_pipeline(vulkan_.device, p, vulkan_.allocator);
      p = VK_NULL_HANDLE;
    }
    if (layout_ != VK_NULL_HANDLE) dispatch_.destroy_pipeline_layout(vulkan_.device, layout_, vulkan_.allocator);
    if (crop_layout_ != VK_NULL_HANDLE) dispatch_.destroy_pipeline_layout(vulkan_.device, crop_layout_, vulkan_.allocator);
    if (pool_ != VK_NULL_HANDLE) dispatch_.destroy_descriptor_pool(vulkan_.device, pool_, vulkan_.allocator);
    if (crop_pool_ != VK_NULL_HANDLE) dispatch_.destroy_descriptor_pool(vulkan_.device, crop_pool_, vulkan_.allocator);
    if (set_layout_ != VK_NULL_HANDLE) dispatch_.destroy_descriptor_set_layout(vulkan_.device, set_layout_, vulkan_.allocator);
    if (crop_set_layout_ != VK_NULL_HANDLE) dispatch_.destroy_descriptor_set_layout(vulkan_.device, crop_set_layout_, vulkan_.allocator);
    layout_ = crop_layout_ = VK_NULL_HANDLE;
    pool_ = crop_pool_ = VK_NULL_HANDLE;
    set_layout_ = crop_set_layout_ = VK_NULL_HANDLE;
    sets_.clear();
    decide_sets_.clear();
    crop_sets_.clear();
    for (OwnedImage* image : {&soft_[0], &soft_[1], &half_[0], &half_[1], &quarter_[0], &quarter_[1], &base_, &fallback_, &tight_[0], &tight_[1], &guide_, &bend_}) image->Destroy(vulkan_, dispatch_);
    for (OwnedBuffer* buffer : {&params_, &search_, &cost_, &label_, &plan_, &stat_, &front_, &palpha_, &cells_, &out_, &disagree_, &counter_, &blocks_, &tiles_, &thumb_, &flist_, &snap_, &dummy_}) buffer->Destroy(vulkan_, dispatch_);
    if (sampler_ != VK_NULL_HANDLE) dispatch_.destroy_sampler(vulkan_.device, sampler_, vulkan_.allocator);
    if (cubic_sampler_ != VK_NULL_HANDLE) dispatch_.destroy_sampler(vulkan_.device, cubic_sampler_, vulkan_.allocator);
    sampler_ = cubic_sampler_ = VK_NULL_HANDLE;
  }

  CreateInfo create_info_;
  VulkanContext vulkan_;
  VulkanDispatch dispatch_;
  Capabilities capabilities_;
  PFN_vkCmdUpdateBuffer cmd_update_buffer_ = nullptr;
  PFN_vkCmdDispatchIndirect cmd_dispatch_indirect_ = nullptr;
  PFN_vkCmdClearColorImage cmd_clear_color_image_ = nullptr;
  VkCommandBuffer cmd_ = VK_NULL_HANDLE;

  bool initialized_ = false, direct_output_ = false;
  uint32_t width_ = 0, height_ = 0, step_ = 4, lw_ = 0, lh_ = 0, block_ = 8, gw_ = 0, gh_ = 0, fw_ = 0, fh_ = 0;
  float cell_w_ = 1.0f, cell_h_ = 1.0f;
  VkFormat input_format_ = VK_FORMAT_UNDEFINED, output_format_ = VK_FORMAT_UNDEFINED;

  std::unique_ptr<DenseFlowSource> dense_;
  OwnedImage soft_[2], half_[2], quarter_[2], base_, fallback_, tight_[2], guide_, bend_;
  bool hud_memory_ = false;  // the screen's memory is cleared once per size
  Image screen_truth_{};     // SetScreenTruth: the host's screen, codes as in shaders/rc7/rc7_hudgs.comp
  bool screen_now_ = false;  // this record adds it to the guess
  OwnedBuffer params_, search_, cost_, label_, plan_, stat_, front_, palpha_, cells_, out_, disagree_, counter_, blocks_, tiles_, thumb_, flist_, snap_, dummy_;
  VkSampler sampler_ = VK_NULL_HANDLE, cubic_sampler_ = VK_NULL_HANDLE;
  struct Step {
    std::function<void()> run;
    bool indirect;
    double us;  // how long it takes on the Adreno, about
  };
  std::vector<Step> steps_;
  size_t step_next_ = 0;
  double step_now_us_ = 0.0;
  bool in_layers_ = false, have_surfaces_ = false, step_indirect_ = false, long_done_ = false, search_fresh_ = false, arrays_ready_ = false, have_field_ = false;
  VkDescriptorSetLayout set_layout_ = VK_NULL_HANDLE, crop_set_layout_ = VK_NULL_HANDLE;
  VkPipelineLayout layout_ = VK_NULL_HANDLE, crop_layout_ = VK_NULL_HANDLE;
  VkDescriptorPool pool_ = VK_NULL_HANDLE, crop_pool_ = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> sets_, decide_sets_, crop_sets_;
  uint32_t slot_ = 0;
  struct PrepIdentity {
    uint64_t sequence = 0;
    uint32_t width = 0, height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    ActiveRect rect;
  } prep_identity_;
  bool prep_valid_ = false, prep_reuse_ = false;
  uint32_t prep_a_ = 0, prep_b_ = 1;
  std::array<VkPipeline, kPipeCount> pipelines_{};
};

}  // namespace

std::unique_ptr<AlgorithmContext> CreateRc7Context(const CreateInfo& create_info, Status* status) {
  auto context = std::make_unique<Rc7Context>(create_info);
  if (!context->Initialize()) {
    if (status) *status = Status::kUnsupported;
    return nullptr;
  }
  if (status) *status = Status::kSuccess;
  return context;
}

}  // namespace zerofg
