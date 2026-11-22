#include "zerofg/zerofg.h"

#include <utility>

#include "algorithm.h"
#include "hud/hud_layer.h"

namespace zerofg {

std::unique_ptr<Interpolator> Interpolator::Create(
    const CreateInfo& create_info, Status* status) {
  const auto fail = [status](Status failure) {
    if (status) {
      *status = failure;
    }
    return std::unique_ptr<Interpolator>();
  };

  if (create_info.vulkan.instance == VK_NULL_HANDLE ||
      create_info.vulkan.physical_device == VK_NULL_HANDLE ||
      create_info.vulkan.device == VK_NULL_HANDLE ||
      create_info.vulkan.get_instance_proc_addr == nullptr ||
      create_info.frame_context_count == 0 ||
      create_info.frame_context_count > 8) {
    return fail(Status::kInvalidArgument);
  }
  if (create_info.mode != Mode::kZero && create_info.mode != Mode::kReallyZero) {
    return fail(Status::kInvalidArgument);
  }

  // Auto takes the generic fast paths the device really has enabled (Modern:
  // half colour, subgroup propagation, hardware cubic); Compat runs the golden
  // forms. QCOM is the Modern backend (no QCOM-specific form measured to pay).
  Backend resolved_backend = create_info.backend;
  if (resolved_backend == Backend::kAuto) {
    resolved_backend = Backend::kModern;
  }
  if (resolved_backend != Backend::kCompat &&
      resolved_backend != Backend::kModern &&
      resolved_backend != Backend::kQcom) {
    return fail(Status::kUnsupported);
  }
  // The ZeroFG floor: an effective Vulkan 1.3 device with synchronization2.
  if (create_info.capabilities.effective_api_version < VK_API_VERSION_1_3 ||
      !create_info.capabilities.synchronization2_enabled) {
    return fail(Status::kUnsupported);
  }
  // Both engines have one context whatever the frame context index: the surfaces, the search under way and the flow's
  // starts are history, and one per index would see only some of the pairs and work from history several pairs old.
  // What must differ per index is only the descriptor sets that bind the Reals and the output: they keep one copy each.
  const bool zero = create_info.mode == Mode::kZero;
  const uint32_t context_count = 1u;
  std::vector<std::unique_ptr<AlgorithmContext>> contexts;
  contexts.reserve(context_count);
  for (uint32_t i = 0; i < context_count; ++i) {
    CreateInfo context_create_info = create_info;
    context_create_info.backend = resolved_backend;
    Status context_status = Status::kSuccess;
    std::unique_ptr<AlgorithmContext> context =
        zero ? CreateRc7Context(context_create_info, &context_status) : CreateDfContext(context_create_info, &context_status);
    if (!context) {
      return fail(context_status);
    }
    contexts.push_back(std::move(context));
  }

  if (status) {
    *status = Status::kSuccess;
  }
  CreateInfo stored = create_info;
  stored.backend = resolved_backend;
  return std::unique_ptr<Interpolator>(
      new Interpolator(std::move(contexts), create_info.frame_context_count, stored));
}

Interpolator::Interpolator(
    std::vector<std::unique_ptr<AlgorithmContext>> contexts,
    uint32_t frame_context_count, const CreateInfo& create_info)
    : contexts_(std::move(contexts)), frame_context_count_(frame_context_count), create_info_(create_info) {}

Interpolator::~Interpolator() = default;

Status Interpolator::Resize(uint32_t width, uint32_t height,
                            VkFormat input_format,
                            VkFormat output_format) {
  if (contexts_.empty()) {
    return Status::kInvalidArgument;
  }
  for (const auto& context : contexts_) {
    const Status status =
        context->Resize(width, height, input_format, output_format);
    if (status != Status::kSuccess) {
      return status;
    }
  }
  width_ = width;
  height_ = height;
  input_format_ = input_format;
  output_format_ = output_format;
  // The HUD layer follows the new size on its next use.
  if (hud_ && hud_->Resize(width, height, input_format, output_format) != Status::kSuccess) {
    hud_.reset();
    hud_failed_ = true;
  }
  return Status::kSuccess;
}

Status Interpolator::Interpolate(VkCommandBuffer command_buffer,
                                 uint32_t frame_context_index,
                                 const Image& previous,
                                 const Image& current, float phase,
                                 const Image& output) {
  if (frame_context_index >= frame_context_count_) {
    return Status::kInvalidArgument;
  }
  return contexts_[contexts_.size() == 1 ? 0 : frame_context_index]->Interpolate(
      command_buffer, previous, current, phase, output);
}

Status Interpolator::Interpolate(VkCommandBuffer command_buffer,
                                 uint32_t frame_context_index,
                                 const Image& previous,
                                 const Image& current, float phase,
                                 const Image& output,
                                 const HudPlanes& previous_hud,
                                 const HudPlanes& current_hud) {
  if (frame_context_index >= frame_context_count_) {
    return Status::kInvalidArgument;
  }
  AlgorithmContext* const context = contexts_[contexts_.size() == 1 ? 0 : frame_context_index].get();
  const auto plain = [&]() { return context->Interpolate(command_buffer, previous, current, phase, output); };
  if (previous_hud.p.image == VK_NULL_HANDLE || previous_hud.w.image == VK_NULL_HANDLE || current_hud.p.image == VK_NULL_HANDLE ||
      current_hud.w.image == VK_NULL_HANDLE || hud_failed_ || width_ == 0) {
    return plain();
  }
  if (!hud_) {
    hud_ = HudLayer::Create(create_info_);
    if (!hud_ || hud_->Resize(width_, height_, input_format_, output_format_) != Status::kSuccess) {
      hud_.reset();
      hud_failed_ = true;
      return plain();
    }
  }
  const Status status = hud_->Record(
      command_buffer, previous, current, previous_hud, current_hud, output,
      [&](const Image& a, const Image& b, const Image& out, const Image& screen) {
        // What the layer leaves in the pictures is HUD too: the engine's screen (RC7) hears it from the planes.
        context->SetScreenTruth(&screen);
        const Status s = context->Interpolate(command_buffer, a, b, phase, out);
        context->SetScreenTruth(nullptr);
        return s;
      });
  // A plane that does not fit this pair: the pair without the layer (nothing was recorded for it).
  return status == Status::kUnsupported ? plain() : status;
}

}  // namespace zerofg
