// Project SwordMan - esperas ativas do MGR na CPU (limite de ~20 ms de Swap a Swap, 23/10).
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
