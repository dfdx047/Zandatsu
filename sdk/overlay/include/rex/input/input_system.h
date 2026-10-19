#pragma once
/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include <rex/input/device_assignment.h>
#include <rex/input/input.h>
#include <rex/input/input_driver.h>
#include <rex/input/state_merge.h>
#include <rex/system/interfaces/input.h>

namespace rex::ui {
class Window;
}

namespace rex::input {

class InputSystem : public system::IInputSystem {
 public:
  explicit InputSystem(rex::ui::Window* window);
  ~InputSystem() override;

  rex::ui::Window* window() const { return window_; }

  X_STATUS Setup() override;
  void Shutdown() override;

  void AddDriver(std::unique_ptr<InputDriver> driver);
  void AttachWindow(rex::ui::Window* window);
  void SetActiveCallback(std::function<bool()> callback);

  /// Replaces any previous assignment. Call before the guest starts polling.
  void SetDeviceAssignment(std::unique_ptr<DeviceAssignment> assignment);

  X_RESULT GetCapabilities(uint32_t user_index, uint32_t flags, X_INPUT_CAPABILITIES* out_caps);
  X_RESULT GetState(uint32_t user_index, X_INPUT_STATE* out_state);
  X_RESULT SetState(uint32_t user_index, X_INPUT_VIBRATION* vibration);
  X_RESULT GetKeystroke(uint32_t user_index, uint32_t flags, X_INPUT_KEYSTROKE* out_keystroke);
  /// The controller state for the emulator's own dialogs: ignores the UI
  /// blockers and the toggle combo mask.
  X_RESULT GetStateForUI(uint32_t user_index, X_INPUT_STATE* out_state);

  /// While any blocker is held the guest reads a neutral pad. Buttons still
  /// held when the last one drops stay masked until released, so the press
  /// that dismissed the dialog does not also reach the game.
  void AddUIInputBlocker();
  void RemoveUIInputBlocker();

  /// Calls `callback` on the polling thread when all of `buttons` (X_INPUT_GAMEPAD_*)
  /// get held on one controller, like Back + Start to open a menu. The guest
  /// doesn't see that press.
  void SetUIToggleCombo(uint16_t buttons, std::function<void()> callback);
  /// Changes the buttons of the combo, 0 to turn it off.
  void SetUIToggleComboButtons(uint16_t buttons);

 private:
  /// Re-enumerates every driver and notifies the assignment when the set
  /// changed.
  void RefreshDevices();
  InputDriver* DriverForDevice(DeviceId id);
  const DeviceInfo* DeviceInfoFor(DeviceId id) const;

  rex::ui::Window* window_ = nullptr;

  std::vector<std::unique_ptr<InputDriver>> drivers_;

  std::unique_ptr<DeviceAssignment> assignment_;
  ActiveDeviceTracker active_devices_;

  // Ordered by ordinal. Ordinals are never recycled, so unplugging pad one
  // does not renumber pad two.
  std::vector<DeviceInfo> devices_;
  std::vector<InputDriver*> device_owners_;

  // The guest polls from its threads while dialogs read the controllers from
  // the UI thread.
  std::recursive_mutex mutex_;
  std::atomic<int> ui_input_blockers_{0};
  // Masked out per user until the guest sees them released.
  std::array<uint16_t, kMaxGuestUsers> consumed_buttons_ = {};
  std::atomic<uint16_t> ui_toggle_combo_{0};
  std::function<void()> ui_toggle_callback_;
  std::array<bool, kMaxGuestUsers> ui_toggle_combo_held_ = {};
};

/// Create a default InputSystem with SDL + NOP drivers.
/// In tool mode, only the NOP driver is added.
std::unique_ptr<InputSystem> CreateDefaultInputSystem(bool tool_mode);

}  // namespace rex::input
