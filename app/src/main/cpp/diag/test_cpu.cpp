// zdiag: cpu_cores - a velocidade real de cada nucleo (teste de GPU, 04/11).
//
// Por nucleo, com a thread presa a ele: uma cadeia de somas dependentes (1 ciclo cada: da a frequencia efetiva), a
// latencia de memoria (ponteiros aleatorios em 32 MB) e um fluxo de leituras com troca de bytes (o que o codigo
// recompilado do Xbox 360 faz a toda a hora). Mais a frequencia que o kernel diz, o limite e a mascara do processo.
// Um nucleo que o cpuset nao deixa usar fica "recusado".
#include <sched.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <condition_variable>
#include <ctime>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "zdiag.h"

namespace zdiag {
namespace {

std::string Read(const std::string& p) {
  std::string out;
  if (FILE* f = fopen(p.c_str(), "re")) {
    char buf[512];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = 0;
    out = buf;
    fclose(f);
  }
  while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) out.pop_back();
  return out;
}

long Num(const std::string& p) { return atol(Read(p).c_str()); }

// 100 mil somas dependentes entre leituras do relogio (no RP5 o clock_gettime custa o mesmo que centenas de somas).
double AddChainNs(double ms) {
  const double t0 = NowMs();
  uint64_t x = 1, iters = 0;
  while (NowMs() - t0 < ms) {
    for (int i = 0; i < 25000; ++i) {
#if defined(__aarch64__)
      asm volatile("add %0, %0, #1\n add %0, %0, #1\n add %0, %0, #1\n add %0, %0, #1\n" : "+r"(x));
#else
      x += 4;
      asm volatile("" : "+r"(x));
#endif
    }
    iters += 100000;
  }
  const double el = NowMs() - t0;
  asm volatile("" ::"r"(x));
  return el * 1e6 / double(iters);
}

double ChaseNs(const std::vector<uint32_t>& next, double ms) {
  const double t0 = NowMs();
  uint32_t p = 0;
  uint64_t n = 0;
  while (NowMs() - t0 < ms) {
    for (int i = 0; i < 4096; ++i) p = next[p];
    n += 4096;
  }
  const double el = NowMs() - t0;
  asm volatile("" ::"r"(p));
  return el * 1e6 / double(n);
}

double SwapStreamNs(const std::vector<uint32_t>& buf, double ms) {
  const double t0 = NowMs();
  uint64_t acc = 0, n = 0;
  while (NowMs() - t0 < ms) {
    for (size_t i = 0; i < buf.size(); i += 4) {
      acc += __builtin_bswap32(buf[i]) ^ __builtin_bswap32(buf[i + 1]);
      acc += __builtin_bswap32(buf[i + 2]) + __builtin_bswap32(buf[i + 3]);
    }
    n += buf.size();
  }
  const double el = NowMs() - t0;
  asm volatile("" ::"r"(acc));
  return el * 1e6 / double(n);
}

bool Pin(int c) {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(c, &set);
  return sched_setaffinity(0, sizeof set, &set) == 0;
}

double Pct(std::vector<double> v, double p) {
  if (v.empty()) return -1;
  std::sort(v.begin(), v.end());
  return v[std::min(v.size() - 1, size_t(p * double(v.size())))];
}

// Ida e volta entre duas threads (mutex + condition_variable, como o rex::thread::PosixCondition): a do nucleo a
// acorda a do b e espera que ela responda. Devolve as voltas em microssegundos (vazio se um nucleo recusar ou se
// uma resposta levar mais de 100 ms).
std::vector<double> PingPong(int a, int b, int rounds) {
  std::mutex mu;
  std::condition_variable cv;
  int pings = 0, pongs = 0;  // o b responde a cada ping com um pong
  bool stop = false;
  std::atomic<bool> ready{false}, b_ok{true};
  std::vector<double> us;
  us.reserve(rounds);
  std::thread other([&] {
    if (!Pin(b)) b_ok = false;
    ready = true;
    std::unique_lock<std::mutex> l(mu);
    while (!stop) {
      if (pongs < pings) {
        pongs = pings;
        cv.notify_all();
        continue;
      }
      cv.wait_for(l, std::chrono::milliseconds(200));
    }
  });
  bool ok = Pin(a);
  while (!ready) std::this_thread::yield();
  ok = ok && b_ok;
  for (int i = 0; i < rounds && ok; ++i) {
    // As threads do jogo dormem entre os sinais: deixar a outra adormecer antes de a acordar.
    std::this_thread::sleep_for(std::chrono::microseconds(200));
    const double t0 = NowMs();
    std::unique_lock<std::mutex> l(mu);
    const int want = ++pings;
    cv.notify_all();
    if (!cv.wait_for(l, std::chrono::milliseconds(100), [&] { return pongs >= want; })) ok = false;
    else us.push_back((NowMs() - t0) * 1000.0);
  }
  {
    std::lock_guard<std::mutex> l(mu);
    stop = true;
    cv.notify_all();
  }
  other.join();
  return ok ? us : std::vector<double>{};
}

// Quanto um sleep de 1 ms passa do pedido (o KeDelayExecutionThread e o PaceSwap dependem disto).
std::vector<double> SleepOver(int rounds) {
  std::vector<double> us;
  for (int i = 0; i < rounds; ++i) {
    timespec ts{0, 1000000};
    const double t0 = NowMs();
    nanosleep(&ts, nullptr);
    us.push_back((NowMs() - t0) * 1000.0 - 1000.0);
  }
  return us;
}

}  // namespace

void TestCpuCores(Test& t) {
  const int ncpu = int(sysconf(_SC_NPROCESSORS_CONF));
  t.details.k("cpuset").v(Read("/proc/self/cpuset"));
  {
    FILE* f = fopen("/proc/self/status", "re");
    char line[256];
    std::string allowed;
    while (f && fgets(line, sizeof line, f))
      if (!strncmp(line, "Cpus_allowed_list:", 18)) allowed = line + 18;
    if (f) fclose(f);
    while (!allowed.empty() && (allowed.back() == '\n' || allowed.front() == '\t' || allowed.front() == ' ')) {
      if (allowed.back() == '\n') allowed.pop_back(); else allowed.erase(0, 1);
    }
    t.details.k("cpusAllowed").v(allowed);
  }
  // Ponteiros aleatorios numa linha de cache cada (64 bytes = 16 uint32), 32 MB.
  const size_t lines = (32u << 20) / 64;
  std::vector<uint32_t> next(lines * 16, 0);
  {
    std::vector<uint32_t> order(lines);
    for (size_t i = 0; i < lines; ++i) order[i] = uint32_t(i);
    std::mt19937 rng(42);
    std::shuffle(order.begin() + 1, order.end(), rng);
    for (size_t i = 0; i < lines; ++i) next[order[i] * 16] = order[(i + 1) % lines] * 16;
  }
  std::vector<uint32_t> stream((8u << 20) / 4);
  for (size_t i = 0; i < stream.size(); ++i) stream[i] = uint32_t(i * 2654435761u);

  // Corre numa thread do app: a mascara original volta no fim.
  cpu_set_t orig;
  CPU_ZERO(&orig);
  const bool have_orig = sched_getaffinity(0, sizeof orig, &orig) == 0;
  t.details.k("cores").arr();
  std::string summary;
  double best_ghz = 0;
  for (int c = 0; c < ncpu; ++c) {
    const std::string base = "/sys/devices/system/cpu/cpu" + std::to_string(c);
    t.Step("core" + std::to_string(c), [&](Json& info) {
      info.kv("cpu", c).kv("online", Read(base + "/online").empty() ? std::string("?") : Read(base + "/online"));
      info.kv("maxKHz", Num(base + "/cpufreq/cpuinfo_max_freq")).kv("capKHz", Num(base + "/cpufreq/scaling_max_freq"));
      info.kv("governor", Read(base + "/cpufreq/scaling_governor"));
      cpu_set_t set;
      CPU_ZERO(&set);
      CPU_SET(c, &set);
      if (sched_setaffinity(0, sizeof set, &set) != 0) {
        info.kv("refused", std::string(strerror(errno)));
        return St::Skip;
      }
      AddChainNs(40);  // o governador sobe a frequencia
      const long cur0 = Num(base + "/cpufreq/scaling_cur_freq");
      const double add_ns = AddChainNs(40);
      const double chase_ns = ChaseNs(next, 40);
      const double swap_ns = SwapStreamNs(stream, 40);
      const long cur1 = Num(base + "/cpufreq/scaling_cur_freq");
      int now_cpu = sched_getcpu();
      const double ghz = add_ns > 0 ? 1.0 / add_ns : 0;
      best_ghz = std::max(best_ghz, ghz);
      info.kv("ranOn", now_cpu).kv("curKHzBefore", cur0).kv("curKHzAfter", cur1);
      info.kv("addNs", add_ns).kv("effectiveGHz", ghz).kv("memLatencyNs", chase_ns).kv("bswapStreamNsPerWord", swap_ns);
      t.details.obj().kv("cpu", c).kv("effectiveGHz", ghz).kv("maxKHz", Num(base + "/cpufreq/cpuinfo_max_freq"))
          .kv("capKHz", Num(base + "/cpufreq/scaling_max_freq")).kv("curKHz", cur1).kv("memLatencyNs", chase_ns)
          .kv("bswapStreamNsPerWord", swap_ns).end();
      char b[96];
      snprintf(b, sizeof b, "%s%d:%.2fGHz/%.0fns", summary.empty() ? "" : " ", c, ghz, chase_ns);
      summary += b;
      return St::Pass;
    });
  }
  t.details.end();

  // Acordar entre nucleos: todos os pares (a acorda b), 150 voltas cada.
  t.details.k("wakeRoundTripUs").arr();
  std::string worst;
  double worst_p50 = 0;
  t.Step("wake_latency", [&](Json& info) {
    int pairs = 0;
    for (int a = 0; a < ncpu; ++a)
      for (int b = 0; b < ncpu; ++b) {
        if (a == b) continue;
        std::vector<double> us = PingPong(a, b, 150);
        if (us.empty()) continue;
        const double p50 = Pct(us, 0.5), p95 = Pct(us, 0.95), mx = Pct(us, 1.0);
        t.details.obj().kv("a", a).kv("b", b).kv("p50", p50).kv("p95", p95).kv("max", mx).end();
        ++pairs;
        if (p50 > worst_p50) {
          worst_p50 = p50;
          char w[64];
          snprintf(w, sizeof w, "%d->%d %.0f us", a, b, p50);
          worst = w;
        }
      }
    info.kv("pairs", pairs).kv("worst", worst);
    return pairs ? St::Pass : St::Skip;
  });
  t.details.end();

  t.details.k("sleep1msOverUs").arr();
  std::string sleeps;
  t.Step("sleep_1ms", [&](Json& info) {
    int n = 0;
    for (int c = 0; c < ncpu; ++c) {
      if (!Pin(c)) continue;
      std::vector<double> us = SleepOver(120);
      t.details.obj().kv("cpu", c).kv("p50", Pct(us, 0.5)).kv("p95", Pct(us, 0.95)).kv("max", Pct(us, 1.0)).end();
      char b[48];
      snprintf(b, sizeof b, "%s%d:+%.0f", sleeps.empty() ? "" : " ", c, Pct(us, 0.5));
      sleeps += b;
      ++n;
    }
    info.kv("summary", sleeps);
    return n ? St::Pass : St::Skip;
  });
  t.details.end();

  if (have_orig) sched_setaffinity(0, sizeof orig, &orig);
  t.message = summary + " | acordar pior " + worst + " | sleep 1 ms +us " + sleeps;
}

}  // namespace zdiag
