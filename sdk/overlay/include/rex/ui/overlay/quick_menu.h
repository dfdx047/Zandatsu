/**
 * @file        rex/ui/overlay/quick_menu.h
 *
 * @brief       Controller-friendly settings menu drawn over the game: a few
 *              hand-picked cvars per section, applied live and saved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <rex/ui/imgui_dialog.h>

namespace rex::ui {

// A setting in the quick menu, backed by a cvar.
//
// Layout note: these structs keep the exact layout older apps were built with (the exe
// and rexruntime.dll are built apart), so the extra bits the panel understands ride
// inside existing strings; an older panel just shows them as plain text:
//   QuickMenuItem::label         "Name|TAG"  draws TAG (BETA, EXPERIMENTAL...) as a pill after Name.
//   QuickMenuSection::title      "TITLE" + newline + "note": the note shows, small, under the header.
//   QuickMenuConfig::text_change "Change" + newline + "Section": the 2nd line names the LB / RB hint.
struct QuickMenuItem {
  enum class Kind {
    // Boolean cvar, shown as a switch.
    kToggle,
    // One of `choices`.
    kChoice,
    // A number from `min` to `max` in `step` increments.
    kNumber,
    // A button: runs `action` (on the UI thread, after the paint) and closes
    // the menu. `cvar` may be empty; `value_text` is shown on the right.
    kAction,
  };

  Kind kind = Kind::kToggle;
  std::string label;
  // Shown in the help box while the item is selected (it wraps; keep it to a sentence or two).
  std::string help;
  std::string cvar;
  // Set to the same value as `cvar` (like draw_resolution_scale_y with _x).
  std::vector<std::string> mirrored_cvars;
  // kChoice: the cvar value and the text shown for it. Values the cvar doesn't
  // allow are left out.
  std::vector<std::pair<std::string, std::string>> choices;
  // kNumber. Shown as printf(format, value * display_scale + display_offset);
  // with a negative display_scale, right lowers the cvar value.
  double min = 0.0;
  double max = 1.0;
  double step = 1.0;
  double display_scale = 1.0;
  double display_offset = 0.0;
  std::string format = "%g";
  // Only shown while the cvar `shown_if_cvar` has one of `shown_if_values`.
  std::string shown_if_cvar;
  std::vector<std::string> shown_if_values;
  // kAction.
  std::function<void()> action;
  std::string value_text;
};

struct QuickMenuSection {
  // Name in the section list; after a newline, a one-line note shown under the header.
  std::string title;
  std::vector<QuickMenuItem> items;
};

struct QuickMenuConfig {
  std::string title = "SETTINGS";
  std::vector<QuickMenuSection> sections;
  // Y on the controller turns this boolean cvar on or off and closes the menu,
  // a shortcut to a mode like a free camera. Empty = none.
  std::string quick_toggle_label;
  std::string quick_toggle_cvar;
  // Shown small over the title (the game's name, for instance).
  std::string subtitle;
  // UI text, so apps can translate it.
  std::string text_change = "Change";
  std::string text_close = "Close";
  std::string text_saved = "Saved automatically";
  std::string text_restart = "RESTART";
  std::string text_on = "ON";
  std::string text_off = "OFF";
  // Called (on the UI thread) after a setting changed, instead of saving the
  // whole cvar config: apps with their own settings file sync and save here.
  std::function<void()> on_changed;
  // When set and false, the menu doesn't open (e.g. before the game starts).
  std::function<bool()> can_open;
};

class QuickMenuDialog : public ImGuiDialog {
 public:
  struct PadState {
    uint16_t buttons = 0;  // X_INPUT_GAMEPAD_* bits.
    int16_t thumb_lx = 0;
    int16_t thumb_ly = 0;
  };

  struct Callbacks {
    // Reads the controllers. Called on the UI thread, outside of painting.
    std::function<PadState()> read_pad;
    // Runs a function on the UI thread after the current paint.
    std::function<void(std::function<void()>)> defer;
    // Asks the owner to destroy the menu (run through `defer`).
    std::function<void()> close;
    // After a setting changed, e.g. to save the config (run through `defer`).
    std::function<void()> changed;
  };

  // Text is drawn with overlay_text (its fonts must be added to the atlas).
  QuickMenuDialog(ImGuiDrawer* imgui_drawer, QuickMenuConfig config, Callbacks callbacks);
  ~QuickMenuDialog() override;

  // Whether a quick menu is open (it has the controllers then). Any thread.
  static bool IsOpen();

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  struct SharedPad;

  bool IsShown(const QuickMenuItem& item) const;
  std::vector<size_t> ShownItems(const QuickMenuSection& section) const;
  // direction: -1 left, 1 right, 0 activate (toggle / next).
  void Change(const QuickMenuItem& item, int direction);
  void Set(const QuickMenuItem& item, const std::string& value);
  void RequestClose();

  QuickMenuConfig config_;
  Callbacks callbacks_;
  std::shared_ptr<SharedPad> pad_;

  size_t section_ = 0;
  size_t selected_item_ = 0;
  bool close_requested_ = false;

  bool pad_seen_ = false;
  uint16_t last_buttons_ = 0;
  // Pressed to close the menu, which happens when they're released.
  uint16_t close_buttons_ = 0;
  // Y was pressed: toggle quick_toggle_cvar when closing.
  bool quick_toggle_ = false;
  int repeat_direction_ = 0;
  double repeat_time_ = 0.0;
  double open_time_ = -1.0;

  uint64_t fps_swaps_ = 0;
  double fps_time_ = -1.0;
  float fps_ = 0.0f;
};

}  // namespace rex::ui
