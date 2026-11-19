/*
 * nfs_runtime.h - what the generated code and native32 need from this project:
 * the SSE register file, the loud-failure hooks, manual overrides, and the
 * crash report.
 */
#ifndef NFS_RUNTIME_H
#define NFS_RUNTIME_H
#include <stdint.h>

/* Install the crash reporter (vectored handler, after native32's). */
void nfs_install_crash_handler(void);

/* Write the full machine report to the log and to logs/crash_<stamp>.txt.
 * `why` is one line. Safe to call from any thread holding the machine. */
void nfs_report(const char *why, const void *host_ctx /* CONTEXT* or NULL */);

/* Name of an import/native address ("kernel32.dll!CreateFileA"), or NULL. */
const char *nfs_native_name(uint32_t va);

/* Manual overrides: a guest VA whose body is replaced by hand-written C.
 * Each one is documented where it is defined (src/runtime/overrides.c). */
typedef void (*nfs_override_fn)(void);
nfs_override_fn nfs_override_lookup(uint32_t va);
int nfs_override_count(void);

/* Game root (directory holding SPEED2.EXE) and the guest image path. */
extern char g_nfs_game_root[];
extern char g_nfs_exe_path[];

/* Direct3D 9 tracing (d3d9_trace.c). */
void nfs_d3d9_trace_init(void);
void nfs_d3d9_write_report(void);
uint32_t nfs_d3d9_frames(void);

#define NFS_GUEST_BASE 0x00400000u

#endif
