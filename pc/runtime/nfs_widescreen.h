#pragma once
// MGR PC: the NFSU2 widescreen hooks do not apply. Kept as no-ops so the shared runtime builds unchanged; the MGR's own
// aspect/resolution handling goes through the launcher options (PCPorts ports/mgr-pc/ESTADO.md).
inline void nfs_widescreen_init(const char*){}
inline int nfs_widescreen_hud_entries(){return 0;}
inline void nfs_widescreen_apply_frame_rate(){}
inline void nfs_widescreen_set_resolution(unsigned,unsigned){}
inline void nfs_widescreen_set_frame_rate(unsigned){}
