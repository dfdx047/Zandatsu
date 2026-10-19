/**
 * @file        perf/frame_rate.h
 * @brief       Guest frame rate tracking, available in every build config.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace rex::perf {

// Called once per guest swap (VdSwap) by the command processor.
void RecordGuestSwap();

// Total guest swaps since startup.
uint64_t GetGuestSwapCount();

// Times between the latest guest swaps in milliseconds, oldest first (up to
// the last 255). Returns how many were written.
size_t GetGuestFrameTimes(float* out_ms, size_t max_count);

// Guest frontbuffer size (before resolution scaling) of the latest swap.
void RecordGuestFrontbuffer(uint32_t width, uint32_t height);

// Draw resolution scale the GPU backend renders with, and the one the config
// asked for (higher when an upscaler quality mode renders below it).
void SetDrawResolutionScale(uint32_t x, uint32_t y, uint32_t requested_x, uint32_t requested_y);

struct RenderInfo {
  uint32_t frontbuffer_width = 0;
  uint32_t frontbuffer_height = 0;
  uint32_t scale_x = 0;
  uint32_t scale_y = 0;
  uint32_t requested_scale_x = 0;
  uint32_t requested_scale_y = 0;
};
RenderInfo GetRenderInfo();

}  // namespace rex::perf
