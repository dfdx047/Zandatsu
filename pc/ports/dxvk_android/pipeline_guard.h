// Android: the PowerVR BXM-8-256 driver (MediaTek MT6855, vulkan.mtk.so +0x611B8)
// faults inside vkCreateGraphicsPipelines reading a wild pointer (fault addresses
// 0x4, 0x600000008, ASCII text...). No such device has presented a frame in any of
// the four ports, and it cannot be reproduced here, so on Imagination GPUs the call
// runs under a fault guard: a fault moves to the next, plainer form of the create
// info and the pipeline is retried. Every step is logged (tag DXVK) so diagnostics
// from these phones say what the driver accepts. Past the last level the fault is
// not caught and the process crashes with a normal tombstone.
#pragma once

#include <atomic>
#include <csetjmp>
#include <csignal>

#include "../util/log/log.h"

namespace dxvk::powervr {

  constexpr uint32_t VendorId = 0x1010;
  constexpr int      LastLevel = 3;

  inline std::atomic<int> g_level = { 0 };
  inline thread_local sigjmp_buf* t_guard = nullptr;
  inline thread_local void*       t_fault = nullptr;
  inline struct sigaction         g_previous[2];

  inline void onFault(int signal, siginfo_t* info, void*) {
    if (sigjmp_buf* guard = t_guard) {
      t_guard = nullptr;
      t_fault = info->si_addr;
      siglongjmp(*guard, 1);
    }
    // Not ours: back to the previous handlers, the faulting instruction runs again.
    sigaction(SIGSEGV, &g_previous[0], nullptr);
    sigaction(SIGBUS,  &g_previous[1], nullptr);
  }

  inline void install() {
    static std::atomic<bool> installed = { false };
    if (installed.exchange(true))
      return;
    struct sigaction action = { };
    action.sa_sigaction = onFault;
    action.sa_flags     = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    sigaction(SIGSEGV, &action, &g_previous[0]);
    sigaction(SIGBUS,  &action, &g_previous[1]);
  }

  // create(level): applies what that level drops, then calls the driver.
  template<typename Create>
  VkResult guarded(const Create& create) {
    install();
    static std::atomic<int> reported = { -1 };
    while (true) {
      int level = g_level.load();
      sigjmp_buf guard;
      if (level <= LastLevel && sigsetjmp(guard, 1)) {
        Logger::warn(str::format("PowerVR: vkCreateGraphicsPipelines faulted at level ", level,
          " (address ", t_fault, "), retrying at level ", level + 1));
        g_level.store(level + 1);
        continue;
      }
      t_guard = level <= LastLevel ? &guard : nullptr;
      VkResult result = create(level);
      t_guard = nullptr;
      if (reported.exchange(level) != level)
        Logger::warn(str::format("PowerVR: graphics pipelines work at level ", level, ", result ", result));
      return result;
    }
  }

}
