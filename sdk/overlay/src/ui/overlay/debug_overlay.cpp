/**
 * @file        ui/overlay/debug_overlay.cpp
 *
 * @brief       Debug overlay implementation. See debug_overlay.h for details.
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#include <rex/ui/overlay/debug_overlay.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <rex/cvar.h>
#include <rex/perf/frame_rate.h>
#include <rex/ui/overlay/overlay_text.h>
#include <rex/version.h>
#include <imgui.h>
#ifdef REXGLUE_ENABLE_PERF_COUNTERS
#include <rex/perf/counter.h>
#include <cinttypes>
#endif

REXCVAR_DEFINE_STRING(debug_overlay_position, "top-left", "UI",
                      "Corner of the frame rate overlay: top-left, top-right, bottom-left, "
                      "bottom-right")
    .allowed({"top-left", "top-right", "bottom-left", "bottom-right"});

namespace rex::ui {

namespace {

// The upscaler shown in the overlay, from the presenter settings.
std::string UpscalerText() {
  if (!rex::cvar::GetFlagInfo("present_effect")) {
    return {};
  }
  const std::string effect = rex::cvar::GetFlagByName("present_effect");
  if (effect == "cas") {
    return "AMD CAS";
  }
  std::string text;
  if (effect == "fsr") {
    text = "AMD FSR 1";
  } else if (effect == "fsr2") {
    text = "AMD FSR 2";
  } else if (effect == "fsr3") {
    text = "AMD FSR 3";
  } else if (effect == "dlss") {
    text = "NVIDIA DLSS";
  } else {
    return "Off";
  }
  static const std::pair<const char*, const char*> kQualityModes[] = {
      {"auto", "Native"},
      {"nativeaa", "Native AA"},
      {"quality", "Quality"},
      {"balanced", "Balanced"},
      {"performance", "Performance"},
      {"ultra_performance", "Ultra Perf."},
  };
  const std::string mode = rex::cvar::GetFlagByName("present_fsr_quality_mode");
  for (const auto& [value, name] : kQualityModes) {
    if (mode == value) {
      text += " \xC2\xB7 ";
      text += name;
      break;
    }
  }
  return text;
}

}  // namespace

DebugOverlayDialog::DebugOverlayDialog(ImGuiDrawer* imgui_drawer, FrameStatsProvider stats_provider)
    : ImGuiDialog(imgui_drawer), stats_provider_(std::move(stats_provider)) {}

DebugOverlayDialog::~DebugOverlayDialog() {}

void DebugOverlayDialog::OnDraw(ImGuiIO& io) {
  // Game FPS: guest swaps (VdSwap) per second, sampled every 0.5 s.
  constexpr double kFpsWindowSeconds = 0.5;
  const auto now = std::chrono::steady_clock::now();
  const uint64_t swaps = rex::perf::GetGuestSwapCount();
  if (fps_window_start_ == std::chrono::steady_clock::time_point{}) {
    fps_window_start_ = now;
    fps_window_swaps_ = swaps;
  }
  const double elapsed = std::chrono::duration<double>(now - fps_window_start_).count();
  if (elapsed >= kFpsWindowSeconds) {
    guest_fps_ = static_cast<double>(swaps - fps_window_swaps_) / elapsed;
    fps_window_start_ = now;
    fps_window_swaps_ = swaps;
  }

  // Frame times of the latest guest frames, for the graph.
  std::array<float, kGraphFrames> frame_times{};
  const size_t frame_count = rex::perf::GetGuestFrameTimes(frame_times.data(), kGraphFrames);
  float frame_time_ms = 0.0f;
  if (frame_count) {
    const size_t average_frames = std::min<size_t>(frame_count, 30);
    for (size_t i = frame_count - average_frames; i < frame_count; ++i) {
      frame_time_ms += frame_times[i];
    }
    frame_time_ms /= float(average_frames);
  }

  // Details: what the game renders at and how it's upscaled.
  std::vector<std::pair<std::string, std::string>> rows;
  char text[96];
  const rex::perf::RenderInfo render = rex::perf::GetRenderInfo();
  if (render.scale_x && render.frontbuffer_width) {
    const uint32_t render_width = render.frontbuffer_width * render.scale_x;
    const uint32_t render_height = render.frontbuffer_height * render.scale_y;
    const uint32_t output_width =
        render.frontbuffer_width * std::max(render.requested_scale_x, render.scale_x);
    const uint32_t output_height =
        render.frontbuffer_height * std::max(render.requested_scale_y, render.scale_y);
    std::snprintf(text, sizeof(text), "%ux%u", render_width, render_height);
    rows.emplace_back("Render", text);
    if (output_width != render_width || output_height != render_height) {
      std::snprintf(text, sizeof(text), "%ux%u", output_width, output_height);
      rows.emplace_back("Output", text);
    }
  }
  if (std::string upscaler = UpscalerText(); !upscaler.empty()) {
    rows.emplace_back("Upscaler", std::move(upscaler));
  }
  std::snprintf(text, sizeof(text), "%.0f FPS", io.Framerate);
  rows.emplace_back("Display", text);
  if (stats_provider_) {
    const FrameStats stats = stats_provider_();
    if (stats.frame_count > 0) {
      std::snprintf(text, sizeof(text), "%.1f FPS (%.2f ms)", stats.fps, stats.frame_time_ms);
      rows.emplace_back("Guest", text);
    }
  }

  // Layout, in 1080p units scaled to the display. Drawn behind all windows and
  // without one, so it never takes the mouse from the game or the menus.
  const ImVec2 display = io.DisplaySize;
  const float u = std::max(0.4f, std::min(display.x / 1920.0f, display.y / 1080.0f));
  const float padding = 16.0f * u;
  const float width = 300.0f * u;
  const float header_height = 50.0f * u;
  const float graph_height = 42.0f * u;
  const float row_height = 26.0f * u;
  const float height = padding + header_height + 8.0f * u + graph_height + 10.0f * u +
                       float(rows.size()) * row_height + padding - 4.0f * u;
  const float margin = 16.0f * u;
  const std::string position = REXCVAR_GET(debug_overlay_position);
  const bool right = position == "top-right" || position == "bottom-right";
  const bool bottom = position == "bottom-left" || position == "bottom-right";
  const ImVec2 panel_min(right ? display.x - margin - width : margin,
                         bottom ? display.y - margin - height : margin);
  const ImVec2 panel_max(panel_min.x + width, panel_min.y + height);
  const float left = panel_min.x + padding;
  const float content_right = panel_max.x - padding;

  ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
  draw_list->AddRectFilled(panel_min, panel_max, IM_COL32(17, 19, 24, 220), 14.0f * u);
  draw_list->AddRect(panel_min, panel_max, IM_COL32(255, 255, 255, 40), 14.0f * u, 0, 1.5f * u);

  // FPS and frame time.
  float y = panel_min.y + padding;
  {
    const float fps_size = 46.0f * u;
    if (guest_fps_ > 0.0) {
      std::snprintf(text, sizeof(text), "%.0f", guest_fps_);
    } else {
      std::snprintf(text, sizeof(text), "--");
    }
    const ImVec2 fps_extent = overlay_text::Measure(fps_size, text);
    overlay_text::Draw(draw_list, fps_size, ImVec2(left, y - 6.0f * u), IM_COL32(255, 255, 255, 255),
                       text);
    const float label_size = 18.0f * u;
    overlay_text::Draw(draw_list, label_size,
                       ImVec2(left + fps_extent.x + 8.0f * u, y + fps_extent.y - 34.0f * u),
                       IM_COL32(245, 140, 28, 255), "FPS");
    if (frame_time_ms > 0.0f) {
      std::snprintf(text, sizeof(text), "%.1f ms", frame_time_ms);
      const float time_size = 22.0f * u;
      const ImVec2 time_extent = overlay_text::Measure(time_size, text);
      overlay_text::Draw(draw_list, time_size,
                         ImVec2(content_right - time_extent.x, y + 12.0f * u),
                         IM_COL32(120, 176, 255, 255), text);
      const char* caption = "FRAME TIME";
      const float caption_size = 13.0f * u;
      overlay_text::Draw(
          draw_list, caption_size,
          ImVec2(content_right - overlay_text::Measure(caption_size, caption).x, y - 2.0f * u),
          IM_COL32(120, 132, 150, 255), caption);
    }
  }
  y += header_height + 8.0f * u;

  // Frame time graph: up is slower. The line marks 60 FPS.
  {
    const ImVec2 graph_min(left, y);
    const ImVec2 graph_max(content_right, y + graph_height);
    draw_list->AddRectFilled(graph_min, graph_max, IM_COL32(255, 255, 255, 12), 6.0f * u);
    float scale_ms = 1000.0f / 30.0f;
    for (size_t i = 0; i < frame_count; ++i) {
      scale_ms = std::max(scale_ms, std::min(frame_times[i] * 1.1f, 100.0f));
    }
    auto graph_y = [&](float ms) {
      return graph_max.y - 3.0f * u - (graph_height - 6.0f * u) * std::min(ms / scale_ms, 1.0f);
    };
    const float line_y = graph_y(1000.0f / 60.0f);
    draw_list->AddLine(ImVec2(graph_min.x + 4.0f * u, line_y), ImVec2(graph_max.x - 4.0f * u, line_y),
                       IM_COL32(255, 255, 255, 34), 1.0f * u);
    if (frame_count >= 2) {
      std::array<ImVec2, kGraphFrames> points;
      const float step = (graph_max.x - graph_min.x - 8.0f * u) / float(kGraphFrames - 1);
      const float start_x = graph_max.x - 4.0f * u - step * float(frame_count - 1);
      for (size_t i = 0; i < frame_count; ++i) {
        points[i] = ImVec2(start_x + step * float(i), graph_y(frame_times[i]));
      }
      draw_list->AddPolyline(points.data(), int(frame_count), IM_COL32(245, 140, 28, 255), 0,
                             2.0f * u);
    }
  }
  y += graph_height + 10.0f * u;

  // Details.
  const float row_size = 18.0f * u;
  for (const auto& [label, value] : rows) {
    overlay_text::Draw(draw_list, row_size, ImVec2(left, y), IM_COL32(150, 160, 176, 255), label);
    const ImVec2 value_extent = overlay_text::Measure(row_size, value);
    overlay_text::Draw(draw_list, row_size, ImVec2(content_right - value_extent.x, y),
                       IM_COL32(236, 240, 246, 255), value);
    y += row_height;
  }

#ifdef REXGLUE_ENABLE_PERF_COUNTERS
  ImGui::SetNextWindowPos(ImVec2(10, panel_max.y + 10), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(280, 200), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(0.5f);
  if (ImGui::Begin("Perf counters##overlay", nullptr, ImGuiWindowFlags_NoCollapse)) {
    // Frame time graph
    auto ft_us = rex::perf::GetSnapshotCounter(rex::perf::CounterId::kFrameTimeUs);
    float ft_ms = static_cast<float>(ft_us) / 1000.0f;
    frame_time_history_[frame_history_idx_] = ft_ms;
    frame_history_idx_ = (frame_history_idx_ + 1) % kFrameHistorySize;
    ImGui::PlotLines("##ft", frame_time_history_.data(), kFrameHistorySize,
                     static_cast<int>(frame_history_idx_), "Frame (ms)", 0.0f, 50.0f,
                     ImVec2(200, 40));

    // GPU
    ImGui::Text("Draw: %" PRId64 "  Stalls: %" PRId64 "  Verts: %" PRId64,
                rex::perf::GetSnapshotCounter(rex::perf::CounterId::kDrawCalls),
                rex::perf::GetSnapshotCounter(rex::perf::CounterId::kCommandBufferStalls),
                rex::perf::GetSnapshotCounter(rex::perf::CounterId::kVerticesProcessed));

    // Audio
    ImGui::Text("XMA: %" PRId64 "  Lat: %.1fms  BufQ: %" PRId64,
                rex::perf::GetSnapshotCounter(rex::perf::CounterId::kXmaFramesDecoded),
                static_cast<float>(
                    rex::perf::GetSnapshotCounter(rex::perf::CounterId::kAudioFrameLatencyUs)) /
                    1000.0f,
                rex::perf::GetSnapshotCounter(rex::perf::CounterId::kBufferQueueDepth));

    // Dispatch
    ImGui::Text("Dispatch: %" PRId64 "  IRQ: %" PRId64,
                rex::perf::GetSnapshotCounter(rex::perf::CounterId::kFunctionsDispatched),
                rex::perf::GetSnapshotCounter(rex::perf::CounterId::kInterruptDispatches));

    // Threading
    ImGui::Text("Threads: %" PRId64 "  APC: %" PRId64 "  Contention: %" PRId64,
                rex::perf::GetSnapshotCounter(rex::perf::CounterId::kActiveThreads),
                rex::perf::GetSnapshotCounter(rex::perf::CounterId::kApcQueueDepth),
                rex::perf::GetSnapshotCounter(rex::perf::CounterId::kCriticalRegionContentions));

    // Caches
    auto tex_h = rex::perf::GetSnapshotCounter(rex::perf::CounterId::kTextureCacheHits);
    auto tex_m = rex::perf::GetSnapshotCounter(rex::perf::CounterId::kTextureCacheMisses);
    auto pip_h = rex::perf::GetSnapshotCounter(rex::perf::CounterId::kPipelineCacheHits);
    auto pip_m = rex::perf::GetSnapshotCounter(rex::perf::CounterId::kPipelineCacheMisses);
    ImGui::Text("TexCache: %" PRId64 "/%" PRId64 "  PipeCache: %" PRId64 "/%" PRId64, tex_h,
                tex_h + tex_m, pip_h, pip_h + pip_m);
  }
  ImGui::End();
#endif

  // Build stamp watermark -- centered near bottom of screen
  auto text_size = ImGui::CalcTextSize(REXGLUE_BUILD_STAMP);
  float watermark_padding = ImGui::GetStyle().WindowPadding.x * 2.0f;
  float bottom_offset = io.DisplaySize.y * 0.03f;
  ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - text_size.x - watermark_padding) * 0.5f,
                                 io.DisplaySize.y - text_size.y - bottom_offset));
  ImGui::SetNextWindowSize(ImVec2(0, 0));
  ImGui::PushStyleColor(ImGuiCol_Text, imgui_drawer()->style().debug.muted_text);
  if (ImGui::Begin("##watermark", nullptr,
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                       ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(REXGLUE_BUILD_STAMP);
  }
  ImGui::End();
  ImGui::PopStyleColor();
}

}  // namespace rex::ui
