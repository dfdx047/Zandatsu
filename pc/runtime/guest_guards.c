/* Guest functions whose lifted body is replaced by a guarded copy (listed in pc/config/overrides.txt; the lift emits a
 * stub that calls recomp_lookup_manual -> overrides.c). Compiled with the lifted code (mgr_guest: same macros/flags). */
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
#include "recomp_funcs.h"
#ifdef __ANDROID__
#include <android/log.h>
#endif

/* 0x00F40EA0 (thiscall): [this+0x1EF8] = 1; this->[0x1E70]->vfunc[0x10](); [this+0x1EF8] = 0.
 * While the CONTINUE save loads, a job (sub_009CEFE0 -> 0x009D5FAE/0x009D6021) calls it before [this+0x1E70] is set:
 * guest NULL read, SEGV (2 of 3 launcher runs on 20/11). Guard: skip the call while the object does not exist. */
void mgr_guard_00F40EA0(void) {
    int _fpu_cmp = 0;
    uint32_t _cf = 0;
    int _df = 1;
    uint32_t _flag_a = 0, _flag_b = 0;
    uint32_t _flag_k = FK_NONE;
    RECOMP_REGS_LOCALS
    RECOMP_ENTER(0x00F40EA0u);
    PUSH32(esp, esi);
    esi = ecx;
    ecx = MEM32(esi + 0x1E70);
    if (ecx) {
        MEM32(esi + 0x1EF8) = 1;
        eax = MEM32(ecx);
        edx = MEM32(eax + 0x10);
        RECOMP_ICALL(edx);
        RECOMP_FLAGS_IN();
        MEM32(esi + 0x1EF8) = 0;
    } else {
#ifdef __ANDROID__
        static unsigned hits;
        if (hits++ < 8) __android_log_print(ANDROID_LOG_WARN, "MGR", "guard 00F40EA0: [0x%08X+0x1E70] is NULL, call skipped", esi);
#endif
    }
    esi = POP32_VAL(esp);
    RECOMP_FLAGS_OUT(); esp += 4; RECOMP_RETURN();
}

/* 0x00F41B10 (thiscall, ret 0x50, eax = 0 on its own early-out path): reads [[this+0x1E70]+0x1688] at 0x00F41CFD. Same
 * race as above (2nd site, 1 of 3 launcher runs after the 00F40EA0 guard, also in the lab on 20/11). Entry guard (lift
 * guards.txt): with no object, return 0 the way the early-out path does. */
int mgr_guard_enter_00F41B10(void) {
    if (MEM32(g_ecx + 0x1E70u)) return 1;
#ifdef __ANDROID__
    static unsigned hits;
    if (hits++ < 8) __android_log_print(ANDROID_LOG_WARN, "MGR", "guard 00F41B10: [0x%08X+0x1E70] is NULL, returns 0", g_ecx);
#endif
    g_eax = 0;
    g_esp += 4 + 0x50;
    return 0;
}

/* 0x009F3EC0 (cdecl, no args, plain ret): per-frame update of the object at 0x01B788C0, called by the main loop
 * (0x00A547F9 in sub_00A54490) unless flag 0x8000000 of [0x01BEA070] is set. While CONTINUE reloads, the main loop
 * runs frames after sub_00A540F0 tore the object down ([0x01B788C0+0x1E70] = 0) and before 0x009EE60A creates it
 * again; the flag is already clear, so 0x00F40ED0 wrote to [NULL+0xC4] (SEGV, 2 of 2 lab runs on 20/11 19:44/19:51).
 * Entry guard: with no object, skip the call exactly as the game does when the flag is set. */
int mgr_guard_enter_009F3EC0(void) {
    if (MEM32(0x01B788C0u + 0x1E70u)) return 1;
#ifdef __ANDROID__
    static unsigned hits;
    if (hits++ < 8) __android_log_print(ANDROID_LOG_WARN, "MGR", "guard 009F3EC0: [0x01B788C0+0x1E70] is NULL, frame update skipped");
#endif
    g_esp += 4;
    return 0;
}

/* 0x00F40ED0 / 0x00F40EF0 (thiscall, ret 4): setters [[this+0x1E70]+0xC4] / [[this+0x1E70]+0xC8] = arg. The same
 * CONTINUE reload window as above reaches them from more callers than 0x009F3EC0: with MGR_MT=1 (faster loading)
 * sub_009D5F00 from the main loop wrote to [NULL+0xC8] (SEGV at 0x00F40EFA, 20/11 21:24). Guard the setters
 * themselves: with no object the store is dropped. */
static int mgr_guard_setter(uint32_t va) {
    if (MEM32(g_ecx + 0x1E70u)) return 1;
#ifdef __ANDROID__
    static unsigned hits;
    if (hits++ < 8) __android_log_print(ANDROID_LOG_WARN, "MGR", "guard %08X: [0x%08X+0x1E70] is NULL, store dropped", va, g_ecx);
#endif
    g_esp += 4 + 4;
    return 0;
}
int mgr_guard_enter_00F40ED0(void) { return mgr_guard_setter(0x00F40ED0u); }
int mgr_guard_enter_00F40EF0(void) { return mgr_guard_setter(0x00F40EF0u); }

/* 0x009D5F00 (cdecl, no args, plain ret): per-frame update that pushes state into the object at [0x01B78870]
 * (0x00F40EF0, 0x00F416F0, 0x00F40F60 all go through [obj+0x1E70]). Called by the main loop (sub_00A4B080) during the
 * CONTINUE reload too; with MGR_MT=1 that ended in 0x00F416F0 calling through a NULL vtable (21:32). With no object,
 * do what the game does when flag 0x8000000 of [0x01BEA09C] is set: update bit 0x40000 of [0x01BEA084] and return. */
int mgr_guard_enter_009D5F00(void) {
    uint32_t obj = MEM32(0x01B78870u);
    if (obj && MEM32(obj + 0x1E70u)) return 1;
    if (MEM32(0x01B78868u)) MEM32(0x01BEA084u) |= 0x00040000u; else MEM32(0x01BEA084u) &= ~0x00040000u;
#ifdef __ANDROID__
    static unsigned hits;
    if (hits++ < 8) __android_log_print(ANDROID_LOG_WARN, "MGR", "guard 009D5F00: [0x%08X+0x1E70] is NULL, update skipped", obj);
#endif
    g_esp += 4;
    return 0;
}
