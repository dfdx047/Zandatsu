// zdiag: perfil de CPU do processo do jogo durante o teste de desempenho (teste de GPU, 04/11).
//
// Pergunta a responder: numa POCO X7 Pro (Mali-G720, Dimensity 8400) a thread principal do jogo leva 30 ms por frame
// mas so usa 14% de um nucleo. Onde esta o resto: na fila a espera de um nucleo, a dormir (a espera de outra thread ou
// de um temporizador), ou a correr num nucleo lento?
//
// Escreve em <dir>/ (o DiagRunner junta tudo no zip):
//   threads.csv  a cada 1 s, por thread: utime/stime, schedstat (correr / fila / fatias), trocas de contexto
//                voluntarias e forcadas, page faults, nucleo, prioridade, mascara de nucleos (valores acumulados)
//   cpufreq.csv  a cada 100 ms, a frequencia de cada nucleo; a cada 1 s o limite (scaling_max_freq)
//   stacks.txt   pilhas das threads do jogo (a principal e as mais ocupadas), 200 por segundo cada, separadas pelo
//                estado no momento (R = a correr ou na fila, S/D = bloqueada), agregadas desde o ficheiro <dir>/go
//   process.txt  cpuset, mascara do processo, modulos carregados com o build-id (para simbolizar no PC)
//   sched_<tid>_<n>.txt  /proc/self/task/<tid>/sched da thread principal (se o kernel o der), no go e 40 s depois
//
// A amostragem: uma thread envia SIGPROF (rt_tgsigqueueinfo, com um numero de sequencia) a cada thread alvo; o handler
// guarda o pc, o lr e a cadeia de frame pointers. Uma thread bloqueada num futex acorda, corre o handler e volta a
// esperar (SA_RESTART). So threads do guest (XThread, Havok, CriMana): as esperas delas aceitam acordar sem motivo.
#include <dirent.h>
#include <fcntl.h>
#include <link.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <ucontext.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "zdiag.h"

namespace zdiag {
namespace {

constexpr int kMaxFrames = 24;
constexpr int kSlots = 64;
constexpr int kTargets = 8;
constexpr int kSampleUs = 5000;  // 200 Hz por thread

struct Slot {
  std::atomic<uint32_t> seq{0};
  int n = 0;
  uint64_t frames[kMaxFrames];
  uint64_t scan[8];
  int nscan = 0;
  // A pilha da thread alvo ([anon:stack_and_tls:<tid>] no /proc/self/maps), posta pelo amostrador antes do sinal.
  // Sem ela so se guardam o pc e o lr: um x29 que nao e frame pointer (driver, libc) nao pode levar a ler fora.
  uint64_t stack_lo = 0, stack_hi = 0;
};
Slot g_slots[kSlots];
std::atomic<bool> g_started{false};
uint64_t g_text_lo = 0, g_text_hi = 0;  // o maior modulo executavel (libgame.so), para o varrimento da pilha

uint64_t Strip(uint64_t a) { return a & 0x0000FFFFFFFFFFFFull; }  // PAC / tag do ponteiro

void Handler(int, siginfo_t* si, void* ucv) {
  const uint32_t seq = uint32_t(si->si_value.sival_int);
  Slot& s = g_slots[seq % kSlots];
  auto* uc = static_cast<ucontext_t*>(ucv);
  const uint64_t pc = uc->uc_mcontext.pc, sp = uc->uc_mcontext.sp;
  uint64_t fp = uc->uc_mcontext.regs[29];
  int n = 0;
  s.frames[n++] = Strip(pc);
  s.frames[n++] = Strip(uc->uc_mcontext.regs[30]);
  const uint64_t lo = s.stack_lo, hi = s.stack_hi;
  uint64_t prev = sp;
  while (hi && n < kMaxFrames && fp >= prev && fp >= lo && fp + 16 <= hi && (fp & 15) == 0) {
    const uint64_t* f = reinterpret_cast<const uint64_t*>(fp);
    const uint64_t ret = Strip(f[1]);
    if (ret < 4096) break;
    s.frames[n++] = ret;
    prev = fp + 16;
    fp = f[0];
  }
  s.n = n;
  // Se a cadeia e curta (codigo sem frame pointer), os enderecos do modulo grande que estao na pilha.
  s.nscan = 0;
  if (n < 5 && g_text_hi && hi && sp >= lo && sp < hi) {
    const uint64_t* w = reinterpret_cast<const uint64_t*>(sp & ~7ull);
    const int words = int(std::min<uint64_t>(96, (hi - (sp & ~7ull)) / 8));
    for (int i = 0; i < words && s.nscan < 8; ++i) {
      const uint64_t v = Strip(w[i]);
      if (v >= g_text_lo && v < g_text_hi) s.scan[s.nscan++] = v;
    }
  }
  s.seq.store(seq, std::memory_order_release);
}

std::string ReadFile(const std::string& p, size_t max = 65536) {
  std::string out;
  FILE* f = fopen(p.c_str(), "re");
  if (!f) return out;
  char buf[4096];
  size_t r;
  while (out.size() < max && (r = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, r);
  fclose(f);
  return out;
}

void WriteText(const std::string& path, const std::string& s) {
  const std::string tmp = path + ".tmp";
  if (FILE* f = fopen(tmp.c_str(), "we")) {
    fwrite(s.data(), 1, s.size(), f);
    fclose(f);
    rename(tmp.c_str(), path.c_str());
  }
}

double WallMs() {
  return std::chrono::duration<double, std::milli>(std::chrono::system_clock::now().time_since_epoch()).count();
}

struct ThreadStat {
  std::string comm;
  char state = '?';
  long utime = 0, stime = 0, minflt = 0, majflt = 0, prio = 0, nice = 0;
  int cpu = -1;
  unsigned long long run_ns = 0, wait_ns = 0, slices = 0;
  long vcsw = 0, nvcsw = 0;
  std::string allowed;
  bool ok = false;
};

// stat: "tid (comm) S ppid ..." - os campos depois do ")" comecam no 3.
bool ParseStat(const std::string& s, ThreadStat& t) {
  const size_t open = s.find('('), close = s.rfind(')');
  if (open == std::string::npos || close == std::string::npos || close + 2 >= s.size()) return false;
  t.comm = s.substr(open + 1, close - open - 1);
  std::vector<std::string> f;
  size_t i = close + 2;
  while (i < s.size()) {
    size_t j = s.find(' ', i);
    if (j == std::string::npos) j = s.size();
    f.push_back(s.substr(i, j - i));
    i = j + 1;
  }
  if (f.size() < 37) return false;
  t.state = f[0].empty() ? '?' : f[0][0];
  t.minflt = atol(f[7].c_str());
  t.majflt = atol(f[9].c_str());
  t.utime = atol(f[11].c_str());
  t.stime = atol(f[12].c_str());
  t.prio = atol(f[15].c_str());
  t.nice = atol(f[16].c_str());
  t.cpu = atoi(f[36].c_str());  // campo 39
  t.ok = true;
  return true;
}

ThreadStat ReadThread(int tid, bool full) {
  ThreadStat t;
  const std::string base = "/proc/self/task/" + std::to_string(tid);
  if (!ParseStat(ReadFile(base + "/stat", 2048), t)) return t;
  if (!full) return t;
  const std::string ss = ReadFile(base + "/schedstat", 256);
  if (!ss.empty()) sscanf(ss.c_str(), "%llu %llu %llu", &t.run_ns, &t.wait_ns, &t.slices);
  const std::string st = ReadFile(base + "/status", 8192);
  auto field = [&](const char* name) -> std::string {
    const size_t p = st.find(name);
    if (p == std::string::npos) return {};
    size_t a = p + strlen(name);
    while (a < st.size() && (st[a] == ' ' || st[a] == '\t')) ++a;
    const size_t b = st.find('\n', a);
    return st.substr(a, b == std::string::npos ? std::string::npos : b - a);
  };
  t.vcsw = atol(field("voluntary_ctxt_switches:").c_str());
  t.nvcsw = atol(field("nonvoluntary_ctxt_switches:").c_str());
  t.allowed = field("Cpus_allowed_list:");
  return t;
}

std::vector<int> Tids() {
  std::vector<int> out;
  if (DIR* d = opendir("/proc/self/task")) {
    while (dirent* e = readdir(d)) {
      const int tid = atoi(e->d_name);
      if (tid > 0) out.push_back(tid);
    }
    closedir(d);
  }
  return out;
}

bool Sampleable(const std::string& comm) {
  return comm.rfind("XThread", 0) == 0 || comm.rfind("Main XThread", 0) == 0 || comm.rfind("Havok", 0) == 0 ||
         comm.rfind("CriMana", 0) == 0;
}

// Modulos executaveis: nome, build-id, intervalos do texto (enderecos virtuais do ELF = pc - bias).
struct Module {
  std::string name, build_id;
  uint64_t bias = 0, lo = 0, hi = 0;
};

int PhdrCb(dl_phdr_info* info, size_t, void* data) {
  auto* mods = static_cast<std::vector<Module>*>(data);
  Module m;
  const char* path = info->dlpi_name && info->dlpi_name[0] ? info->dlpi_name : "[exe]";
  const char* slash = strrchr(path, '/');
  m.name = slash ? slash + 1 : path;
  m.bias = info->dlpi_addr;
  m.lo = UINT64_MAX;
  for (int i = 0; i < info->dlpi_phnum; ++i) {
    const ElfW(Phdr)& ph = info->dlpi_phdr[i];
    if (ph.p_type == PT_LOAD && (ph.p_flags & PF_X)) {
      m.lo = std::min<uint64_t>(m.lo, info->dlpi_addr + ph.p_vaddr);
      m.hi = std::max<uint64_t>(m.hi, info->dlpi_addr + ph.p_vaddr + ph.p_memsz);
    }
    if (ph.p_type == PT_NOTE) {
      const uint8_t* p = reinterpret_cast<const uint8_t*>(info->dlpi_addr + ph.p_vaddr);
      const uint8_t* end = p + ph.p_memsz;
      while (p + 12 <= end) {
        uint32_t namesz, descsz, type;
        memcpy(&namesz, p, 4); memcpy(&descsz, p + 4, 4); memcpy(&type, p + 8, 4);
        const uint8_t* name = p + 12;
        const uint8_t* desc = name + ((namesz + 3) & ~3u);
        if (desc + descsz > end) break;
        if (type == 3 /* NT_GNU_BUILD_ID */ && namesz == 4 && memcmp(name, "GNU", 4) == 0) {
          static const char* hx = "0123456789abcdef";
          for (uint32_t k = 0; k < descsz; ++k) { m.build_id += hx[desc[k] >> 4]; m.build_id += hx[desc[k] & 15]; }
        }
        p = desc + ((descsz + 3) & ~3u);
      }
    }
  }
  if (m.lo != UINT64_MAX) mods->push_back(m);
  return 0;
}

class Profiler {
 public:
  explicit Profiler(std::string dir) : dir_(std::move(dir)) {}

  void Run() {
    mkdir(dir_.c_str(), 0755);
    struct sigaction old {};
    sigaction(SIGPROF, nullptr, &old);
    sampling_ = old.sa_handler == SIG_DFL || old.sa_handler == SIG_IGN;
    if (sampling_) {
      struct sigaction sa {};
      sa.sa_sigaction = Handler;
      sa.sa_flags = SA_SIGINFO | SA_RESTART;
      sigemptyset(&sa.sa_mask);
      sampling_ = sigaction(SIGPROF, &sa, nullptr) == 0;
    }
    threads_ = fopen((dir_ + "/threads.csv").c_str(), "we");
    freq_ = fopen((dir_ + "/cpufreq.csv").c_str(), "we");
    if (threads_)
      fputs("t_ms,tid,comm,state,utime,stime,run_ns,wait_ns,slices,vcsw,nvcsw,minflt,majflt,cpu,prio,nice,allowed\n",
            threads_);
    ncpu_ = 0;
    while (ncpu_ < 32 && access(("/sys/devices/system/cpu/cpu" + std::to_string(ncpu_)).c_str(), F_OK) == 0) ++ncpu_;
    if (freq_) {
      fputs("t_ms,kind", freq_);
      for (int c = 0; c < ncpu_; ++c) fprintf(freq_, ",cpu%d", c);
      fputc('\n', freq_);
    }
    WriteProcess("start");
    double next_threads = 0, next_freq = 0, next_targets = 0, next_dump = 0, next_mods = 0;
    double sched_b_at = 0;
    for (;;) {
      const double now = WallMs();
      if (now >= next_mods) { RefreshModules(); next_mods = now + 3000; }
      if (now >= next_freq) { WriteFreq(now, next_threads <= now); next_freq = now + 100; }
      if (now >= next_threads) { WriteThreads(now); next_threads = now + 1000; }
      if (now >= next_targets) { PickTargets(); next_targets = now + 2000; }
      if (!go_ && access((dir_ + "/go").c_str(), F_OK) == 0) {
        go_ = true;
        go_ms_ = now;
        agg_.clear();
        samples_ = 0;
        undelivered_ = 0;
        WriteProcess("go");
        DumpSched("a");
        sched_b_at = now + 40000;
      }
      if (sched_b_at && now >= sched_b_at) { DumpSched("b"); sched_b_at = 0; }
      if (sampling_) SampleOnce();
      if (now >= next_dump) { DumpStacks(); next_dump = now + 5000; }
      usleep(sampling_ ? kSampleUs : 50000);
    }
  }

 private:
  void WriteProcess(const char* when) {
    std::string s = std::string("when ") + when + " t_ms " + std::to_string(long(WallMs())) + "\n";
    s += "cpuset " + ReadFile("/proc/self/cpuset", 256);
    const std::string st = ReadFile("/proc/self/status", 8192);
    for (const char* k : {"Cpus_allowed_list:", "Threads:", "VmRSS:"}) {
      const size_t p = st.find(k);
      if (p != std::string::npos) s += st.substr(p, st.find('\n', p) - p + 1);
    }
    s += "sampling " + std::string(sampling_ ? "on" : "off (SIGPROF ocupado)") + "\n";
    for (int c = 0; c < ncpu_; ++c) {
      const std::string b = "/sys/devices/system/cpu/cpu" + std::to_string(c) + "/cpufreq/";
      s += "cpu" + std::to_string(c) + " max " + Trim(ReadFile(b + "cpuinfo_max_freq", 64)) + " governor " +
           Trim(ReadFile(b + "scaling_governor", 64)) + " online " +
           Trim(ReadFile("/sys/devices/system/cpu/cpu" + std::to_string(c) + "/online", 8)) + "\n";
    }
    for (const Module& m : BigModules())
        s += "module " + m.name + " " + (m.build_id.empty() ? "-" : m.build_id) + " text " +
             std::to_string((m.hi - m.lo) >> 10) + " KB\n";
    WriteText(dir_ + "/process_" + when + ".txt", s);
  }

  static std::string Trim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == ' ')) s.pop_back();
    return s.empty() ? "?" : s;
  }

  void RefreshModules() {
    std::vector<Module> m;
    dl_iterate_phdr(PhdrCb, &m);
    std::sort(m.begin(), m.end(), [](const Module& a, const Module& b) { return a.lo < b.lo; });
    uint64_t best = 0;
    for (const Module& x : m)
      if (x.hi - x.lo > best) { best = x.hi - x.lo; g_text_lo = x.lo; g_text_hi = x.hi; }
    mods_ = std::move(m);
  }

  // Os modulos que aparecem nas pilhas do jogo: os do app, o driver Vulkan, a libc.
  std::vector<Module> BigModules() const {
    std::vector<Module> out;
    std::set<std::string> seen;
    for (const Module& m : mods_) {
      const bool ours = m.name.rfind("lib", 0) == 0 &&
                        (m.name == "libgame.so" || m.name == "librexruntime.so" || m.name == "libSDL3.so" ||
                         m.name == "libmain.so" || m.name == "libzdiag.so" || m.name == "libc++_shared.so" ||
                         m.name == "libc.so" || m.name == "libm.so");
      const bool driver = m.name.find("vulkan") != std::string::npos || m.name.find("GLES") != std::string::npos ||
                          m.name.find("mali") != std::string::npos || m.name.find("gsl") != std::string::npos;
      if ((ours || driver) && seen.insert(m.name + m.build_id).second) out.push_back(m);
    }
    return out;
  }

  std::string Symbol(uint64_t a) const {
    // Busca binaria pelo modulo; "nome+0xvaddr".
    auto it = std::upper_bound(mods_.begin(), mods_.end(), a, [](uint64_t v, const Module& m) { return v < m.lo; });
    if (it != mods_.begin()) {
      --it;
      if (a < it->hi) {
        char buf[160];
        snprintf(buf, sizeof buf, "%s+0x%llx", it->name.c_str(), (unsigned long long)(a - it->bias));
        return buf;
      }
    }
    return "?";
  }

  void WriteFreq(double now, bool with_max) {
    if (!freq_) return;
    fprintf(freq_, "%ld,cur", long(now));
    for (int c = 0; c < ncpu_; ++c)
      fprintf(freq_, ",%s",
              Trim(ReadFile("/sys/devices/system/cpu/cpu" + std::to_string(c) + "/cpufreq/scaling_cur_freq", 32))
                  .c_str());
    fputc('\n', freq_);
    if (with_max) {
      fprintf(freq_, "%ld,max", long(now));
      for (int c = 0; c < ncpu_; ++c)
        fprintf(freq_, ",%s",
                Trim(ReadFile("/sys/devices/system/cpu/cpu" + std::to_string(c) + "/cpufreq/scaling_max_freq", 32))
                    .c_str());
      fputc('\n', freq_);
    }
    fflush(freq_);
  }

  void WriteThreads(double now) {
    if (!threads_) return;
    for (int tid : Tids()) {
      ThreadStat t = ReadThread(tid, true);
      if (!t.ok) continue;
      const long ticks = t.utime + t.stime;
      auto& last = last_ticks_[tid];
      // Threads paradas desde a ultima linha ficam de fora (a soma nao muda).
      if (last.first && ticks == last.second && t.vcsw + t.nvcsw == last_sw_[tid]) continue;
      last = {true, ticks};
      last_sw_[tid] = t.vcsw + t.nvcsw;
      std::string comm = t.comm;
      std::replace(comm.begin(), comm.end(), ',', ' ');
      fprintf(threads_, "%ld,%d,%s,%c,%ld,%ld,%llu,%llu,%llu,%ld,%ld,%ld,%ld,%d,%ld,%ld,\"%s\"\n", long(now), tid,
              comm.c_str(), t.state, t.utime, t.stime, t.run_ns, t.wait_ns, t.slices, t.vcsw, t.nvcsw, t.minflt,
              t.majflt, t.cpu, t.prio, t.nice, t.allowed.c_str());
      cpu_delta_[tid] = {comm, ticks};
    }
    fflush(threads_);
  }

  void PickTargets() {
    // As threads do guest que mais CPU gastaram nos ultimos 2 s, e sempre as "Main XThread".
    std::vector<std::pair<long, int>> busy;
    for (auto& [tid, e] : cpu_delta_) {
      const long d = e.second - prev_pick_[tid];
      prev_pick_[tid] = e.second;
      if (Sampleable(e.first)) busy.push_back({e.first.rfind("Main XThread", 0) == 0 ? 1000000 + d : d, tid});
    }
    std::sort(busy.rbegin(), busy.rend());
    targets_.clear();
    for (auto& [d, tid] : busy) {
      if (int(targets_.size()) >= kTargets) break;
      if (d > 0) targets_.push_back(tid);
    }
    // Os limites das pilhas (o bionic da nome ao mapeamento de cada thread).
    stacks_.clear();
    if (FILE* f = fopen("/proc/self/maps", "re")) {
      char line[512];
      while (fgets(line, sizeof line, f)) {
        const char* tag = strstr(line, "[anon:stack_and_tls:");
        if (!tag) continue;
        unsigned long long lo, hi;
        char perms[8] = {};
        if (sscanf(line, "%llx-%llx %7s", &lo, &hi, perms) != 3 || perms[0] != 'r' || perms[1] != 'w') continue;
        stacks_[atoi(tag + 20)] = {lo, hi};
      }
      fclose(f);
    }
  }

  void SampleOnce() {
    for (int tid : targets_) {
      ThreadStat t = ReadThread(tid, false);
      if (!t.ok) continue;
      const uint32_t seq = ++seq_;
      Slot& s = g_slots[seq % kSlots];
      s.seq.store(0, std::memory_order_relaxed);
      const auto st = stacks_.find(tid);
      s.stack_lo = st == stacks_.end() ? 0 : st->second.first;
      s.stack_hi = st == stacks_.end() ? 0 : st->second.second;
      std::atomic_thread_fence(std::memory_order_release);
      siginfo_t info {};
      info.si_signo = SIGPROF;
      info.si_code = SI_QUEUE;
      info.si_pid = getpid();
      info.si_uid = getuid();
      info.si_value.sival_int = int(seq);
      if (syscall(__NR_rt_tgsigqueueinfo, getpid(), tid, SIGPROF, &info) != 0) continue;
      // Ate 3 ms pelo handler: se a thread nao corre nesse tempo, esta na fila (conta-se a parte).
      bool got = false;
      for (int i = 0; i < 60; ++i) {
        if (s.seq.load(std::memory_order_acquire) == seq) { got = true; break; }
        usleep(50);
      }
      const char cls = t.state == 'R' ? 'R' : t.state == 'D' ? 'D' : 'S';
      std::string key;
      if (got) {
        for (int i = 0; i < s.n; ++i) key += (i ? ";" : "") + Symbol(s.frames[i]);
        if (s.nscan) {
          key += " |scan";
          for (int i = 0; i < s.nscan; ++i) key += ";" + Symbol(s.scan[i]);
        }
      } else {
        key = "(sinal nao entregue em 3 ms)";
        ++undelivered_;
      }
      auto& per = agg_[tid];
      if (per.comm.empty()) per.comm = t.comm;
      per.cores[t.cpu]++;
      per.states[cls]++;
      if (per.stacks.size() < 4000 || per.stacks.count({cls, key})) per.stacks[{cls, key}]++;
      ++samples_;
    }
  }

  void DumpStacks() {
    std::string s = "go " + std::string(go_ ? "sim" : "nao") + " desde_ms " + std::to_string(long(go_ms_)) +
                    " amostras " + std::to_string(samples_) + " nao_entregues " + std::to_string(undelivered_) + "\n";
    for (const Module& m : BigModules()) s += "module " + m.name + " " + (m.build_id.empty() ? "-" : m.build_id) + "\n";
    for (auto& [tid, per] : agg_) {
      s += "thread " + std::to_string(tid) + " " + per.comm + " estados";
      for (auto& [c, n] : per.states) s += std::string(" ") + c + "=" + std::to_string(n);
      s += " nucleos";
      for (auto& [c, n] : per.cores) s += " " + std::to_string(c) + "=" + std::to_string(n);
      s += "\n";
      std::vector<std::pair<long, const std::pair<char, std::string>*>> v;
      for (auto& [k, n] : per.stacks) v.push_back({n, &k});
      std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.first > b.first; });
      for (size_t i = 0; i < v.size() && i < 300; ++i)
        s += "  " + std::to_string(v[i].first) + " " + v[i].second->first + " " + v[i].second->second + "\n";
    }
    WriteText(dir_ + "/stacks.txt", s);
  }

  void DumpSched(const char* tag) {
    for (auto& [tid, e] : cpu_delta_) {
      if (e.first.rfind("Main XThread", 0) != 0) continue;
      const std::string s = ReadFile("/proc/self/task/" + std::to_string(tid) + "/sched", 16384);
      WriteText(dir_ + "/sched_" + std::to_string(tid) + "_" + tag + ".txt", s.empty() ? "(sem /sched)\n" : s);
    }
  }

  struct Agg {
    std::string comm;
    std::map<int, long> cores;
    std::map<char, long> states;
    std::map<std::pair<char, std::string>, long> stacks;
  };

  std::string dir_;
  FILE* threads_ = nullptr;
  FILE* freq_ = nullptr;
  int ncpu_ = 0;
  bool sampling_ = false, go_ = false;
  double go_ms_ = 0;
  uint32_t seq_ = 0;
  long samples_ = 0, undelivered_ = 0;
  std::vector<Module> mods_;
  std::vector<int> targets_;
  std::unordered_map<int, std::pair<uint64_t, uint64_t>> stacks_;
  std::unordered_map<int, std::pair<bool, long>> last_ticks_;
  std::unordered_map<int, long> last_sw_;
  std::unordered_map<int, std::pair<std::string, long>> cpu_delta_;
  std::unordered_map<int, long> prev_pick_;
  std::map<int, Agg> agg_;
};

}  // namespace

bool StartProfiler(const std::string& dir) {
  if (g_started.exchange(true)) return false;
  std::thread([dir] {
    pthread_setname_np(pthread_self(), "zdiag-prof");
    Profiler(dir).Run();
  }).detach();
  return true;
}

}  // namespace zdiag
