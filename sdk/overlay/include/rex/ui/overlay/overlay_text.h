/**
 * @file        rex/ui/overlay/overlay_text.h
 *
 * @brief       Text for the styled overlays (quick menu, frame rate panel):
 *              a system UI font baked at two sizes, drawn from the sharpest.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#pragma once

#include <string_view>

#include <imgui.h>

namespace rex::ui::overlay_text {

// Adds the fonts to the atlas while it's being set up. Without them (no
// system font found) text falls back to the default ImGui font.
void AddFonts(ImFontAtlas* atlas);

// Physical pixels per ImGui display unit (the window's DPI scale), to pick the
// baked size that gives the sharpest text.
void SetPixelScale(float scale);

ImFont* Font(float size);
ImVec2 Measure(float size, std::string_view text);
void Draw(ImDrawList* draw_list, float size, ImVec2 position, ImU32 color, std::string_view text,
          float wrap_width = 0.0f);

}  // namespace rex::ui::overlay_text
