/* Guest functions of the MGR:R PC exe whose body is replaced by hand-written C (listed in pc/config/overrides.txt).
 * Structure from codepdbh/nfsu2-recompiled src/runtime/overrides.c (MIT). Empty until the lift shows which ones need it. */
#include <string.h>
#include "recomp_types.h"
#include "nfs_runtime.h"

void mgr_game_log(void);   /* guest_mgr.inc */

static const struct { uint32_t va; nfs_override_fn fn; } k_overrides[] = {
    { 0x00DD5650u, mgr_game_log },   /* game debug print (empty in retail) */
    { 0x00DD56A0u, mgr_game_log },   /* game debug print, second variant (empty in retail) */
};

nfs_override_fn nfs_override_lookup(uint32_t va) {
    for (unsigned i = 0; i < sizeof k_overrides / sizeof k_overrides[0]; i++)
        if (k_overrides[i].va && k_overrides[i].va == va) return k_overrides[i].fn;
    return 0;
}

int nfs_override_count(void) { return (int)(sizeof k_overrides / sizeof k_overrides[0]); }
