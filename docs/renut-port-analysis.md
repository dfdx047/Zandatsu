# Renut (mgr-nx-deko) -> Project SwordMan (Android): porting analysis

Date: 2026-10-23. Read-only analysis; no repo was modified.
Paths: `MGRNX` = /tmp/claude-0/-home-claude/a7e7c5fc-7625-57d5-87d2-debffa507158/scratchpad/mgrnx ; `SM` = /home/claude/projectswordman ; `SDK` = /home/claude/sdkwork.
Both repos are GPL-3.0 (MGRNX/LICENSE, SM/LICENSE); plume is MIT. Copying the code into SM is license-compatible.

---------------------------------------------------------------------------------------------------
## 1. Build graph

### 1.1 Entry points
- MGRNX/app/CMakeLists.txt:23 `option(MGR_RENUT_RENDERER ...)` (default ON only on Switch); :171-185 adds the glue sources
  and `include(cmake/mgr_renut_gpu.cmake)`. :59-64: on PC `rexglue_setup_target(sa2 GPU_PLUGINS xenos)`, on Switch no plugin.
- MGRNX/app/cmake/mgr_renut_gpu.cmake: wraps Renut's gpu.cmake.
  - :8-9 the renderer is an **OBJECT library** (`mgr_renut_gpu`, :103) on purpose: hooks replace weak `sub_X`; a static
    archive would never be pulled in. Keep this on Android (link the objects into libmain.so, same .so as `mgr_recomp`).
  - :13-14 `RENUT_SHADER_DUMP_DIR` (= out/mgr-deko-shaders/containers), `RENUT_ASSETS_DIR`.
  - :18-26 `MGR_RENUT_PREBUILT_SHADER_CACHE` + `banjo_renut_shader_cache_override()` = use a prepared shader_cache cpp
    instead of building XenosRecomp. **This is the mechanism to reuse on Android.**
  - :30-36 forwards toolchain vars to the XenosRecomp ExternalProject (bad for Android, see 1.4).
  - :38-41 `rex::o1heap` alias (our SDK: check the target name; same fix applies).
  - :46-90 Switch-only (NVK, deko3d, uam, SPIRV-Cross, GLSL package). Drop.
  - :103-116 extra sources: banjo_renut_select.cpp, banjo_renut_tuning.cpp, banjo_renut_checks.cpp, banjo_renut_record.cpp,
    display_sync.cpp, perf/frame_stats.cpp, sa2_compat/frame_latency.cpp, sa2_compat/sa2_compat.cpp, gpu/sa2_renut_pacing.cpp.
  - :117-125 include dirs: app root, app/src, sa2_compat, `REXGLUE_ENTRYPOINT_INCLUDE_DIR`, `${REXSDK_DIR}/src/core`.
  - :127-138 deko3d glue (Switch). :143,159 defines `MGR_RENUT_RENDERER=1 SA2_RENUT_RENDERER=1` on both targets.
  - :161-163 `-ffp-contract=off -fno-strict-aliasing`.
- MGRNX/app/src/renut_engine/gpu/gpu.cmake (335 lines):
  - :36-39 `add_subdirectory(thirdparty/plume)`.
  - :43-63 zstd (decompress only), xxHash, smol-v built from reblue-XenosRecomp/thirdparty, static, PIC.
  - :68-74 DXC: `thirdparty/dxc-bin/bin/x64/dxc-linux` + `lib/x64` (LD_LIBRARY_PATH) on non-Windows hosts.
    **The dxc-bin `bin/` and `lib/` folders are not in this checkout** (only `inc/`, CMakeLists.txt): download a DXC
    Linux release or use the distro's `dxc` (Arch/CachyOS has a directx-shader-compiler package; verify version).
  - :79-125 `renut_host_shader()` -> DXC `-spirv -fvk-use-dx-layout` (+`-fvk-invert-y` for VS), header `g_<stem>_spirv`.
    :112-124 deko3d variant (Switch only).
  - :140-238 `renut_xenos_shader_cache()`: XenosRecomp as ExternalProject + Python dump + early-alpha pass +
    collect_captured_shaders.cmake -> `${CMAKE_BINARY_DIR}/renut_gpu/shader_cache.cpp`. Skipped when the override exists (:141-144).
  - :241-280 the 33 renderer sources (backend/*, d3d/d3d_native_state.cpp, d3d/hooks/{device,draw,resource,shader}.cpp
    (state.cpp deliberately excluded, :258), guest/{callbacks,command_buffers,guest_records,sa2_screen}.cpp,
    overlay/*, render/*).
  - :282-293 12 host shaders: overlay_vs/ps, present_vs/ps, depth_resolve_ps, depth_resolve_ms_ps, texture_mip_ps,
    resolve_copy_ps, rectangle_list_gs (gs_6_0), point_sprite_gs (gs_6_0), stream_convert_cs, target_readback_cs.
  - :312-324 a private Dear ImGui copy compiled in, except on Switch.
  - :325-326 `RENUT_RECOMP` define; links plume, renut_zstd, renut_smolv, renut_xxhash, rex::o1heap.
  - :330-334 `find_package(X11)` on any non-Apple non-Switch platform -> **breaks on Android**; guard with `NOT ANDROID`.
- plume (MGRNX/app/thirdparty/plume/CMakeLists.txt): STATIC lib, sources plume_vulkan.cpp (+D3D12 on Win, Metal on Apple,
  deko3d on Switch :118-226). Bundles volk, Vulkan-Headers, VMA under contrib/. On Android `IS_LINUX` is false
  (:16, CMAKE_SYSTEM_NAME is "Android"), so nothing Linux/SDL-specific is pulled; no Android-specific CMake needed.
- XenosRecomp (thirdparty/reblue-XenosRecomp/XenosRecomp/CMakeLists.txt): host exe; deps DXC (libdxcompiler), xxHash,
  zstd, fmt, smol-v; `XENOS_RECOMP_RENUT=ON` -> `RENUT_RECOMP`. DXIL only on Windows (`XENOS_RECOMP_DXIL`); without it the
  generated cpp still defines empty `g_compressedDxilCache` (main.cpp:455-469), so the renderer links.
  Uses `std::execution::par_unseq` (main.cpp:179; pch.h:19): with GCC/libstdc++ this may require linking TBB, with
  clang+libc++ `-fexperimental-library` (CMakeLists handles the latter). Build it with the host compiler.
- Python steps: (a) gpu.cmake dump_shaders.py (needs `xxhash` module, :26-34) only if no dump dir and no override;
  (b) renut_early_alpha.py + early_alpha.py (:199-216, option `BANJO_RENUT_EARLY_ALPHA` default ON - perf only, can be OFF);
  (c) for MGR the real input step is tools/extract_mgr_cpk_shaders.py + tools/installer_probe/cpk_toc.py (stdlib only) +
  cpk_shader_scan.cpp (C++17, no deps); (d) renut_switch_shader_cache.py (stdlib) to convert the cache to .incbin form.
- Glue the MGR build also compiles in Renut mode (CMakeLists.txt:172-181): mgr_src/mgr_me_hooks.cpp, mgr_sa2_stubs.cpp,
  src/sa2_capture.cpp, src/profiler/sa2_thread_cycles_pc.cpp, src/me/sa2_render_mode.cpp, src/cpu/sa2_enhancements.cpp,
  sa2_codegen_checks.cpp, sa2_forms_selfcheck.cpp. Most are SA2/Switch diagnostics (see 2.3).

### 1.2 Size
renut_engine/gpu: 99 files, ~32k lines of C++ (`find ... | xargs wc -l`). plume_vulkan.cpp ~5.5k lines.

### 1.3 Drop for Android Vulkan
deko3d everything (banjo_renut_deko3d.cpp/.h, plume_deko3d*.cpp, uam, `BANJO_DEKO3D_*`, deko3d host shader blobs,
`MGR_RENUT_RELEASE_SHADERS`/GLSL package), NVK vars, `SA2_SWITCH_NO_NVK`, `-g1`, LTO + hot-order + `-O0` register
file (CMakeLists.txt:187-222), D3D12 / DXIL / DRED / windows.h paths (all behind `_WIN32`), d3d/hooks/state.cpp,
guest/photo_capture.cpp, guest/widescreen.cpp (already excluded), mgr_d3d_census.cpp, mgr_shader_dump.cpp (optional),
mgr_wwise_dsp.inc (Wwise leaf, unrelated), the src/me native PM4 renderer (MGR_NATIVE_RENDERER, CMakeLists.txt:83-169).

### 1.4 Android-specific build problems
1. `ExternalProject_Add(renut_xenos_recomp ... -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER})` (gpu.cmake:152-169) would build
   XenosRecomp with the NDK clang -> an arm64 Android binary that cannot run on the PC. Also dxc-bin's CMake picks the
   arch from `CMAKE_SYSTEM_PROCESSOR` (aarch64 under the NDK). => Never build XenosRecomp inside the Gradle/NDK CMake.
   Define `banjo_renut_shader_cache_override()` (as mgr_renut_gpu.cmake:18-26) pointing at a cpp produced by a host step.
2. gpu.cmake:330-334 X11 -> guard.
3. ImGui: our SDK's `imgui` is an OBJECT library (SDK/thirdparty/CMakeLists.txt:263-270) and SM links `imgui::imgui`
   into libmain (SM/app/src/main/cpp/CMakeLists.txt:157-163). Adding Renut's copy (gpu.cmake:312-323) = duplicate
   symbols in libmain. Take the Switch branch on Android (no second copy, no `overlay::Start`, app_integration.cpp:102-110).
4. Host shaders: DXC must run on the host (gpu.cmake uses `CMAKE_HOST_WIN32`, OK); just provide the binary path.

---------------------------------------------------------------------------------------------------
## 2. SDK coupling (their fork vs SDK = /home/claude/sdkwork)

### 2.1 Same in both (no work)
- Hook mechanism: `REX_FUNC`, `REX_EXTERN` (SDK/include/rex/ppc/func.h:45-46), `REX_HOOK`/`REX_HOOK_RAW`
  (SDK/include/rex/hook.h:36-49), `REX_IMPORT` (hook.h:229), `rex::ppc::HostToGuestFunction`/`GuestToHostFunction`
  (ppc/function.h, file identical in both). Generated functions: `DEFINE_REX_FUNC` = `__imp__sub_X` + weak alias
  `sub_X` in both (SDK/resources/templates/codegen/pch_h.inja:65-86). Their fork only adds a GCC spelling and
  `REX_EXPORT_CENSUS` (fork hook.h:28-57), irrelevant with clang.
  Renut's macros (banjo_renut_select.h:31-61) expand to these -> compatible.
- Guest memory: renut/gpu/guest/guest_memory.h uses `REX_KERNEL_MEMORY()->virtual_membase()/physical_membase()`
  (SDK kernel_state.h:73, xmemory.h:381,398); `rex::memory::detail::PhysicalHostOffset` is 0 on Linux/Android
  (SDK xmemory.h:40-46), matching the non-Win branch of guest_memory.h:47-51 and draw.cpp:829-850.
- ReXApp hooks used by mgr_main.cpp exist in ours: OnPreSetup(RuntimeConfig&) with `config.graphics`/`gpu_plugin`
  (SDK runtime.h:67-68), OnCreateImmediateDrawer (rex_app.h:204), OnFinalizePaths, LaunchModule (:260),
  OnWindowCloseRequested (:224), OnPostLaunchModule (:177), OnWindowPixelSizeChanged (:215),
  window()->RequestPresenterUIPaintFromUIThread (ui/window.h:376). "Detached" mode (no graphics system, app-owned
  drawer) exists in our rex_app.cpp:932-940, which is what Renut's ConfigureRuntime (overlay/app_integration.cpp:86-89)
  relies on.
- Other APIs present in ours: rex::cvar::{Query, SetFlagByName, HasNonDefaultValue}, RegisterLogCategory, ImGuiDrawer,
  ImmediateDrawer, AppUIDrawContext, ImGuiDialog, runtime::FunctionDispatcher, runtime::ResolveIndirectFunction,
  perf::SetCsvLogPath, input::{SharedAssignment, CreateDefaultInputSystem}, MappedPtr, GetLoggerRaw, ui::RegisterBind,
  kernel::xboxkrnl::VdQueryVideoMode (used at d3d/hooks/device.cpp:287), filesystem::GetExecutableFolder
  (Android: returns `REX_APP_DIR`, SDK/src/core/filesystem_posix.cpp:102-110 - so Renut's NextToExecutable paths,
  banjo_renut_tuning.cpp:1375-1377, land in the app's files dir; good).
- 177 cvars defined by Renut+glue: **no name collision** with our SDK or SM/app/src/main/cpp.

### 2.2 Gaps (fork-only things Renut/glue uses)
| # | What | Where used | Fix |
|---|---|---|---|
| G1 | `rex/input/read_hook.h`, `rex::input::SetReadHook` (fork SDK input_system.cpp:255-259) | backend/render_thread.cpp:15, :390 (late input + latency probe) | small SDK patch (call hook in InputSystem::GetState) or `#if` it out (late-input feature off) |
| G2 | `ReXApp::BuiltInConfig()` / `DefaultConfigFile()` / `cvar::LoadConfigString` (fork rex_app.h:152-157, cvar.h:124-126) | mgr_main.cpp:182-200 | not needed: use cvars.txt / OnPreSetup defaults in our MgrApp |
| G3 | `rex/sys_counters.h` (`rex::syscount`) | src/cpu/sa2_enhancements.cpp:15,279; sa2_codegen_checks.cpp:114-117 | drop those two files; stub the few symbols Renut reads from sa2_enhancements.h (cpu/sa2_enhancements.h is header-only cvars + counters) |
| G4 | `rex/ppc/diet_ops.h` | src/cpu/sa2_forms_selfcheck.cpp:28 | drop (self-check of their codegen forms) |
| G5 | `rex/ui/switch_saltynx.h` | src/me/sa2_render_mode.cpp:15 (unguarded include), :89 | guard include; that function is Switch-only |
| G6 | `g_rex_atomic_legacy_barrier` (fork atomic_posix.cpp) | sa2_enhancements.cpp:64,196 | goes with G3 |
| G7 | `rex::latency::*` | display_sync.cpp, render_thread.cpp, device.cpp:903-904 | already app-side: sa2_compat/rex/frame_latency.h + frame_latency.cpp (no SDK change) |
| G8 | `g_banjo_store_cards`, `RexStoreCardsHostBytes` | draw.cpp:908 etc. | app-side stubs in sa2_compat/sa2_compat.cpp:10-15; MGR disables store cards (draw.cpp:5026-5028) |
| G9 | banjo/masseffect cvars read by tuning | sa2_compat/sa2_compat.cpp:19-110 | app-side, keep |
| G10 | `REXCVAR_DECLARE(bool, banjo_cpu_verify)` | banjo_renut_record.cpp:38 | defined nowhere in MGRNX; fine only while unused; define it if the link complains |
| G11 | `REXCVAR_DECLARE mgr_native_reuse_scratch` | mgr_main.cpp:54,223 (defined in src/me/native/sa2_native_draws.cpp:75, not built in Renut-only mode) | don't port mgr_main.cpp's log line |
| G12 | Switch thread helpers (`RexSetGuestThreadCoreLayout`, `threading_switch.h`, `RexSwitchPerfCount`) | device.cpp:886-892, me/me_platform_compat.h:19-36, perf/frame_stats.cpp:105 | all under `REX_PLATFORM_SWITCH`; no-ops elsewhere |
| G13 | `rex::kernel::xboxkrnl::VdSwapCount/VblankPerSwap/WaitSwapForVblank` (fork video.h:31-40) | not used by Renut/MGR glue | none |

Our-side incompatibilities created by Renut (not SDK gaps but must be handled):
- **VdSwap is never reached in Renut mode**: Renut replaces `D3DDevice_Swap` (device.cpp:878-1267, shared hook at
  sub_826B3510) and does not call the original. Our pause + frame counter live in `rex_vdswap_hook` (patch 0100,
  SM/app/src/main/cpp/android_hooks.cpp:1-30) -> call it from the Renut swap path, or from the sub_826B3510 hook.
- No vblank interrupt / no graphics system: pacing is emulated (gpu/sa2_renut_pacing.cpp:4-6,40,151). Everything
  implemented in librexgpu-xenos.so (patches 0012 pause, 0015 FSR/CAS, 0092 frame interpolation, 0105 freeze replay,
  0107/0108 `rex_swordman_*` stats and `swordman_skip_ps`, AutoPilot `capture`/`selftest`, screenshots via presenter)
  is inactive under Renut. Symbols still link (we keep linking rex::gpu-xenos), they just report nothing.
- SM's MgrApp::OnPreSetup forces `gpu_plugin = "xenos"` when empty (SM/src/mgr_app.h:42-46). In Renut mode it must call
  `renut::gpu::ConfigureRuntime(config)` instead (clears graphics + gpu_plugin) and start the renderer in
  OnFinalizePaths, like mgr_main.cpp:289-391.
- Renut's selector cvar is `sa2_renderer` with default **"renut"** (banjo_renut_select.cpp:17,46). For an opt-in M1,
  change the default to "xenos"/"" (or rename to `swordman_renderer`).

### 2.3 GPU_PLUGINS difference
MGRNX/app/CMakeLists.txt:59-64: Switch builds call `rexglue_setup_target(sa2)` (no plugin) because Renut owns Vulkan;
PC keeps `GPU_PLUGINS xenos` as a reference and Renut is chosen at boot by cvar. SM already uses
`rexglue_configure_target(main GPU_PLUGINS xenos)` (SM/app/src/main/cpp/CMakeLists.txt:167) - keep it: both renderers
in one APK, chosen at boot. Only one may own the ANativeWindow (a second VkSurface on the same window fails with
VK_ERROR_NATIVE_WINDOW_IN_USE_KHR), which Renut's detached mode guarantees as long as the xenos plugin is not loaded.

### 2.4 Codegen options
Their flavour (MGRNX/app/codegen_flavor.toml:119-135: cr/ctr/xer/reserved_as_local, skip_msr, dform_disp_split,
non_volatile_as_local, elide_lr_stores, non_argument_as_local, fp_* ...) is a **performance** layer; Renut does not
require it. Renut's hooks only read argument registers r3-r10 and f1 (grep: draw.cpp:630,642,652) and write r3, which
our classic codegen keeps in `ctx`. Function naming is `sub_%08X` in both (SDK function_graph.cpp:470-482, identical).
`args_in_registers` exists only in their fork and is not enabled in their MGR flavour. No codegen change needed.

---------------------------------------------------------------------------------------------------
## 3. Codegen / manifest: guest addresses

Their manifest: MGRNX/app/sa2_manifest.toml (project name "sa2", includes overrides.toml + codegen_flavor.toml).
Ours: SM/mgr_manifest.toml (includes mgr_overrides.toml, 15 `[functions]` entries).

**None of the hooked addresses is declared in their tomls** (grep of all *.toml): they rely on automatic function
discovery, as ours does. The only matches are 0x826ACC10, 0x826ACDF8, 0x826C0278 inside
`non_argument_as_local_keep` (codegen_flavor.toml:5520-5536), a perf-flavour list, not boundaries.

Addresses Renut + its MGR glue define as hooks (strong `sub_X`) or call as `__imp__sub_X` (must exist as function
starts in OUR generated code, otherwise the link fails on `__imp__sub_X`):

| Address | Role | File:line |
|---|---|---|
| sub_826B2418 | Direct3D_CreateDevice (raw) | d3d/hooks/device.cpp:1437 |
| sub_826B59B0 | InsertCallback | device.cpp:1440 |
| sub_826B3050 | SynchronizeToPresentationInterval | device.cpp:1441 |
| sub_826B5688 | CDevice::KickOff | device.cpp:1442 |
| sub_826B5348 | KickOffSegment | device.cpp:1443 |
| sub_826B58C0 | BeginRingBig | device.cpp:1444 |
| sub_826B4078 | RingBufferDeviceAllocate | device.cpp:1445 |
| sub_826B44C0 | AllocateInlineData | device.cpp:1446 |
| sub_826B45A8 | NewSegment | device.cpp:1447 |
| sub_826B5A08 | SetRingBufferParameters | device.cpp:1448 |
| sub_826B3510 | D3DDevice_Swap (shared: Renut side + mgr_me_hooks.cpp:206) | device.cpp:1449 |
| sub_826B42A8 | BlockOnFence | device.cpp:1450 |
| sub_826B1C88 | CBlocker::Check | device.cpp:1451 |
| sub_826B57C8 | IsFencePending | device.cpp:1452 |
| sub_826BC928 | DrawVertices (shared; mgr_me_hooks.cpp:277) | d3d/hooks/draw.cpp:602 |
| sub_826BCD40 | DrawIndexedVertices (shared; mgr_me_hooks.cpp:257) | draw.cpp:603 |
| sub_826BC468 | BeginVertices | draw.cpp:606 |
| sub_826B5FC8 | WindowScissor (raw) | draw.cpp:611 |
| sub_826BC260 | ClearF (raw) | draw.cpp:623 |
| sub_826BB0D0 | Resolve (raw) | draw.cpp:640 |
| sub_826AAF80 | D3DResource_Destroy (raw) | d3d/hooks/resource.cpp:1696 |
| sub_826ACD00 | XGRegisterVertexShader (raw) | d3d/hooks/shader.cpp:476 |
| sub_826AC3D8 | XGRegisterPixelShader (raw) | shader.cpp:483 |
| sub_826AC938 | CreateVertexDeclaration (raw) | shader.cpp:490 |
| sub_826C0C48 | InitializePresentationParameters (called) | device.cpp:1312,1398 |
| sub_826C0278 | InitializeApiState (called) | device.cpp:1313,1406 |
| sub_82715A90 | game XMemFree (REX_IMPORT) | resource.cpp:50 |
| sub_826CB580 | draw-time shader load (glue hook, dump only) | mgr_src/mgr_me_hooks.cpp:178 |
| sub_826B2FC0 | engine GPU-wait poll (glue hook, `mgr_gpu_wait_sleep`) | mgr_me_hooks.cpp:333 |
| sub_826CA7E8/826CA450/826CA690 | native-renderer flushes (MGR_NATIVE_RENDERER only) | mgr_me_hooks.cpp:299-316 |
| sub_826ACDF8/826ACC10 | Create{Vertex,Pixel}Shader (shader dump, optional) | mgr_shader_dump.cpp:114-123 |
| sub_825B4070 | Wwise filter (unrelated) | mgr_wwise_dsp.inc:232 |

Runtime safety: d3d/mgr_hook_map.h:6-35 lists 29 guards (address, bytes, FNV-32) checked at device creation
(device.cpp:1323-1333) - a different XEX build is detected and logged.

Whether each address is a function start in OUR generated/default cannot be checked here (generated code is not on
this machine). Check on the PC:
```
for a in 826B2418 826B59B0 826B3050 826B5688 826B5348 826B58C0 826B4078 826B44C0 826B45A8 826B5A08 826B3510 826B42A8 \
  826B1C88 826B57C8 826BC928 826BCD40 826BC468 826B5FC8 826BC260 826BB0D0 826AAF80 826ACD00 826AC3D8 826AC938 \
  826C0C48 826C0278 82715A90 826CB580 826B2FC0; do grep -q "DEFINE_REX_FUNC(sub_$a)" generated/default/*.cpp || echo "missing $a"; done
```
Any missing one goes into mgr_overrides.toml `[functions]` (`0x826B2418 = {}`). Expectation: all present (XDK D3D
functions are direct `bl` targets; their auto-discovery found them with the same SDK base), but unverified.
Side finding: sub_826B2FC0 is the spinning GPU-wait poll that SM/app/src/main/cpp/android_hooks.cpp:10-15 is looking
for ("~2.8 million calls a frame, 91 % of a core", mgr_me_hooks.cpp:321-345).
Also: their overrides.toml registers 0x824410B0, 0x82441108/20/38, 0x82518AB0, ... which our mgr_overrides.toml does not,
plus two thunks registered at runtime (mgr_missing_functions.cpp:22-50, "Sundowner crash"). Not Renut-related.

---------------------------------------------------------------------------------------------------
## 4. Shaders

### 4.1 What the runtime expects
Embedded arrays only (Vulkan path): shaders/shader_cache.h:74-88 `g_shaderCacheEntries[]`, `g_shaderCacheEntryCount`,
`g_shaderVertexInputs[]`, `g_shaderLiterals[]`, `g_compressedSpirvCache[]` (+ sizes), `g_compressedDxilCache[]`
(empty off Windows). shader_library.cpp:84-160 decompresses (zstd + smol-v) on first use. No file loader on the Vulkan
path (the only file loader is deko3d's `deko3d_glsl.bin`, banjo_renut_deko3d.cpp:104). Missing shaders are captured at
run time to `renut_shader_capture_dir` (shader_library.cpp:28,169-179) for the next build.

### 4.2 Inputs for MGR
tools/installer_probe/README.md (2026-10-23): `media/data000.cpk` holds 1,202 unique Xenos containers (1,036 of the
1,119 runtime-dumped ones byte for byte); default.xex has none usable; the 83 others are runtime variants Renut does not
need (it keys VS by the container passed to XGRegisterVertexShader). So the only input is **data000.cpk**:
1. `tools/installer_probe/cpk_toc.py` (stdlib) + `cpk_shader_scan.cpp` (CRILAYLA + container scan; build
   `clang++ -O2 -std=c++17 cpk_shader_scan.cpp -o cpkscan`) driven by `tools/extract_mgr_cpk_shaders.py <game> --out
   <dir> --scanner <exe>` -> `vs_/ps_<sha>.bin` (note: default scanner path is `cpk_shader_scan.exe`; pass `--scanner`).
2. XenosRecomp (host, RENUT_RECOMP): `XenosRecomp <dir> shader_cache.cpp shader_common.h [hlsl_dump_dir]`
   (tools/win/build_mgr_deko_shaders.ps1:10-17 shows the call).
3. Optional `renut_switch_shader_cache.py` logic: move the SPIR-V array to `spirv_cache.zst` + `.incbin` (big
   initializer lists are slow to compile; clang handles .incbin in inline asm too).
All three run on Linux (Python stdlib, C++17, DXC has Linux builds). Nothing in this chain is Windows-only except the
.ps1 wrappers.

### 4.3 Proposal
- Personal builds: new `tools/renut-shaders.sh` called by tools/build.sh before Gradle, only when
  `assets/media/data000.cpk` or the XenosRecomp/shader_common sources changed (stamp file like the codegen one):
  build cpkscan + XenosRecomp with the host toolchain into `out/host/`, extract, translate, write
  `out/renut/shader_cache.cpp` (+ .zst). Pass the path to Gradle CMake (`-DSM_RENUT_SHADER_CACHE_CPP=...`) and use
  `banjo_renut_shader_cache_override()`. The APK then contains game-derived data: personal only.
- Distribution: do not embed. Patch shader_library.cpp/shader_cache.h so the tables become pointers filled from a
  file (`<app files>/renut/shaders.bin`: header + entries + vertex inputs + literals + zstd SPIR-V). Produce it from the
  player's own data000.cpk with a PC script (same chain, Linux/Windows; DXC is the reason it cannot trivially run on
  the phone - an on-device path would need libdxcompiler built for Android arm64, large but possible) and push it with
  adb or pick it in the app. Fallback when absent: keep the xenos renderer.
- `BANJO_RENUT_EARLY_ALPHA` OFF at first (fewer moving parts); turn on later as an A/B.

---------------------------------------------------------------------------------------------------
## 5. Android specifics

### 5.1 Surface
- plume already has Android: `VK_USE_PLATFORM_ANDROID_KHR` (plume_vulkan.h:24-25), `VK_KHR_android_surface`
  (plume_vulkan.cpp:313-314), `vkCreateAndroidSurfaceKHR` (:2503-2513), size via `ANativeWindow_getWidth/Height`
  (:2966-2968), `RenderWindow = ANativeWindow*` (plume_render_interface_types.h:22,45-46). libmain already links
  `android` (needed for ANativeWindow_*).
- renut native_window.cpp has only Win32 / Switch / X11 branches (:8-17, :93-145); the X11 branch also does
  `setenv("SDL_VIDEO_DRIVER","x11")` at static init (:100-103) - must not run on Android. Add an `__ANDROID__` branch:
  `SDL_GetPointerProperty(SDL_GetWindowProperties(w), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr)` - exactly
  what our SDK does in SDK/src/ui/window_sdl.cpp:466-474. Pixel size: keep SDL_GetWindowSizeInPixels or ANativeWindow.
- Lifecycle: Android destroys the ANativeWindow when the app goes to background; Renut builds its swapchain once
  (Video::CreateHostDevice via StartRenderer, app_integration.cpp:95-121). Needs a surface-lost path (recreate surface
  + swapchain on resume / new window). Not present in Renut. Risk.
- Swapchain `preTransform = IDENTITY` (plume_vulkan.cpp:2833): on rotated panels the compositor rotates (extra pass,
  SUBOPTIMAL). Check what our presenter does on RP5/Cube and copy it.
- Custom driver: plume calls `volkInitialize()` (plume_vulkan.cpp:5419) -> system libvulkan. Turnip via adrenotools
  needs `volkInitializeCustom(vkGetInstanceProcAddr from adrenotools_open_libvulkan(...))` with the same
  `REX_VULKAN_DRIVER_DIR/NAME` env our SDK reads (SDK/src/ui/vulkan/vulkan_instance.cpp:63-90). Small plume patch.
  `TU_DEBUG=push_consts_per_stage` / storage-range env from TurnipSupport.applyAutoEnv will also apply to plume.

### 5.2 Features Renut actually needs (from shader_common.h and plume)
- Device extensions required by plume: only `VK_KHR_swapchain` (plume_vulkan.cpp:335-337); everything else optional,
  and plume enables **all** core features the device reports (`pEnabledFeatures = &deviceFeatures.features`, :4807).
  Instance API 1.2 (:5431).
- But the translated shaders assume, without a capability check in Renut:
  - `bufferDeviceAddress` + `shaderInt64`: push constants are `uint64_t` addresses read with `vk::RawBufferLoad`
    (shader_common.h:67-90, RENUT_SHARED_UINT64 :116; per-draw vertex-fetch copies by device address, comment :95-104).
  - descriptor indexing / bindless: `Texture2D g_Texture2DDescriptorHeap[]` etc. (shader_common.h:224-227), plume
    sets UPDATE_AFTER_BIND + PARTIALLY_BOUND + VARIABLE_COUNT (plume_vulkan.cpp:1526-1533), 8192 textures + 256
    samplers (backend/video.h:29-30).
  - `RWByteAddressBuffer g_MemExport` (shader_common.h:206) -> vertexPipelineStoresAndAtomics for memexport VS.
  - BC1-BC5 sampled natively (render/format.cpp:186-197, `bc(...)` = copy, no decoder). CTX1 is converted (:199-200).
  - Depth `D32_SFLOAT_S8_UINT` (format.cpp:38-42); D24S8 only on deko3d (:47-50).
- Adreno 650 + Turnip: geometryShader, BDA, int64, descriptor indexing, BC: all exposed by Turnip on a6xx (to my
  knowledge; confirm with `vulkaninfo` on RP5). Qualcomm proprietary: GS and BC yes; BDA/int64/update-after-bind
  limits depend on driver version - verify.
- Mali-G57 (RG Cube): **BC textures are not supported on Mali** (our SDK already masks textureCompressionBC and
  decompresses on Mali, SDK/src/graphics/vulkan/texture_cache.cpp:3828-3832, vulkan_device.cpp:53). Renut has no
  fallback -> textures fail on the Cube until a BCn decoder (CPU or compute) is added. geometryShader on Mali
  Valhall is commonly reported as unsupported (unverified - check vulkaninfo). D32S8 support on Mali: verify.
- Geometry shaders (point_sprite_gs.hlsl, rectangle_list_gs.hlsl): optional. Without `geometryShader` Renut logs and
  draws rectangle lists as triangle lists and point sprites as 1-px points (backend/video.cpp:324-336;
  render/draw.cpp:2367,3164-3176). A 3-vertex rectangle drawn as a triangle covers half the rectangle -> wrong for any
  game rectangle draw (Renut does its own clears/resolves, so the impact on MGR is unknown). On tilers GS is also slow;
  our SDK patch 0154 removed GS for the same primitives in xenos. Plan: CPU/VS expansion like Renut's existing
  triangle-fan/quad expansion (draw.cpp:2365 `expand = kTriangleFan || kQuadList`).
- Threads: render thread + 16 ms overlay pump thread (app_integration.cpp:47-80) + guest threads calling into hooks;
  std::thread only off Switch. Fine on bionic.

---------------------------------------------------------------------------------------------------
## 6. Plan

Layout suggestion: copy into SM as `renut/` (engine + plume + XenosRecomp + imgui-less) with an `UPSTREAM.md`
naming MGRNX commit; Android-specific edits as normal commits (the SDK patch rule applies to sdkwork only).

### M1 - compiles for arm64, Renut selected by cvar (difficulty: medium, ~2-4 days)
1. Host step `tools/renut-shaders.sh`: build cpkscan + XenosRecomp (host gcc/clang, DXC Linux), extract
   data000.cpk, translate -> out/renut/shader_cache.cpp. (medium; DXC/TBB toolchain fiddling)
2. CMake (SM/app/src/main/cpp/CMakeLists.txt): `option(SM_RENUT ON)`; include a trimmed `renut.cmake` = gpu.cmake
   minus deko3d/X11/ImGui copy/ExternalProject, plus the override function; OBJECT lib `mgr_renut_gpu` linked into
   `main`; host shaders via host DXC. (medium)
3. Source edits: native_window.cpp Android branch; drop/guard G1 (read hook), G3-G6 files, sa2_render_mode include;
   `sa2_renderer` default "xenos"; app_integration Android = Switch path (no overlay::Start); mgr_me_hooks.cpp reduced
   to the three shared hooks + sub_826B2FC0. (easy-medium)
4. MgrApp: in OnPreSetup choose Renut -> `ConfigureRuntime`, else keep xenos; OnFinalizePaths -> StartRenderer;
   OnCreateImmediateDrawer/OnWindowPixelSizeChanged forwarding (copy mgr_main.cpp:289-391). (easy)
5. Link check = address check: any missing `__imp__sub_X` -> add to mgr_overrides.toml. (easy)

### M2 - boots to title (difficulty: medium-hard, ~1 week)
6. plume: adrenotools-aware `volkInitializeCustom`; keep system driver path. (easy)
7. Pause/watchdog: call `rex_vdswap_hook` from the Renut Swap path. (easy)
8. Vulkan feature gate at StartRenderer: refuse Renut (fall back to xenos, no silent driver switch) if BDA/int64/
   descriptor indexing/BC missing; log which. (easy)
9. First boots on RP5/Turnip: expect pipeline/format issues, descriptor limits, D32S8, GMEM behaviour; DEVICE_LOST
   triage with `renut_gpu_*` diagnostics. (hard, open-ended)
10. Surface lifecycle (background/foreground) and rotation. (medium)

### M3 - gameplay (difficulty: hard, weeks)
11. Shader coverage: play chapters with capture dir on, rebuild cache (runtime capture -> pull -> rebuild).
12. Mali: BCn decode fallback; GS-free rectangle/point expansion. (medium-hard)
13. Perf on Adreno: tiling-friendly render passes (load/store ops), D24S8 option, push-constant vs UBO path
    (their UBO path is deko3d-only), MSAA off, render scale via `sa2_render_height_*`.
14. Re-wire SwordMan features that lived in xenos (FSR/CAS present pass, frame interpolation, AutoPilot capture,
    skip_ps) or accept their absence in Renut mode.
15. Distribution: shaders.bin file loader + PC generator.

### Risks
- No feature checks in Renut: unsupported BDA/int64/BC = crash or garbage, not a clean error (mitigate with step 8).
- Mali (RG Cube) is a second port in itself (BC, GS, maybe D32S8).
- Renut is tuned for NVK/deko3d and Windows D3D12; Adreno performance unknown; GMEM loads/stores may dominate.
- Surface loss on Android not handled by Renut/plume today.
- Shared ImGui / duplicate symbols if the Renut ImGui copy is not excluded.
- Their pacing replaces vblank interrupts; any MGR logic tied to the GPU interrupt callback behaves differently.
- Shader cache is game data: never commit it; the APK with an embedded cache is personal-only.
- Upstream churn: MGRNX changes daily (banjo-nx/sa2-nx lineage); pin a commit and port deltas deliberately.
