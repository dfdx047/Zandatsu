#pragma once

#include <functional>
#include <memory>

#include "zerofg/zerofg.h"

namespace zerofg {

// The HUD layer, for a host that knows which pixels are HUD (ARMSX2's ZeroGS; docs/GS_HUD_RULE_2026-11-06.md on the algorithm repository). Around any
// generator: the HUD that is the same in both Reals (and that both HUD planes explain) is taken out of A and B, the generator runs once on
// those pictures, and B's HUD goes back on exactly those pixels. A HUD that moves or changes, or a pixel a plane cannot explain, stays in
// the Reals and is interpolated as always (doubt = world); the code image tells an engine with a screen of its own (RC7) where that HUD
// is (Generate's `screen`, AlgorithmContext::SetScreenTruth).
class HudLayer {
 public:
  // `screen`: the HUD as the planes say, at the Reals' size (r8, sampled, GENERAL): 0 no HUD, 0.5 a HUD pixel left in the pictures, 1 a
  // HUD pixel taken out and put back (AlgorithmContext::SetScreenTruth).
  using Generate = std::function<Status(const Image& previous, const Image& current, const Image& output, const Image& screen)>;

  static std::unique_ptr<HudLayer> Create(const CreateInfo& create_info);
  ~HudLayer();

  Status Resize(uint32_t width, uint32_t height, VkFormat input_format, VkFormat output_format);

  // kUnsupported when the planes or the formats do not fit: the caller then generates without the layer.
  Status Record(VkCommandBuffer command_buffer, const Image& previous, const Image& current, const Interpolator::HudPlanes& previous_hud,
                const Interpolator::HudPlanes& current_hud, const Image& output, const Generate& generate);

  class Impl;

 private:
  explicit HudLayer(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

}  // namespace zerofg
