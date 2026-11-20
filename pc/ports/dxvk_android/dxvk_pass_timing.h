#pragma once
// GPU time per render pass for DXVK 1.9.2b (no per-pass timestamps upstream). DXVK_PASS_TIMING=<frames> (e.g. 300):
// each render pass gets a BOTTOM_OF_PIPE timestamp before vkCmdBeginRenderPass and after vkCmdEndRenderPass; when the
// command list's fence has signalled, the pairs are summed per pass key (ordinal in the frame, size, formats). Every
// <frames> presents the table goes to logcat (tag MGR, "pass-timing"), ms per frame, largest first. The ordinal is reset
// by a marker recorded into the CS stream at Present, so it counts passes in GPU submission order.
// Copied into the prepared DXVK source as src/dxvk/dxvk_pass_timing.h by prepare.py.
#include <vulkan/vulkan.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <tuple>
#include <vector>
#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace dxvk::passtiming {
  struct Label { uint32_t ordinal, w, h, color0, color1, depth, natt, draws; };

  inline uint32_t interval() {
    static const uint32_t value = [] { const char* s = std::getenv("DXVK_PASS_TIMING"); return s ? uint32_t(std::atoi(s)) : 0u; }();
    return value;
  }
  inline bool enabled() { return interval() != 0; }

  struct Totals { double ns = 0; uint64_t passes = 0; uint64_t draws = 0; };
  using Key = std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>;
  inline std::mutex& lock() { static std::mutex m; return m; }
  inline std::map<Key, Totals>& table() { static std::map<Key, Totals> t; return t; }
  inline uint32_t& ordinal() { static uint32_t o = 0; return o; }      // CS thread only
  inline std::atomic<uint64_t>& frames() { static std::atomic<uint64_t> f{0}; return f; }
  inline double& gapNs() { static double g = 0; return g; }

  inline void log(const char* text) {
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "MGR", "pass-timing %s", text);
#else
    std::fprintf(stderr, "pass-timing %s\n", text);
#endif
  }

  inline void flushTable(uint64_t nframes) {
    std::vector<std::pair<Key, Totals>> rows(table().begin(), table().end());
    std::sort(rows.begin(), rows.end(), [](auto& a, auto& b) { return a.second.ns > b.second.ns; });
    double total = 0; for (auto& r : rows) total += r.second.ns;
    char line[256];
    std::snprintf(line, sizeof line, "frames %llu: render passes %.2f ms/frame, between passes %.2f ms/frame",
      (unsigned long long) nframes, total / 1e6 / nframes, gapNs() / 1e6 / nframes);
    log(line);
    for (size_t i = 0; i < rows.size() && i < 40; i++) {
      auto& [k, t] = rows[i];
      std::snprintf(line, sizeof line, "#%02u %4ux%-4u c0=%u c1=%u d=%u att=%u  %6.3f ms/frame  %5.2f passes/frame  %6.1f draws/frame",
        std::get<0>(k), std::get<1>(k), std::get<2>(k), std::get<3>(k), std::get<4>(k), std::get<5>(k), std::get<6>(k),
        t.ns / 1e6 / nframes, double(t.passes) / nframes, double(t.draws) / nframes);
      log(line);
    }
    table().clear(); gapNs() = 0;
  }

  // CS thread, in stream order: a Present was recorded.
  inline void frame() {
    ordinal() = 0;
    uint64_t f = ++frames();
    if (f % interval() == 0) {
      std::lock_guard<std::mutex> g(lock());
      flushTable(interval());
    }
  }

  inline uint32_t nextOrdinal() { return ordinal()++; }

  // Submission thread, after the command list's fence signalled.
  inline void submit(const std::vector<Label>& labels, const uint64_t* ts, uint32_t count, float periodNs) {
    std::lock_guard<std::mutex> g(lock());
    for (uint32_t i = 0; i + 1 < count && i / 2 < labels.size(); i += 2) {
      const Label& l = labels[i / 2];
      if (ts[i + 1] < ts[i]) continue;
      Totals& t = table()[Key(std::min(l.ordinal, 99u), l.w, l.h, l.color0, l.color1, l.depth, l.natt)];
      t.ns += double(ts[i + 1] - ts[i]) * periodNs; t.passes++; t.draws += l.draws;
      if (i >= 2 && ts[i] > ts[i - 1]) gapNs() += double(ts[i] - ts[i - 1]) * periodNs;
    }
  }
}
