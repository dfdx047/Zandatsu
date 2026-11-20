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
