// zdiag: JSON, opcoes, executor dos testes (progresso, passos, force_crash), PNG.
#include <android/log.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>

#include "zdiag.h"

namespace zdiag {

//-----------------------------------------------------------------------------
// JSON
//-----------------------------------------------------------------------------
void Json::str(std::string_view x) {
  s_ += '"';
  for (unsigned char c : x) {
    switch (c) {
      case '"': s_ += "\\\""; break;
      case '\\': s_ += "\\\\"; break;
      case '\n': s_ += "\\n"; break;
      case '\r': s_ += "\\r"; break;
      case '\t': s_ += "\\t"; break;
      default:
        if (c < 0x20) {
          char b[8];
          snprintf(b, sizeof(b), "\\u%04x", c);
          s_ += b;
        } else {
          s_ += char(c);
        }
    }
  }
  s_ += '"';
}

void Json::num(double x) {
  if (!std::isfinite(x)) { s_ += "null"; return; }
  char b[32];
  snprintf(b, sizeof(b), "%.9g", x);
  s_ += b;
}

Json& Json::hex(std::string_view key, uint64_t x) {
  char b[24];
  snprintf(b, sizeof(b), "0x%llx", (unsigned long long)x);
  return k(key).v(b);
}

//-----------------------------------------------------------------------------
// Opcoes
//-----------------------------------------------------------------------------
Options Options::Parse(std::string_view s) {
  Options o;
  size_t i = 0;
  while (i < s.size()) {
    size_t e = s.find(';', i);
    if (e == std::string_view::npos) e = s.size();
    std::string_view item = s.substr(i, e - i);
    size_t eq = item.find('=');
    auto trim = [](std::string_view x) {
      while (!x.empty() && x.front() == ' ') x.remove_prefix(1);
      while (!x.empty() && x.back() == ' ') x.remove_suffix(1);
      return std::string(x);
    };
    if (!item.empty()) {
      if (eq == std::string_view::npos) o.kv[trim(item)] = "1";
      else o.kv[trim(item.substr(0, eq))] = trim(item.substr(eq + 1));
    }
    i = e + 1;
  }
  return o;
}

std::string Options::str(const std::string& k, const std::string& def) const {
  auto it = kv.find(k);
  return it == kv.end() ? def : it->second;
}

long Options::num(const std::string& k, long def) const {
  auto it = kv.find(k);
  if (it == kv.end() || it->second.empty()) return def;
  return strtol(it->second.c_str(), nullptr, 0);
}

//-----------------------------------------------------------------------------
// Utilitarios
//-----------------------------------------------------------------------------
void Log(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char b[1024];
  vsnprintf(b, sizeof(b), fmt, ap);
  va_end(ap);
  __android_log_write(ANDROID_LOG_INFO, "zdiag", b);
#ifdef ZDIAG_CLI
  fprintf(stderr, "%s\n", b);
#endif
}

double NowMs() {
  using namespace std::chrono;
  return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

const char* StName(St s) { return s == St::Pass ? "pass" : s == St::Fail ? "fail" : "skip"; }

static bool WriteFile(const std::string& path, const void* data, size_t size, bool sync) {
  int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (fd < 0) return false;
  const char* p = static_cast<const char*>(data);
  while (size) {
    ssize_t n = write(fd, p, size);
    if (n <= 0) break;
    p += n;
    size -= size_t(n);
  }
  if (sync) fsync(fd);
  close(fd);
  return size == 0;
}

//-----------------------------------------------------------------------------
// Test
//-----------------------------------------------------------------------------
static Test* g_current = nullptr;

// Sinais fatais: o .progress fica com o passo da THREAD que morreu (com varias threads a criar pipelines o
// ultimo passo escrito pode ser de outra thread; e o debuggerd demora a parar as outras). Linha 1 = passo,
// linha 2 = "signal=N". Depois passa ao handler anterior (debuggerd/tombstone).
namespace {
char g_progress_path[512];
char g_last_step[160];
std::atomic<bool> g_frozen{false};
thread_local char tl_step[160];
struct sigaction g_old_actions[65];

void CrashHandler(int sig, siginfo_t* info, void* uctx) {
  if (!g_frozen.exchange(true) && g_progress_path[0]) {
    int fd = open(g_progress_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd >= 0) {
      const char* step = tl_step[0] ? tl_step : g_last_step;
      (void)!write(fd, step, strlen(step));
      if (!tl_step[0]) (void)!write(fd, " (outra thread)", 15);
      char b[24] = "\nsignal=";
      int n = 8, v = sig;
      char digits[8];
      int k = 0;
      do { digits[k++] = char('0' + v % 10); v /= 10; } while (v && k < 7);
      while (k) b[n++] = digits[--k];
      (void)!write(fd, b, size_t(n));
      close(fd);
    }
  }
  struct sigaction& o = g_old_actions[sig];
  if ((o.sa_flags & SA_SIGINFO) && o.sa_sigaction) {
    o.sa_sigaction(sig, info, uctx);
    return;
  }
  if (!(o.sa_flags & SA_SIGINFO) && o.sa_handler != SIG_DFL && o.sa_handler != SIG_IGN) {
    o.sa_handler(sig);
    return;
  }
  signal(sig, SIG_DFL);
  raise(sig);
}

void InstallCrashHandlers(const std::string& progress_path) {
  snprintf(g_progress_path, sizeof(g_progress_path), "%s", progress_path.c_str());
  g_frozen = false;
  static bool installed = false;
  if (installed) return;
  installed = true;
  for (int sig : {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE, SIGTRAP}) {
    struct sigaction sa{};
    sa.sa_sigaction = CrashHandler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, &g_old_actions[sig]);
  }
}
}  // namespace

void NoteResult(VkResult r) {
  if (r != VK_ERROR_DEVICE_LOST && r != VK_TIMEOUT) return;
  if (g_current && !g_current->gpu_dead) {
    g_current->gpu_dead = true;
    g_current->gpu_dead_reason = ResultName(r);
  }
}

Test::Test(std::string id_, std::string out_dir, Options o)
    : id(std::move(id_)), outDir(std::move(out_dir)), opt(std::move(o)) {
  progress_path_ = outDir + "/" + id + ".progress";
  crash_at_ = opt.str("force_crash");
  fsync_ = opt.num("fsync", 1) != 0;
  details.obj();
  steps_.arr();
}

void Test::Progress(const std::string& step) {
  snprintf(tl_step, sizeof(tl_step), "%s", step.c_str());
  {
    std::lock_guard<std::mutex> lock(progress_mu_);
    if (g_frozen) return;
    snprintf(g_last_step, sizeof(g_last_step), "%s", step.c_str());
    WriteFile(progress_path_, step.data(), step.size(), fsync_);
  }
  if (!crash_at_.empty() && crash_at_ == id + "/" + step) {
    Log("force_crash em %s/%s", id.c_str(), step.c_str());
    // SIGSEGV de verdade (o compilador nao sabe que o ponteiro e nulo).
    volatile int* volatile p = nullptr;
    *p = 0x2DEAD;
    raise(SIGSEGV);
  }
}

St Test::Step(const std::string& name, const std::function<St(Json&)>& fn) {
  if (gpu_dead) return SkipStep(name, "GPU perdida antes: " + gpu_dead_reason);
  Progress(name);
  Log("%s/%s", id.c_str(), name.c_str());
  Json info;
  info.obj();
  St st = St::Fail;
  double t0 = NowMs();
  try {
    st = fn(info);
  } catch (const std::exception& e) {
    info.kv("error", e.what());
    Log("%s/%s: %s", id.c_str(), name.c_str(), e.what());
    st = St::Fail;
  }
  double ms = NowMs() - t0;
  info.end();
  steps_.obj().kv("name", name).kv("status", StName(st)).kv("ms", std::round(ms * 100) / 100);
  steps_.k("info").raw(info.str()).end();
  (st == St::Pass ? passes : st == St::Fail ? fails : skips)++;
  return st;
}

St Test::SkipStep(const std::string& name, const std::string& reason) {
  steps_.obj().kv("name", name).kv("status", "skip").kv("ms", 0);
  steps_.k("info").obj().kv("reason", reason).end().end();
  skips++;
  return St::Skip;
}

void Test::Finish() {
  details.end_all();
  steps_.end_all();
}

std::string Test::Png(const std::string& name, const uint8_t* rgba, int w, int h, int stride, int scale,
                      bool keep_alpha) {
  if (scale < 1) scale = 1;
  int ow = (w + scale - 1) / scale, oh = (h + scale - 1) / scale;
  int ch = keep_alpha ? 4 : 3;
  std::vector<uint8_t> out(size_t(ow) * oh * ch);
  for (int y = 0; y < oh; ++y) {
    for (int x = 0; x < ow; ++x) {
      const uint8_t* s = rgba + size_t(y * scale) * stride + size_t(x * scale) * 4;
      memcpy(&out[(size_t(y) * ow + x) * ch], s, ch);
    }
  }
  std::string file = id + "_" + name + ".png";
  for (char& c : file)
    if (c == '/') c = '_';
  if (!WritePng(outDir + "/" + file, ow, oh, ch, out.data())) return "";
  return file;
}

//-----------------------------------------------------------------------------
// PNG com zlib (o NDK traz a libz)
//-----------------------------------------------------------------------------
static void Be32(std::vector<uint8_t>& v, uint32_t x) {
  v.push_back(uint8_t(x >> 24)); v.push_back(uint8_t(x >> 16)); v.push_back(uint8_t(x >> 8)); v.push_back(uint8_t(x));
}
static void Chunk(std::vector<uint8_t>& png, const char* type, const uint8_t* data, size_t n) {
  Be32(png, uint32_t(n));
  size_t start = png.size();
  png.insert(png.end(), type, type + 4);
  if (n) png.insert(png.end(), data, data + n);
  Be32(png, uint32_t(crc32(0, png.data() + start, uInt(png.size() - start))));
}

bool WritePng(const std::string& path, int w, int h, int channels, const uint8_t* px) {
  const size_t row = size_t(w) * channels;
  std::vector<uint8_t> raw((row + 1) * h);
  for (int y = 0; y < h; ++y) {
    raw[y * (row + 1)] = 0;  // filtro "none"
    memcpy(&raw[y * (row + 1) + 1], px + y * row, row);
  }
  uLongf zsize = compressBound(uLong(raw.size()));
  std::vector<uint8_t> z(zsize);
  if (compress2(z.data(), &zsize, raw.data(), uLong(raw.size()), 6) != Z_OK) return false;
  std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  std::vector<uint8_t> ihdr;
  Be32(ihdr, uint32_t(w));
  Be32(ihdr, uint32_t(h));
  ihdr.push_back(8);
  ihdr.push_back(channels == 4 ? 6 : channels == 3 ? 2 : channels == 2 ? 4 : 0);
  ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
  Chunk(png, "IHDR", ihdr.data(), ihdr.size());
  Chunk(png, "IDAT", z.data(), zsize);
  Chunk(png, "IEND", nullptr, 0);
  return WriteFile(path, png.data(), png.size(), false);
}

//-----------------------------------------------------------------------------
// Executor
//-----------------------------------------------------------------------------
const std::vector<TestDef>& AllTests() {
  static const std::vector<TestDef> tests = {
      {"info", TestInfo},
      {"pipelines_serial", TestPipelinesSerial},
      {"pipelines_parallel", TestPipelinesParallel},
      {"viewport_scissor", TestViewportScissor},
      {"clear_loadop", TestClearLoadOp},
      {"bc_decode", TestBcDecode},
      {"timestamps", TestTimestamps},
  };
  return tests;
}

std::string TestsJson() {
  Json j;
  j.arr();
  for (const TestDef& t : AllTests()) j.v(t.id);
  j.end();
  return j.str();
}

std::string RunTest(const std::string& id, const std::string& out_dir, const LoaderInfo& li,
                    const std::string& options) {
  mkdir(out_dir.c_str(), 0755);
  Test t(id, out_dir, Options::Parse(options));
  g_current = &t;
  InstallCrashHandlers(out_dir + "/" + id + ".progress");
  const TestDef* def = nullptr;
  for (const TestDef& d : AllTests())
    if (id == d.id) def = &d;
  double t0 = NowMs();
  std::string fatal;
  if (!def) {
    fatal = "teste desconhecido";
  } else {
    t.Progress("load_vulkan");
    if (!LoadVulkan(li)) {
      fatal = "Vulkan nao carregou: " + LastLoaderError();
    } else {
      try {
        def->fn(t);
      } catch (const std::exception& e) {
        fatal = e.what();
      }
    }
  }
  t.Finish();
  double ms = NowMs() - t0;
  const char* status = !fatal.empty() || t.fails ? "fail" : t.passes ? "pass" : "skip";
  std::string message = t.message;
  if (!fatal.empty()) message = message.empty() ? fatal : message + " | " + fatal;

  Json r;
  r.obj().kv("id", id).kv("status", status).kv("durationMs", long(std::lround(ms))).kv("message", message);
  r.k("steps").raw(t.steps_json());
  r.k("details").raw(t.details.str());
  r.k("vulkan");
  WriteLoaderJson(r);
  r.end();
  std::string json = r.str();
  WriteFile(out_dir + "/" + id + ".json", json.data(), json.size(), true);
  unlink((out_dir + "/" + id + ".progress").c_str());
  g_progress_path[0] = 0;
  tl_step[0] = 0;
  Log("%s: %s (%.0f ms) %s", id.c_str(), status, ms, message.c_str());
  g_current = nullptr;
  return json;
}

}  // namespace zdiag
