#pragma once
// Lab (livediff entry hook, CMake MGR_LIVEDIFF_ENTRY): force-included after arm64_guest.h, whose RECOMP_ENTER replaces
// the header's and so drops RECOMP_NATIVE_CHECK. Same body plus the check, so recomp_run_native sees every entry.
#undef RECOMP_ENTER
#define RECOMP_ENTER(va) do { \
    g_cur_func=(va); RECOMP_NATIVE_CHECK(va) RECOMP_FLAGS_IN(); \
    if((va)==0x005BF610u){ \
        RECOMP_REGS_OUT(); \
        if(nfs_android_resolution()){RECOMP_REGS_IN();return;} \
        RECOMP_REGS_IN(); \
    } \
} while(0)
