// mgr - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>

#if defined(__ANDROID__)
#include <rex/main_android.h>
#endif

class MgrApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<MgrApp>(new MgrApp(ctx, "mgr",
        PPCImageConfig));
  }

  // Android: o jogo fica em <pasta do app>/game (default.xex + media/), ou seja
  // /sdcard/Android/data/com.dfdx047.swordman/files/game. O cvar game_data_root
  // (cvars.txt) continua a mandar, se estiver definido.
  void OnConfigurePaths(rex::PathConfig& paths) override {
#if defined(__ANDROID__)
    if (paths.game_data_root.empty()) {
      paths.game_data_root = rex::GetAndroidAppDirectory() / "game";
    }
#else
    (void)paths;
#endif
  }

  // GPU emulada (plugin xenos, Vulkan). Sem isto o cvar gpu_plugin fica vazio, o
  // runtime corre sem GPU e o ecra fica preto ("no GPU emulation loaded" no log).
  // O cvar gpu_plugin (cvars.txt ou --gpu_plugin) continua a mandar, se definido.
  void OnPreSetup(rex::RuntimeConfig& config) override {
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnPostSetup() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
};
