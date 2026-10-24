// mgr - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>

#if defined(__ANDROID__)
#include <rex/main_android.h>
#endif

#if defined(SM_RENUT)
#include <cstdio>
#include <cstdlib>
#include <string>
#include <functional>
#include <optional>

#include <rex/logging.h>
#include <rex/system/xthread.h>

#include "renut_engine/gpu/banjo_renut_select.h"
#include "renut_engine/gpu/overlay/app_integration.h"

#include <stdexcept>

// src/renut_glue.cpp: as funcoes Vulkan que o Renut assume (false = fica o Xenia).
bool SwordManRenutDeviceSupported();
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
  // /sdcard/Android/data/com.dfdx047.zandatsu/files/game. O cvar game_data_root
  // (cvars.txt) continua a mandar, se estiver definido.
  void OnConfigurePaths(rex::PathConfig& paths) override {
#if defined(__ANDROID__)
    if (paths.game_data_root.empty()) {
      paths.game_data_root = rex::GetAndroidAppDirectory() / "game";
    }
    // Jogo importado de uma ISO (GameActivity.java): o descritor da ISO que o launcher abriu pelo content resolver.
    // O SDK (patch 0179) monta-a como disco (DiscImageDevice), sem pasta extraida.
    if (const char* fd = std::getenv("ZANDATSU_ISO_FD"); fd && *fd) {
      paths.game_data_root = std::string("/proc/self/fd/") + fd;
    }
#else
    (void)paths;
#endif
  }

  // GPU emulada (plugin xenos, Vulkan). Sem isto o cvar gpu_plugin fica vazio, o
  // runtime corre sem GPU e o ecra fica preto ("no GPU emulation loaded" no log).
  // O cvar gpu_plugin (cvars.txt ou --gpu_plugin) continua a mandar, se definido.
  //
  // swordman_renderer = renut (src/renut_glue.cpp): sem GPU emulada nem plugin; o Renut (renderizador nativo de
  // Direct3D do mgr-nx-deko) desenha na janela com o seu proprio dispositivo Vulkan.
  void OnPreSetup(rex::RuntimeConfig& config) override {
#if defined(SM_RENUT)
    if (banjo::renut_gpu::Active()) {
      const bool supported = SwordManRenutDeviceSupported();
      if (!supported) {
        banjo::renut_gpu::detail::g_active = -1;  // Xenia ate ao fim da sessao
      }
#if defined(__ANDROID__)
      // O launcher le-o: "no" = este aparelho fica no modo emulado e as opcoes do Renut escondem-se.
      if (FILE* f = std::fopen((rex::GetAndroidAppDirectory() / "renut_support.txt").c_str(), "w")) {
        std::fputs(supported ? "ok\n" : "no\n", f);
        std::fclose(f);
      }
#endif
    }
    if (banjo::renut_gpu::Active()) {
      renut::gpu::ConfigureRuntime(config);
      REXLOG_INFO("graphics: Renut (Direct3D nativo do mgr-nx-deko, plume/Vulkan), sem PM4 nem EDRAM");
      return;
    }
#endif
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
  }

#if defined(SM_RENUT)
  // Renut: a janela ja existe; cria o dispositivo Vulkan dele antes do runtime. Aqui ja nao ha volta ao Xenia (o
  // OnPreSetup ja correu sem plugin): a verificacao de funcoes la evita os casos conhecidos.
  std::optional<rex::PathConfig> OnFinalizePaths(const rex::PathConfig& defaults,
      std::function<void(rex::PathConfig)> resume) override {
    if (banjo::renut_gpu::Active() &&
        !renut::gpu::StartRenderer(app_context(), [this] { return imgui_drawer(); })) {
      REXLOG_ERROR("Renut: o renderizador nao arrancou (swordman_renderer = xenos para usar o Xenia)");
      throw std::runtime_error("Renut: o renderizador nao arrancou");
    }
    return ReXApp::OnFinalizePaths(defaults, std::move(resume));
  }

  // Renut: o MGR indexa o seu escalonador de tarefas (5 vias) pela CPU logica do PCR do guest. A escolha
  // round-robin do SDK conta tambem as threads de servico do host; sem a GPU emulada essa escolha muda e pode cair
  // num slot nao registado (-1: falha de memoria do guest em sub_827E2C00 ~27 s apos o arranque). Como no
  // mgr_main.cpp do mgr-nx-deko: a thread principal arranca na CPU logica 0.
  void OnPostLaunchModule(rex::system::XThread* thread) override {
    if (thread && banjo::renut_gpu::Active()) {
      thread->SetAffinity(1u);
      REXLOG_INFO("Renut: thread principal do guest na CPU logica 0");
    }
  }

  // Renut: o ImGui do SDK (menus) desenhado pelo plume.
  std::unique_ptr<rex::ui::ImmediateDrawer> OnCreateImmediateDrawer() override {
    if (banjo::renut_gpu::Active()) return renut::gpu::CreateOverlayDrawer();
    return ReXApp::OnCreateImmediateDrawer();
  }

  void OnWindowPixelSizeChanged(uint32_t width, uint32_t height) override {
    ReXApp::OnWindowPixelSizeChanged(width, height);
    if (banjo::renut_gpu::Active()) renut::gpu::OnWindowResized();
  }

  void OnShutdown() override {
    if (banjo::renut_gpu::Active()) renut::gpu::ShutdownRenderer();
  }
#endif

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnPostSetup() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
};
