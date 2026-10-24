// Dump de um frame do Renut: ver renut_frame_dump.h.

#include "renut_frame_dump.h"

#include <cstdio>
#include <filesystem>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/main_android.h>

REXCVAR_DEFINE_BOOL(swordman_renut_frame_dump, false, "SwordMan",
                    "Renut (diagnóstico): com files/frame_dump.req, escreve o frame seguinte em "
                    "logs/frame_dump_<N>.txt (cada draw, clear e resolve com shaders, alvos e texturas)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace swordman::frame_dump {
namespace {

FILE* g_file = nullptr;
uint64_t g_frame = 0;      // o frame visto por último
uint64_t g_dumping = 0;    // o frame a ser escrito (0 = nenhum)
uint32_t g_dumps = 0;

}  // namespace

bool On() {
  static const bool on = REXCVAR_GET(swordman_renut_frame_dump);
  return on;
}

bool Active(uint64_t frame) {
  if (!On()) return false;
  if (frame == g_frame) return g_dumping == frame;
  g_frame = frame;
  if (g_file) {
    std::fclose(g_file);
    g_file = nullptr;
    REXLOG_INFO("Renut: frame {} escrito (frame_dump_{}.txt)", g_dumping, g_dumps);
    g_dumping = 0;
  }
  // O pedido, uma vez a cada 30 frames.
  if (frame % 30) return false;
  const auto dir = rex::GetAndroidAppDirectory();
  std::error_code ec;
  if (!std::filesystem::remove(dir / "frame_dump.req", ec)) return false;
  ++g_dumps;
  const auto path = dir / "logs" / ("frame_dump_" + std::to_string(g_dumps) + ".txt");
  g_file = std::fopen(path.c_str(), "w");
  if (!g_file) return false;
  g_dumping = frame;
  std::fprintf(g_file, "frame %llu\n", (unsigned long long)frame);
  return true;
}

void Line(const std::string& line) {
  if (!g_file) return;
  std::fputs(line.c_str(), g_file);
  std::fputc('\n', g_file);
}

}  // namespace swordman::frame_dump
