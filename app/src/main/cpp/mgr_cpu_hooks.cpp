// Zandatsu - esperas ativas do MGR na CPU (limite de ~20 ms de Swap a Swap, 23/10).
//
// sub_828BED28: o lado de escrita de um lock leitor/escritor do jogo, usado pelos workers do escalonador de tarefas.
// Entra numa secção crítica (obj+8, se obj+36 != 0) e espera que o contador de leitores (obj+0) chegue a 0 num laço
// cru (lwz/cmpwi/bgt; o lwsync do original nem chega ao código gerado). No RP5 eram 4,5% do tempo dos workers
// (simpleperf na luta do fight-bench). Com swordman_rwlock_wait o laço passa a esperar de verdade: algumas voltas com
// a dica "yield" do ARM e depois sched_yield, para não tirar o núcleo a quem tem de largar o lock.

#include <sched.h>

#include <atomic>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc.h>

REXCVAR_DEFINE_BOOL(swordman_rwlock_wait, false, "SwordMan",
                    "sub_828BED28 (lock leitor/escritor do MGR): espera com yield em vez do laço cru do jogo")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REX_EXTERN(__imp__RtlEnterCriticalSection);
REX_EXTERN(__imp__sub_828BED28);

REX_HOOK_RAW(sub_828BED28) {
  static const bool wait = REXCVAR_GET(swordman_rwlock_wait);
  if (!wait) {
    __imp__sub_828BED28(ctx, base);
    return;
  }
  const uint32_t obj = ctx.r3.u32;
  const auto load = [base](uint32_t address) {
    return int32_t(__builtin_bswap32(
        __atomic_load_n(reinterpret_cast<const uint32_t*>(base + address), __ATOMIC_ACQUIRE)));
  };
  if (load(obj + 36) != 0) {
    // Como o original: r3 = obj + 8, e o lr de volta ao ponto a seguir à chamada.
    const uint64_t lr = ctx.lr;
    ctx.r3.u64 = obj + 8;
    ctx.lr = 0x828BED50;
    __imp__RtlEnterCriticalSection(ctx, base);
    ctx.lr = lr;
  }
  for (uint32_t round = 0; load(obj) > 0; ++round) {
    if (round < 64) {
      __asm__ volatile("yield");
    } else {
      sched_yield();
    }
  }
}

// sub_828BE6C0 (12/11, POCO X6 Pro): o fork/join do escalonador de tarefas do MGR. r3 = o grupo de workers, r4 = em
// quantas fatias repartir (limitado ao numero de workers em obj+4; fica em obj+12). obj+0 aponta para as entradas dos
// workers, 24 bytes cada: +8 semaforo de comecar, +12 semaforo de fim, +16 funcao, +20 argumento da fatia. O original
// marca o grupo ocupado (obj+16, CAS; se ja estava, volta sem fazer nada), solta o semaforo de cada worker 1..n-1,
// corre a fatia 0 (funcao(0, argumento) da entrada 0) e espera o fim de cada um. O worker (sub_828BF158) faz o mesmo
// com a sua entrada. Na luta sao ~330 fork/join por frame com ~10 us de trabalho por fatia: no X6 cada worker acordava
// ~19 500 vezes por segundo e esperava ~14 us por um nucleo a cada vez (perfetto, sched), e os semaforos eram ~23% da
// CPU do jogo. Com swordman_job_serial as fatias correm todas, por ordem, na thread que pede o job: sem acordar ninguem.
// Cortar fatias (so a 0) perde trabalho: o jogo nem chega as logos.
REXCVAR_DEFINE_BOOL(swordman_job_serial, false, "SwordMan",
                    "Jobs do escalonador do MGR (sub_828BE6C0) corridos todos na thread que os pede, sem acordar os "
                    "workers")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REX_EXTERN(__imp__sub_828BE6C0);
REX_EXTERN(__imp__sub_828BF158);

namespace {
// r13 (os dados por thread do jogo) de cada worker, pela sua entrada: gravado quando o worker arranca (sub_828BF158).
// 13/52: sem isto, em serie, o jogo leu um ponteiro nulo + 0xD0 ao entrar na luta.
struct WorkerR13 {
  std::atomic<uint32_t> entry{0};
  uint64_t r13 = 0;
};
WorkerR13 g_worker_r13[64];

bool FindWorkerR13(uint32_t entry, uint64_t* r13) {
  for (auto& w : g_worker_r13) {
    const uint32_t e = w.entry.load(std::memory_order_acquire);
    if (e == 0) return false;
    if (e == entry) {
      *r13 = w.r13;
      return true;
    }
  }
  return false;
}
}  // namespace

// O laco de cada worker (r3 = a sua entrada): fica com o r13 dele.
REX_HOOK_RAW(sub_828BF158) {
  const uint32_t entry = ctx.r3.u32;
  for (auto& w : g_worker_r13) {
    uint32_t expected = 0;
    if (w.entry.load(std::memory_order_acquire) == entry) break;
    if (w.entry.load(std::memory_order_acquire) != 0) continue;
    w.r13 = ctx.r13.u64;
    if (w.entry.compare_exchange_strong(expected, entry, std::memory_order_acq_rel)) break;
  }
  __imp__sub_828BF158(ctx, base);
}

REX_HOOK_RAW(sub_828BE6C0) {
  // 12/11: a launcher procura "job serial ligado" no log para o desligar depois de um travamento (JobSerialGuard.kt).
  static const bool serial = [] {
    const bool on = REXCVAR_GET(swordman_job_serial);
    REXLOG_INFO("swordman: job serial {}", on ? "ligado" : "desligado");
    return on;
  }();
  if (!serial) {
    __imp__sub_828BE6C0(ctx, base);
    return;
  }
  const uint32_t obj = ctx.r3.u32;
  const auto load = [base](uint32_t address) {
    return __builtin_bswap32(__atomic_load_n(reinterpret_cast<const uint32_t*>(base + address), __ATOMIC_ACQUIRE));
  };
  const auto store = [base](uint32_t address, uint32_t value) {
    __atomic_store_n(reinterpret_cast<uint32_t*>(base + address), __builtin_bswap32(value), __ATOMIC_RELEASE);
  };
  auto* busy = reinterpret_cast<uint32_t*>(base + obj + 16);
  uint32_t expected = 0;
  if (!__atomic_compare_exchange_n(busy, &expected, __builtin_bswap32(1u), false, __ATOMIC_ACQ_REL,
                                   __ATOMIC_ACQUIRE)) {
    // Ocupado: como o original (volta sem nada). Contado no log, para ver se a serie o torna mais frequente.
    static std::atomic<uint32_t> busy_count{0};
    const uint32_t c = busy_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (c <= 20 || (c & (c - 1)) == 0)
      REXLOG_WARN("swordman: job serial: grupo {:08X} ocupado ({} vezes), r4={} lr={:08X}", obj, c, ctx.r4.s32,
                  uint32_t(ctx.lr));
    __imp__sub_828BE6C0(ctx, base);
    return;
  }
  int32_t slices = ctx.r4.s32;
  const int32_t workers = int32_t(load(obj + 4));
  if (slices > workers) slices = workers;
  if (slices < 1) slices = 1;
  const uint32_t entries = load(obj + 0);
  uint64_t r13s[64];
  for (int32_t i = 1; i < slices; ++i) {
    if (i >= 64 || !FindWorkerR13(entries + 24u * uint32_t(i), &r13s[i])) {
      store(obj + 16, 0);  // worker ainda sem r13 conhecido: como o original
      __imp__sub_828BE6C0(ctx, base);
      return;
    }
  }
  store(obj + 12, uint32_t(slices));
  const uint64_t lr = ctx.lr, r13 = ctx.r13.u64;
  for (int32_t i = 0; i < slices; ++i) {
    const uint32_t entry = entries + 24u * uint32_t(i);
    const uint32_t func = load(entry + 16);
    ctx.r3.u64 = 0;
    ctx.r4.u64 = load(entry + 20);
    ctx.r13.u64 = i ? r13s[i] : r13;
    ctx.lr = 0x828BE7A0;
    rex::runtime::ResolveIndirectFunction(func)(ctx, base);
  }
  ctx.r13.u64 = r13;
  ctx.lr = lr;
  store(obj + 12, 0);
  store(obj + 16, 0);
}
