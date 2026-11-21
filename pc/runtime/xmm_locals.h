/* xmm_locals.h: included by lifted files that pc/tools/xmm_locals.py rewrote (SSE registers as C locals).
 * RECOMP_GPR_OUT/IN are the pinned runtime's own RECOMP_REGS_OUT/IN (recomp_types.h, RECOMP_LOCAL_REGS); the rewritten
 * functions redefine RECOMP_REGS_OUT/IN as these plus their own XMM write-back/reload, and restore them after. Keep the
 * two in step if the pinned runtime ever changes which registers are locals. */
#ifndef MGR_XMM_LOCALS_H
#define MGR_XMM_LOCALS_H
#if defined(RECOMP_GENERATED_CODE) && defined(RECOMP_LOCAL_REGS)
#define RECOMP_GPR_OUT() (g_eax = l_eax, g_ecx = l_ecx, g_edx = l_edx, g_ebx = l_ebx, g_esp = l_esp, g_esi = l_esi, g_edi = l_edi, g_ebp = l_ebp)
#define RECOMP_GPR_IN() (l_eax = g_eax, l_ecx = g_ecx, l_edx = g_edx, l_ebx = g_ebx, l_esp = g_esp, l_esi = g_esi, l_edi = g_edi, l_ebp = g_ebp)
#else
#define RECOMP_GPR_OUT() ((void)0)
#define RECOMP_GPR_IN() ((void)0)
#endif
#endif
