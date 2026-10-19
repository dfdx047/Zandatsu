/**
 * @file        ui/overlay/quick_menu.cpp
 *
 * @brief       Quick settings menu. See quick_menu.h for details.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#include <rex/ui/overlay/quick_menu.h>

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string_view>

#include <imgui.h>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/perf/frame_rate.h>
#include <rex/string/numeric.h>
#include <rex/ui/overlay/overlay_text.h>

namespace rex::ui {

namespace {

// X_INPUT_GAMEPAD_* (rex/input/input.h).
constexpr uint16_t kPadUp = 0x0001;
constexpr uint16_t kPadDown = 0x0002;
constexpr uint16_t kPadLeft = 0x0004;
constexpr uint16_t kPadRight = 0x0008;
constexpr uint16_t kPadStart = 0x0010;
constexpr uint16_t kPadBack = 0x0020;
constexpr uint16_t kPadLeftShoulder = 0x0100;
constexpr uint16_t kPadRightShoulder = 0x0200;
constexpr uint16_t kPadA = 0x1000;
constexpr uint16_t kPadB = 0x2000;
constexpr uint16_t kPadY = 0x8000;
constexpr int16_t kStickThreshold = 16000;

// Seconds before a held direction repeats, and between the repeats.
constexpr double kRepeatDelay = 0.4;
constexpr double kRepeatIntervalVertical = 0.09;
constexpr double kRepeatIntervalHorizontal = 0.12;

ImVec2 TextSize(float size, std::string_view text) {
  return overlay_text::Measure(size, text);
}
void DrawText(ImDrawList* draw_list, float size, ImVec2 position, ImU32 color,
              std::string_view text, float wrap_width = 0.0f) {
  overlay_text::Draw(draw_list, size, position, color, text, wrap_width);
}

// Colors faded in with the menu.
struct Palette {
  float alpha = 1.0f;
  ImU32 operator()(int r, int g, int b, int a = 255) const {
    return IM_COL32(r, g, b, int(float(a) * alpha));
  }
};

bool IsOn(const std::string& value) {
  return rex::string::from_string<bool>(value, false);
}

bool SameValue(const std::string& a, const std::string& b) {
  if (a == b) {
    return true;
  }
  double number_a, number_b;
  return rex::cvar::ParseDouble(a, number_a) && rex::cvar::ParseDouble(b, number_b) &&
         number_a == number_b;
}

using Choice = std::pair<std::string, std::string>;

// The item's choices that its cvar accepts.
std::vector<const Choice*> AllowedChoices(const QuickMenuItem& item) {
  std::vector<const Choice*> allowed;
  const rex::cvar::FlagEntry* info = rex::cvar::GetFlagInfo(item.cvar);
  if (!info) {
    return allowed;
  }
  const rex::cvar::Constraints& constraints = info->constraints;
  for (const Choice& choice : item.choices) {
    if (constraints.HasAllowedValues() &&
        std::find(constraints.allowed_values.begin(), constraints.allowed_values.end(),
                  choice.first) == constraints.allowed_values.end()) {
      continue;
    }
    double number;
    if (constraints.HasRangeConstraint() && rex::cvar::ParseDouble(choice.first, number) &&
        ((constraints.min && number < *constraints.min) ||
         (constraints.max && number > *constraints.max))) {
      continue;
    }
    allowed.push_back(&choice);
  }
  return allowed;
}

int ChoiceIndex(const std::vector<const Choice*>& choices, const std::string& value) {
  for (size_t i = 0; i < choices.size(); ++i) {
    if (SameValue(choices[i]->first, value)) {
      return int(i);
    }
  }
  return -1;
}

double NumberValue(const QuickMenuItem& item, const std::string& value) {
  double number = item.min;
  rex::cvar::ParseDouble(value, number);
  return number;
}

std::string FormatNumber(const QuickMenuItem& item, double value) {
  char text[64];
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
  std::snprintf(text, sizeof(text), item.format.c_str(),
                value * item.display_scale + item.display_offset);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  return text;
}

// The text of a number for the cvar, by its type.
std::string CvarNumber(const std::string& cvar, double value) {
  const rex::cvar::FlagEntry* info = rex::cvar::GetFlagInfo(cvar);
  if (info && info->type != rex::cvar::FlagType::Double) {
    return std::to_string(std::llround(value));
  }
  char text[32];
  std::snprintf(text, sizeof(text), "%.6g", value);
  return text;
}

// "Name|TAG" and "TITLE<newline>note": the extra bits ride in the strings (see quick_menu.h).
struct TextParts {
  std::string_view head;
  std::string_view tail;
};
TextParts SplitAt(std::string_view text, char separator, bool last) {
  const size_t at = last ? text.rfind(separator) : text.find(separator);
  if (at == std::string_view::npos) {
    return {text, {}};
  }
  return {text.substr(0, at), text.substr(at + 1)};
}

bool IsBetaTag(std::string_view tag) {
  constexpr std::string_view kBeta = "beta";
  return tag.size() == kBeta.size() &&
         std::equal(tag.begin(), tag.end(), kBeta.begin(),
                    [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; });
}

// The size (at most `size`, at least `min_size`) at which the text fits `max_width`.
float FitSize(float size, std::string_view text, float max_width, float min_size) {
  const float width = TextSize(size, text).x;
  if (width <= max_width || width <= 0.0f) {
    return size;
  }
  return std::max(min_size, size * max_width / width);
}

float WrappedHeight(float size, std::string_view text, float wrap_width) {
  return overlay_text::Font(size)
      ->CalcTextSizeA(size, FLT_MAX, wrap_width, text.data(), text.data() + text.size())
      .y;
}

// A small rounded label (BETA, EXPERIMENTAL, RESTART). Returns its right edge.
float PillWidth(float u, std::string_view text) {
  return TextSize(17.5f * u, text).x + 22.0f * u;
}
float DrawPill(ImDrawList* draw_list, float u, float x, float center_y, std::string_view text,
               ImU32 fill, ImU32 text_color) {
  const float size = 17.5f * u;
  const ImVec2 extent = TextSize(size, text);
  const float width = extent.x + 22.0f * u, height = 28.0f * u;
  draw_list->AddRectFilled(ImVec2(x, center_y - height * 0.5f),
                           ImVec2(x + width, center_y + height * 0.5f), fill, height * 0.5f);
  DrawText(draw_list, size, ImVec2(x + 11.0f * u, center_y - extent.y * 0.5f), text_color, text);
  return x + width;
}

std::atomic<int> open_menus{0};

}  // namespace

struct QuickMenuDialog::SharedPad {
  PadState state;
  bool valid = false;
  bool pending = false;
};

QuickMenuDialog::QuickMenuDialog(ImGuiDrawer* imgui_drawer, QuickMenuConfig config,
                                 Callbacks callbacks)
    : ImGuiDialog(imgui_drawer),
      config_(std::move(config)),
      callbacks_(std::move(callbacks)),
      pad_(std::make_shared<SharedPad>()) {
  if (!config_.sections.empty()) {
    std::vector<size_t> shown = ShownItems(config_.sections.front());
    if (!shown.empty()) {
      selected_item_ = shown.front();
    }
  }
  open_menus.fetch_add(1, std::memory_order_relaxed);
}

QuickMenuDialog::~QuickMenuDialog() {
  open_menus.fetch_sub(1, std::memory_order_relaxed);
}

bool QuickMenuDialog::IsOpen() {
  return open_menus.load(std::memory_order_relaxed) > 0;
}

bool QuickMenuDialog::IsShown(const QuickMenuItem& item) const {
  if (item.kind == QuickMenuItem::Kind::kAction) {
    return bool(item.action);
  }
  if (!rex::cvar::GetFlagInfo(item.cvar)) {
    return false;
  }
  if (item.kind == QuickMenuItem::Kind::kChoice && AllowedChoices(item).empty()) {
    return false;
  }
  if (!item.shown_if_cvar.empty()) {
    if (!rex::cvar::GetFlagInfo(item.shown_if_cvar)) {
      return false;
    }
    const std::string value = rex::cvar::GetFlagByName(item.shown_if_cvar);
    return std::any_of(item.shown_if_values.begin(), item.shown_if_values.end(),
                       [&](const std::string& shown_value) { return SameValue(shown_value, value); });
  }
  return true;
}

std::vector<size_t> QuickMenuDialog::ShownItems(const QuickMenuSection& section) const {
  std::vector<size_t> shown;
  for (size_t i = 0; i < section.items.size(); ++i) {
    if (IsShown(section.items[i])) {
      shown.push_back(i);
    }
  }
  return shown;
}

void QuickMenuDialog::Change(const QuickMenuItem& item, int direction) {
  if (item.kind == QuickMenuItem::Kind::kAction) {
    // Only A / click runs it (left / right just pass over it).
    if (!direction && item.action && callbacks_.defer) {
      callbacks_.defer(item.action);
      RequestClose();
    }
    return;
  }
  const std::string current = rex::cvar::GetFlagByName(item.cvar);
  switch (item.kind) {
    case QuickMenuItem::Kind::kToggle: {
      const bool on = IsOn(current);
      const bool target = direction ? direction > 0 : !on;
      if (target != on) {
        Set(item, target ? "true" : "false");
      }
    } break;
    case QuickMenuItem::Kind::kChoice: {
      const std::vector<const Choice*> choices = AllowedChoices(item);
      if (choices.empty()) {
        break;
      }
      const int count = int(choices.size());
      const int index = ChoiceIndex(choices, current);
      int target;
      if (!direction) {
        target = (index + 1) % count;
      } else if (index < 0) {
        target = direction > 0 ? 0 : count - 1;
      } else {
        target = std::clamp(index + direction, 0, count - 1);
      }
      if (target != index) {
        Set(item, choices[target]->first);
      }
    } break;
    case QuickMenuItem::Kind::kNumber: {
      const double value = NumberValue(item, current);
      // Right raises what's shown.
      const double step = item.display_scale < 0.0 ? -item.step : item.step;
      double target = value + (direction ? step * direction : step);
      if (!direction && (target > item.max + 1e-9 || target < item.min - 1e-9)) {
        // Activating past the end starts over.
        target = step > 0.0 ? item.min : item.max;
      }
      if (item.step > 0.0) {
        target = item.min + std::round((target - item.min) / item.step) * item.step;
      }
      target = std::clamp(target, item.min, item.max);
      if (std::abs(target - value) > 1e-9) {
        Set(item, CvarNumber(item.cvar, target));
      }
    } break;
  }
}

void QuickMenuDialog::Set(const QuickMenuItem& item, const std::string& value) {
  std::vector<std::string> names{item.cvar};
  for (const std::string& name : item.mirrored_cvars) {
    if (rex::cvar::GetFlagInfo(name)) {
      names.push_back(name);
    }
  }
  // Not while painting: change callbacks may touch the presenter or window.
  auto apply = [names = std::move(names), value, changed = callbacks_.changed]() {
    for (const std::string& name : names) {
      rex::cvar::SetFlagByName(name, value);
    }
    REXLOG_INFO("Settings menu: {} = {}", names.front(), value);
    if (changed) {
      changed();
    }
  };
  if (callbacks_.defer) {
    callbacks_.defer(std::move(apply));
  }
}

void QuickMenuDialog::RequestClose() {
  if (close_requested_ || !callbacks_.close || !callbacks_.defer) {
    return;
  }
  close_requested_ = true;
  callbacks_.defer(callbacks_.close);
}

void QuickMenuDialog::OnDraw(ImGuiIO& io) {
  const double now = ImGui::GetTime();
  if (open_time_ < 0.0) {
    open_time_ = now;
  }
  if (config_.sections.empty()) {
    RequestClose();
    return;
  }

  // The controllers are read after the paint (reading them may pump window
  // events, which mustn't happen in the middle of it), for the next frame.
  if (callbacks_.read_pad && callbacks_.defer && !pad_->pending) {
    pad_->pending = true;
    callbacks_.defer([pad = pad_, read_pad = callbacks_.read_pad]() {
      pad->state = read_pad();
      pad->valid = true;
      pad->pending = false;
    });
  }

  const uint64_t swaps = rex::perf::GetGuestSwapCount();
  if (fps_time_ < 0.0) {
    fps_time_ = now;
    fps_swaps_ = swaps;
  } else if (now - fps_time_ >= 0.5) {
    fps_ = float(double(swaps - fps_swaps_) / (now - fps_time_));
    fps_time_ = now;
    fps_swaps_ = swaps;
  }

  // Input.
  int move = 0;
  int change = 0;
  int section_step = 0;
  bool activate = false;
  bool close = false;

  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
    move = -1;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
    move = 1;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
    change = -1;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
    change = 1;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_PageUp, false)) {
    section_step = -1;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_PageDown, false)) {
    section_step = 1;
  }
  // With keyboard play on, Enter is the Start button of the emulated pad
  // (Space is A).
  if ((ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
       ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) &&
      !rex::cvar::Query<bool>("mnk_mode")) {
    activate = true;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    close = true;
  }

  if (pad_->valid) {
    const PadState& pad = pad_->state;
    uint16_t buttons = pad.buttons;
    if (pad.thumb_ly > kStickThreshold) {
      buttons |= kPadUp;
    } else if (pad.thumb_ly < -kStickThreshold) {
      buttons |= kPadDown;
    }
    if (pad.thumb_lx > kStickThreshold) {
      buttons |= kPadRight;
    } else if (pad.thumb_lx < -kStickThreshold) {
      buttons |= kPadLeft;
    }
    int direction = 0;
    if (buttons & kPadUp) {
      direction = 1;
    } else if (buttons & kPadDown) {
      direction = 2;
    } else if (buttons & kPadLeft) {
      direction = 3;
    } else if (buttons & kPadRight) {
      direction = 4;
    }
    if (!pad_seen_) {
      // Whatever is held as the menu opens (like the buttons that opened it)
      // is not a press in the menu.
      pad_seen_ = true;
      last_buttons_ = buttons;
      repeat_direction_ = direction;
      repeat_time_ = DBL_MAX;
    }
    const uint16_t pressed = buttons & ~last_buttons_;
    last_buttons_ = buttons;
    if (pressed & kPadA) {
      activate = true;
    }
    if (pressed & kPadB) {
      close_buttons_ |= kPadB;
    }
    if ((pressed & (kPadBack | kPadStart)) &&
        (buttons & (kPadBack | kPadStart)) == (kPadBack | kPadStart)) {
      close_buttons_ |= kPadBack | kPadStart;
    }
    if ((pressed & kPadY) && !config_.quick_toggle_cvar.empty()) {
      close_buttons_ |= kPadY;
      quick_toggle_ = true;
    }
    // Closed once they're let go: the game takes the controllers back right
    // after, and a button still held then would reach it as a press.
    if (close_buttons_ && !(buttons & close_buttons_)) {
      close = true;
    }
    if (pressed & kPadLeftShoulder) {
      section_step = -1;
    }
    if (pressed & kPadRightShoulder) {
      section_step = 1;
    }
    bool fire = false;
    if (direction != repeat_direction_) {
      repeat_direction_ = direction;
      repeat_time_ = now + kRepeatDelay;
      fire = direction != 0;
    } else if (direction && now >= repeat_time_) {
      repeat_time_ = now + (direction <= 2 ? kRepeatIntervalVertical : kRepeatIntervalHorizontal);
      fire = true;
    }
    if (fire) {
      switch (direction) {
        case 1:
          move = -1;
          break;
        case 2:
          move = 1;
          break;
        case 3:
          change = -1;
          break;
        case 4:
          change = 1;
          break;
      }
    }
  }

  // Sections with something to show (one hidden by missing cvars has no tab).
  std::vector<size_t> tabs;
  for (size_t i = 0; i < config_.sections.size(); ++i) {
    if (!ShownItems(config_.sections[i]).empty()) {
      tabs.push_back(i);
    }
  }
  if (tabs.empty()) {
    for (size_t i = 0; i < config_.sections.size(); ++i) {
      tabs.push_back(i);
    }
  }
  if (std::find(tabs.begin(), tabs.end(), section_) == tabs.end()) {
    section_ = tabs.front();
  }
  if (section_step) {
    const int tab_count = int(tabs.size());
    const int tab_at = int(std::find(tabs.begin(), tabs.end(), section_) - tabs.begin());
    section_ = tabs[size_t((tab_at + section_step + tab_count) % tab_count)];
    std::vector<size_t> shown = ShownItems(config_.sections[section_]);
    selected_item_ = shown.empty() ? 0 : shown.front();
  }
  const QuickMenuSection& section = config_.sections[section_];
  const std::vector<size_t> rows = ShownItems(section);
  if (!rows.empty() && std::find(rows.begin(), rows.end(), selected_item_) == rows.end()) {
    // The selected item got hidden - go to the next one.
    auto next = std::lower_bound(rows.begin(), rows.end(), selected_item_);
    selected_item_ = next != rows.end() ? *next : rows.back();
  }
  if (move && !rows.empty()) {
    const int row_count = int(rows.size());
    const int row = int(std::find(rows.begin(), rows.end(), selected_item_) - rows.begin());
    selected_item_ = rows[size_t((row + move + row_count) % row_count)];
  }
  if (!rows.empty()) {
    const QuickMenuItem& item = section.items[selected_item_];
    if (change) {
      Change(item, change);
    } else if (activate) {
      Change(item, 0);
    }
  }
  if (close) {
    if (quick_toggle_ && !close_requested_) {
      QuickMenuItem quick_toggle;
      quick_toggle.cvar = config_.quick_toggle_cvar;
      if (rex::cvar::GetFlagInfo(quick_toggle.cvar)) {
        Change(quick_toggle, 0);
      }
    }
    quick_toggle_ = false;
    RequestClose();
  }

  // Layout, in 1080p units scaled to the display: the sections on the left, the
  // selected section's settings on the right with a help box under them, and the
  // button hints along the bottom.
  const ImVec2 display = io.DisplaySize;
  // Room for the longest section, so the panel doesn't jump between sections.
  // Items sharing a label are alternatives shown one at a time.
  size_t max_rows = 1;
  for (const QuickMenuSection& menu_section : config_.sections) {
    std::vector<std::string_view> labels;
    for (const QuickMenuItem& menu_item : menu_section.items) {
      if (std::find(labels.begin(), labels.end(), menu_item.label) == labels.end()) {
        labels.push_back(menu_item.label);
      }
    }
    max_rows = std::max(max_rows, labels.size());
  }
  constexpr float kPanelWidth = 1180.0f, kTabsWidth = 276.0f, kPadding = 32.0f, kGap = 16.0f;
  constexpr float kHeaderHeight = 104.0f, kSectionHeaderHeight = 84.0f, kRowHeight = 60.0f;
  constexpr float kMinHelpHeight = 136.0f, kHintsHeight = 72.0f;
  constexpr float kTabsHeaderHeight = 48.0f, kTabHeight = 58.0f;
  const float rows_height = float(max_rows) * kRowHeight;
  const float tabs_height = kTabsHeaderHeight + float(tabs.size()) * kTabHeight;
  const float body_height =
      std::max(kSectionHeaderHeight + rows_height + kGap + kMinHelpHeight, tabs_height);
  const float help_height = body_height - kSectionHeaderHeight - rows_height - kGap;
  const float base_height = kHeaderHeight + kGap + body_height + kGap + kHintsHeight;
  float u = std::max(0.4f, std::min(display.x / 1920.0f, display.y / 1080.0f));
  u = std::min(u, display.y * 0.94f / base_height);

  const float padding = kPadding * u;
  const float panel_width = std::min(kPanelWidth * u, display.x - 2.0f * padding);
  const float panel_height = base_height * u;
  // Left of the center, so the game stays visible while trying the settings.
  const ImVec2 panel_min(std::max(padding, display.x * 0.04f),
                         std::floor((display.y - panel_height) * 0.5f));
  const ImVec2 panel_max(panel_min.x + panel_width, panel_min.y + panel_height);
  const float tabs_left = panel_min.x + padding;
  const float tabs_right = tabs_left + std::min(kTabsWidth * u, panel_width * 0.28f);
  const float content_left = tabs_right + 36.0f * u;
  const float content_right = panel_max.x - padding;
  const float body_top = panel_min.y + (kHeaderHeight + kGap) * u;
  const float rows_top = body_top + kSectionHeaderHeight * u;

  const Palette color{float(std::clamp((now - open_time_) / 0.15, 0.0, 1.0))};
  const ImU32 accent = color(245, 140, 28);
  const ImU32 line_color = color(255, 255, 255, 28);

  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(display);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  const bool window_open = ImGui::Begin(
      "##rex_quick_menu", nullptr,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav |
          ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleVar(2);
  if (!window_open) {
    ImGui::End();
    return;
  }
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const bool mouse_moved = io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f;
  const bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

  // Panel.
  draw_list->AddRectFilled(ImVec2(panel_min.x + 8.0f * u, panel_min.y + 10.0f * u),
                           ImVec2(panel_max.x + 8.0f * u, panel_max.y + 10.0f * u),
                           color(0, 0, 0, 90), 22.0f * u);
  draw_list->AddRectFilled(panel_min, panel_max, color(17, 19, 24, 244), 20.0f * u);
  draw_list->AddRect(panel_min, panel_max, color(255, 255, 255, 30), 20.0f * u, 0, 1.5f * u);

  // Header band: a warm glow under the title and the launcher's orange rule.
  draw_list->AddRectFilledMultiColor(
      ImVec2(panel_min.x + 2.0f * u, panel_min.y + 2.0f * u),
      ImVec2(panel_max.x - 2.0f * u, panel_min.y + (kHeaderHeight - 6.0f) * u),
      color(64, 36, 12, 150), color(17, 19, 24, 0), color(17, 19, 24, 0), color(64, 36, 12, 150));
  draw_list->AddRectFilled(ImVec2(panel_min.x + 20.0f * u, panel_min.y),
                           ImVec2(panel_max.x - 20.0f * u, panel_min.y + 3.0f * u), accent,
                           2.0f * u);
  if (!config_.subtitle.empty()) {
    DrawText(draw_list, 17.0f * u, ImVec2(tabs_left, panel_min.y + 16.0f * u),
             color(245, 140, 28, 210), config_.subtitle);
  }
  DrawText(draw_list, 46.0f * u, ImVec2(tabs_left, panel_min.y + 38.0f * u), accent, config_.title);

  // What the game renders at, and how fast.
  {
    std::string status;
    const rex::perf::RenderInfo info = rex::perf::GetRenderInfo();
    if (info.frontbuffer_width && info.scale_x && info.scale_y) {
      const uint32_t render_width = info.frontbuffer_width * info.scale_x;
      const uint32_t render_height = info.frontbuffer_height * info.scale_y;
      const uint32_t output_width =
          info.frontbuffer_width * std::max(info.requested_scale_x, info.scale_x);
      const uint32_t output_height =
          info.frontbuffer_height * std::max(info.requested_scale_y, info.scale_y);
      char text[96];
      if (render_width != output_width || render_height != output_height) {
        std::snprintf(text, sizeof(text), "%ux%u \xC2\xBB %ux%u", render_width, render_height,
                      output_width, output_height);
      } else {
        std::snprintf(text, sizeof(text), "%ux%u", render_width, render_height);
      }
      status = text;
    }
    if (fps_ > 0.0f) {
      char text[32];
      std::snprintf(text, sizeof(text), "%s%.0f FPS", status.empty() ? "" : "   ", fps_);
      status += text;
    }
    if (!status.empty()) {
      const float size = 24.0f * u;
      DrawText(draw_list, size,
               ImVec2(content_right - TextSize(size, status).x, panel_min.y + 50.0f * u),
               color(176, 188, 204), status);
    }
  }
  draw_list->AddLine(ImVec2(panel_min.x + padding * 0.5f, panel_min.y + kHeaderHeight * u),
                     ImVec2(panel_max.x - padding * 0.5f, panel_min.y + kHeaderHeight * u),
                     line_color, 1.5f * u);

  // LB / RB chips (also used by the hints below).
  const float chip_size = 18.0f * u;
  auto draw_chip = [&](float x, float y_top, std::string_view text) {
    const ImVec2 text_size = TextSize(chip_size, text);
    const ImVec2 chip_min(x, y_top);
    const ImVec2 chip_max(x + text_size.x + 18.0f * u, y_top + 30.0f * u);
    draw_list->AddRectFilled(chip_min, chip_max, color(44, 49, 61), 7.0f * u);
    draw_list->AddRect(chip_min, chip_max, color(255, 255, 255, 36), 7.0f * u, 0, 1.2f * u);
    DrawText(draw_list, chip_size,
             ImVec2(chip_min.x + 9.0f * u, chip_min.y + (chip_max.y - chip_min.y - text_size.y) * 0.5f),
             color(225, 230, 238), text);
    return chip_max.x;
  };

  // Sections (LB / RB): the list on the left.
  {
    draw_chip(tabs_left, body_top + 2.0f * u, "LB");
    draw_chip(tabs_right - TextSize(chip_size, "RB").x - 18.0f * u, body_top + 2.0f * u, "RB");
    char position[24];
    const size_t tab_at = size_t(std::find(tabs.begin(), tabs.end(), section_) - tabs.begin());
    std::snprintf(position, sizeof(position), "%u / %u", unsigned(tab_at + 1), unsigned(tabs.size()));
    const float position_size = 21.0f * u;
    DrawText(draw_list, position_size,
             ImVec2((tabs_left + tabs_right) * 0.5f - TextSize(position_size, position).x * 0.5f,
                    body_top + 5.0f * u),
             color(140, 150, 166), position);

    const float tab_height = kTabHeight * u;
    for (size_t i = 0; i < tabs.size(); ++i) {
      const size_t section_index = tabs[i];
      const std::string_view title = SplitAt(config_.sections[section_index].title, '\n', false).head;
      const ImVec2 tab_min(tabs_left - 8.0f * u, body_top + kTabsHeaderHeight * u + float(i) * tab_height);
      const ImVec2 tab_max(tabs_right + 4.0f * u, tab_min.y + tab_height - 6.0f * u);
      const bool active = section_index == section_;
      const bool hovered = ImGui::IsMouseHoveringRect(tab_min, tab_max);
      if (active) {
        draw_list->AddRectFilled(tab_min, tab_max, color(122, 66, 14, 235), 10.0f * u);
        draw_list->AddRectFilled(tab_min, ImVec2(tab_min.x + 6.0f * u, tab_max.y), accent,
                                 10.0f * u, ImDrawFlags_RoundCornersLeft);
      } else if (hovered) {
        draw_list->AddRectFilled(tab_min, tab_max, color(255, 255, 255, 16), 10.0f * u);
      }
      const float size = FitSize(28.0f * u, title, tab_max.x - tab_min.x - 36.0f * u, 20.0f * u);
      DrawText(draw_list, size,
               ImVec2(tab_min.x + 24.0f * u, (tab_min.y + tab_max.y) * 0.5f - TextSize(size, title).y * 0.5f),
               active ? color(255, 255, 255) : color(150, 160, 176), title);
      if (clicked && hovered && !active) {
        section_ = section_index;
        std::vector<size_t> shown = ShownItems(config_.sections[section_]);
        selected_item_ = shown.empty() ? 0 : shown.front();
      }
    }
    const float divider_x = tabs_right + 18.0f * u;
    draw_list->AddLine(ImVec2(divider_x, body_top), ImVec2(divider_x, body_top + body_height * u),
                       line_color, 1.5f * u);
  }

  // Section header: its name and, when it has one, a note.
  {
    const TextParts parts = SplitAt(section.title, '\n', false);
    DrawText(draw_list, 38.0f * u, ImVec2(content_left, body_top - 4.0f * u), color(255, 255, 255),
             parts.head);
    if (!parts.tail.empty()) {
      const float size = FitSize(21.0f * u, parts.tail, content_right - content_left, 15.0f * u);
      DrawText(draw_list, size, ImVec2(content_left, body_top + 44.0f * u), color(150, 162, 180),
               parts.tail);
    }
  }

  // Settings.
  const float row_height = kRowHeight * u;
  const float label_size = 31.0f * u;
  const float value_size = 29.0f * u;
  for (size_t row = 0; row < rows.size(); ++row) {
    const size_t item_index = rows[row];
    const QuickMenuItem& item = section.items[item_index];
    const ImVec2 row_min(content_left - 14.0f * u, rows_top + float(row) * row_height);
    const ImVec2 row_max(content_right + 14.0f * u, row_min.y + row_height - 6.0f * u);
    const float center_y = (row_min.y + row_max.y) * 0.5f;
    const bool hovered = ImGui::IsMouseHoveringRect(row_min, row_max);
    if (hovered && mouse_moved) {
      selected_item_ = item_index;
    }
    const bool selected = item_index == selected_item_;
    if (selected) {
      draw_list->AddRectFilled(row_min, row_max, color(132, 72, 16, 240), 10.0f * u);
      draw_list->AddRect(row_min, row_max, color(245, 140, 28, 210), 10.0f * u, 0, 1.6f * u);
      draw_list->AddRectFilled(row_min, ImVec2(row_min.x + 7.0f * u, row_max.y), accent,
                               10.0f * u, ImDrawFlags_RoundCornersLeft);
    } else if (row % 2 == 0) {
      draw_list->AddRectFilled(row_min, row_max, color(255, 255, 255, 7), 10.0f * u);
    }

    // Value, from the right edge. value_left is where it starts: the label gets the rest.
    const std::string current = rex::cvar::GetFlagByName(item.cvar);
    const float right = row_max.x - 22.0f * u;
    const ImU32 value_color = selected ? color(255, 255, 255) : color(255, 205, 140);
    float value_left = right;
    int click_direction = 0;
    if (item.kind == QuickMenuItem::Kind::kAction) {
      // [key chip]  >
      const float arrow_width = 12.0f * u, arrow_height = 20.0f * u;
      draw_list->AddTriangleFilled(
          ImVec2(right, center_y), ImVec2(right - arrow_width, center_y - arrow_height * 0.5f),
          ImVec2(right - arrow_width, center_y + arrow_height * 0.5f), value_color);
      value_left = right - arrow_width - 12.0f * u;
      if (!item.value_text.empty()) {
        const float size = 22.0f * u;
        const ImVec2 extent = TextSize(size, item.value_text);
        const ImVec2 chip_min(value_left - extent.x - 22.0f * u, center_y - 17.0f * u);
        const ImVec2 chip_max(value_left, center_y + 17.0f * u);
        draw_list->AddRectFilled(chip_min, chip_max, color(44, 49, 61), 8.0f * u);
        draw_list->AddRect(chip_min, chip_max, color(255, 255, 255, 40), 8.0f * u, 0, 1.2f * u);
        DrawText(draw_list, size, ImVec2(chip_min.x + 11.0f * u, center_y - extent.y * 0.5f),
                 color(235, 240, 246), item.value_text);
        value_left = chip_min.x;
      }
    } else if (item.kind == QuickMenuItem::Kind::kToggle) {
      const bool on = IsOn(current);
      const float switch_width = 76.0f * u, switch_height = 36.0f * u;
      const ImVec2 track_min(right - switch_width, center_y - switch_height * 0.5f);
      const ImVec2 track_max(right, center_y + switch_height * 0.5f);
      draw_list->AddRectFilled(track_min, track_max, on ? accent : color(70, 76, 92),
                               switch_height * 0.5f);
      const float knob_radius = switch_height * 0.5f - 4.0f * u;
      draw_list->AddCircleFilled(
          ImVec2(on ? track_max.x - switch_height * 0.5f : track_min.x + switch_height * 0.5f,
                 center_y),
          knob_radius, color(245, 247, 250), 24);
      const char* state_text = on ? config_.text_on.c_str() : config_.text_off.c_str();
      const float state_size = 23.0f * u;
      const ImVec2 state_extent = TextSize(state_size, state_text);
      DrawText(draw_list, state_size,
               ImVec2(track_min.x - 14.0f * u - state_extent.x, center_y - state_extent.y * 0.5f),
               selected ? color(255, 255, 255) : (on ? color(255, 205, 140) : color(150, 160, 175)),
               state_text);
      value_left = track_min.x - 14.0f * u - state_extent.x;
    } else {
      std::string value_text;
      bool can_lower = true, can_raise = true;
      if (item.kind == QuickMenuItem::Kind::kChoice) {
        const std::vector<const Choice*> choices = AllowedChoices(item);
        const int index = ChoiceIndex(choices, current);
        value_text = index >= 0 ? choices[size_t(index)]->second : current;
        if (index >= 0) {
          can_lower = index > 0;
          can_raise = index + 1 < int(choices.size());
        }
      } else {
        const double value = NumberValue(item, current);
        value_text = FormatNumber(item, value);
        const bool reversed = item.display_scale < 0.0;
        const bool at_min = value <= item.min + 1e-9, at_max = value >= item.max - 1e-9;
        can_lower = reversed ? !at_max : !at_min;
        can_raise = reversed ? !at_min : !at_max;
      }
      const float arrow_width = 12.0f * u, arrow_height = 20.0f * u, gap = 14.0f * u;
      const ImVec2 value_extent = TextSize(value_size, value_text);
      const float right_arrow_x = right - arrow_width;
      const float value_x = right_arrow_x - gap - value_extent.x;
      const float left_arrow_x = value_x - gap - arrow_width;
      auto arrow_color = [&](bool enabled) {
        return enabled ? (selected ? accent : value_color) : color(255, 255, 255, 46);
      };
      draw_list->AddTriangleFilled(ImVec2(left_arrow_x, center_y),
                                   ImVec2(left_arrow_x + arrow_width, center_y - arrow_height * 0.5f),
                                   ImVec2(left_arrow_x + arrow_width, center_y + arrow_height * 0.5f),
                                   arrow_color(can_lower));
      draw_list->AddTriangleFilled(ImVec2(right_arrow_x + arrow_width, center_y),
                                   ImVec2(right_arrow_x, center_y + arrow_height * 0.5f),
                                   ImVec2(right_arrow_x, center_y - arrow_height * 0.5f),
                                   arrow_color(can_raise));
      DrawText(draw_list, value_size, ImVec2(value_x, center_y - value_extent.y * 0.5f), value_color,
               value_text);
      value_left = left_arrow_x;
      if (clicked && hovered) {
        const float mouse_x = io.MousePos.x;
        if (mouse_x >= left_arrow_x - gap && mouse_x < value_x) {
          click_direction = -1;
        } else if (mouse_x >= value_x + value_extent.x) {
          click_direction = 1;
        }
      }
    }

    // Label, then its pills (BETA / EXPERIMENTAL tag, RESTART).
    const TextParts label = SplitAt(item.label, '|', true);
    const rex::cvar::FlagEntry* info = rex::cvar::GetFlagInfo(item.cvar);
    const bool needs_restart = info && info->lifecycle == rex::cvar::Lifecycle::kRequiresRestart;
    const std::string_view tag = label.tail;
    float pills_width = 0.0f;
    if (!tag.empty()) {
      pills_width += PillWidth(u, tag) + 10.0f * u;
    }
    if (needs_restart) {
      pills_width += PillWidth(u, config_.text_restart) + 10.0f * u;
    }
    const float label_x = row_min.x + 30.0f * u;
    const float size = FitSize(label_size, label.head,
                               value_left - 24.0f * u - label_x - pills_width, label_size * 0.7f);
    const ImVec2 label_extent = TextSize(size, label.head);
    DrawText(draw_list, size, ImVec2(label_x, center_y - label_extent.y * 0.5f),
             selected ? color(255, 255, 255) : color(226, 232, 240), label.head);
    float pill_x = label_x + label_extent.x + 14.0f * u;
    if (!tag.empty()) {
      pill_x = DrawPill(draw_list, u, pill_x, center_y, tag,
                        IsBetaTag(tag) ? color(46, 98, 196) : color(190, 122, 12),
                        color(255, 255, 255)) +
               10.0f * u;
    }
    if (needs_restart) {
      DrawPill(draw_list, u, pill_x, center_y, config_.text_restart, color(66, 48, 24),
               color(255, 168, 70));
    }

    if (clicked && hovered) {
      selected_item_ = item_index;
      Change(item, click_direction);
    }
  }

  // Help for the selected setting, in a box that wraps it.
  {
    const ImVec2 box_min(content_left - 14.0f * u, rows_top + rows_height * u + kGap * u);
    const ImVec2 box_max(content_right + 14.0f * u, box_min.y + help_height * u);
    draw_list->AddRectFilled(box_min, box_max, color(8, 10, 14, 215), 12.0f * u);
    draw_list->AddRect(box_min, box_max, color(255, 255, 255, 24), 12.0f * u, 0, 1.2f * u);
    draw_list->AddRectFilled(box_min, ImVec2(box_min.x + 5.0f * u, box_max.y), accent, 12.0f * u,
                             ImDrawFlags_RoundCornersLeft);
    if (!rows.empty()) {
      // The "i" badge, then the text; a long help gets a smaller font before it would overflow.
      const float badge_radius = 15.0f * u;
      const ImVec2 badge(box_min.x + 34.0f * u, box_min.y + 34.0f * u);
      draw_list->AddCircleFilled(badge, badge_radius, color(46, 98, 196), 24);
      const float badge_size = 21.0f * u;
      DrawText(draw_list, badge_size,
               ImVec2(badge.x - TextSize(badge_size, "i").x * 0.5f,
                      badge.y - TextSize(badge_size, "i").y * 0.5f),
               color(255, 255, 255), "i");
      const std::string& help = section.items[selected_item_].help;
      const float text_x = box_min.x + 64.0f * u;
      const float wrap = box_max.x - 20.0f * u - text_x;
      const float room = (box_max.y - box_min.y) - 28.0f * u;
      float size = 25.0f * u;
      while (size > 17.0f * u && WrappedHeight(size, help, wrap) > room) {
        size -= 1.0f * u;
      }
      DrawText(draw_list, size, ImVec2(text_x, box_min.y + 16.0f * u), color(206, 214, 226), help,
               wrap);
    }
  }

  // Button hints, and the note that settings are kept.
  {
    const float hint_size = 22.0f * u;
    const float center_y = panel_max.y - kHintsHeight * u * 0.5f;
    draw_list->AddLine(ImVec2(panel_min.x + padding * 0.5f, panel_max.y - kHintsHeight * u),
                       ImVec2(panel_max.x - padding * 0.5f, panel_max.y - kHintsHeight * u),
                       line_color, 1.5f * u);
    float x = tabs_left;
    auto draw_label = [&](std::string_view text, ImU32 text_color) {
      DrawText(draw_list, hint_size, ImVec2(x, center_y - TextSize(hint_size, text).y * 0.5f),
               text_color, text);
      x += TextSize(hint_size, text).x + 8.0f * u;
    };
    auto draw_button = [&](const char* letter, ImU32 fill, std::string_view text,
                           std::string_view key) {
      const float radius = 16.0f * u;
      draw_list->AddCircleFilled(ImVec2(x + radius, center_y), radius, fill, 24);
      const float letter_size = 20.0f * u;
      const ImVec2 letter_extent = TextSize(letter_size, letter);
      DrawText(draw_list, letter_size,
               ImVec2(x + radius - letter_extent.x * 0.5f, center_y - letter_extent.y * 0.5f),
               color(255, 255, 255), letter);
      x += radius * 2.0f + 10.0f * u;
      draw_label(text, color(214, 222, 234));
      if (!key.empty()) {
        draw_label(key, color(120, 132, 150));
      }
      x += 20.0f * u;
    };
    std::string_view change_text = config_.text_change;
    std::string_view section_text;
    if (const size_t newline = change_text.find('\n'); newline != std::string_view::npos) {
      section_text = change_text.substr(newline + 1);
      change_text = change_text.substr(0, newline);
    }
    // With keyboard play on, Enter is Start and Space is A.
    draw_button("A", color(22, 150, 62), change_text,
                rex::cvar::Query<bool>("mnk_mode") ? "Space" : "Enter");
    draw_button("B", color(206, 44, 44), config_.text_close, "Esc");
    if (!section_text.empty()) {
      const float chip_top = center_y - 15.0f * u;
      x = draw_chip(x, chip_top, "LB") + 6.0f * u;
      x = draw_chip(x, chip_top, "RB") + 10.0f * u;
      draw_label(section_text, color(214, 222, 234));
      draw_label("PgUp/PgDn", color(120, 132, 150));
      x += 20.0f * u;
    }
    if (!config_.quick_toggle_label.empty() && rex::cvar::GetFlagInfo(config_.quick_toggle_cvar)) {
      draw_button("Y", color(222, 168, 18), config_.quick_toggle_label, "");
    }
    const char* saved = config_.text_saved.c_str();
    DrawText(draw_list, hint_size,
             ImVec2(content_right - TextSize(hint_size, saved).x,
                    center_y - TextSize(hint_size, saved).y * 0.5f),
             color(130, 142, 160), saved);
  }

  ImGui::End();
}

}  // namespace rex::ui
