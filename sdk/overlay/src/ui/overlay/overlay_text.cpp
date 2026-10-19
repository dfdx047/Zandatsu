/**
 * @file        ui/overlay/overlay_text.cpp
 *
 * @brief       Overlay text. See overlay_text.h for details.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#include <rex/ui/overlay/overlay_text.h>

#include <atomic>
#include <cfloat>
#include <filesystem>
#include <system_error>

#include <rex/platform.h>

namespace rex::ui::overlay_text {

namespace {

// The renderer can't bake glyphs on demand, so text is scaled from the closest
// of these sizes.
constexpr float kSizeSmall = 36.0f;
constexpr float kSizeLarge = 72.0f;
ImFont* font_small = nullptr;
ImFont* font_large = nullptr;
std::atomic<float> pixel_scale{1.0f};

}  // namespace

void AddFonts(ImFontAtlas* atlas) {
  if (!atlas || font_small) {
    return;
  }
  static const ImWchar kGlyphRanges[] = {0x0020, 0x00FF, 0};
  static const char* const kFontPaths[] = {
#if REX_PLATFORM_WIN32
      "C:\\Windows\\Fonts\\segoeuib.ttf",
      "C:\\Windows\\Fonts\\seguisb.ttf",
      "C:\\Windows\\Fonts\\arialbd.ttf",
#else
      "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
      "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
      "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
#endif
  };
  for (const char* path : kFontPaths) {
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
      continue;
    }
    ImFontConfig config;
    font_small = atlas->AddFontFromFileTTF(path, kSizeSmall, &config, kGlyphRanges);
    font_large = atlas->AddFontFromFileTTF(path, kSizeLarge, &config, kGlyphRanges);
    break;
  }
}

void SetPixelScale(float scale) {
  pixel_scale.store(scale > 0.1f ? scale : 1.0f, std::memory_order_relaxed);
}

ImFont* Font(float size) {
  if (font_large && size * pixel_scale.load(std::memory_order_relaxed) > kSizeSmall * 1.3f) {
    return font_large;
  }
  return font_small ? font_small : ImGui::GetFont();
}

ImVec2 Measure(float size, std::string_view text) {
  return Font(size)->CalcTextSizeA(size, FLT_MAX, 0.0f, text.data(), text.data() + text.size());
}

void Draw(ImDrawList* draw_list, float size, ImVec2 position, ImU32 color, std::string_view text,
          float wrap_width) {
  draw_list->AddText(Font(size), size, position, color, text.data(), text.data() + text.size(),
                     wrap_width);
}

}  // namespace rex::ui::overlay_text
