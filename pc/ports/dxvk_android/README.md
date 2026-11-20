# DXVK native Android port

This target adapts official DXVK native tag `native-1.9.2b`, commit
`c8dc91fabd00cac11d697ccf07426e798393cd40`, for Android ARM64. It is separate
from the Xbox 360 renderers in the reference repositories.

Before configuring the Android application, clone and prepare the dependency
from the repository root:

```powershell
git clone --branch native-1.9.2b https://github.com/Joshua-Ashton/dxvk-native.git ../reference/dxvk-native
python ports/dxvk_android/prepare.py ../reference/dxvk-native build/dxvk-source
```

Run shader generation with Python, `glslangValidator` and `spirv-val` available
in the same environment (the development setup uses Ubuntu/WSL):

```bash
python3 ports/dxvk_android/shaders.py build/dxvk-source
```

This generates and validates 54 built-in shaders for Vulkan 1.1. Application
CMake links the prepared backend into `libnfsu2_android.so`. The locally lifted
game sources and patched pcrecomp runtime must also exist as described in the
project README. Build caches, shaders and fetched source are not committed.

Changes include ARM64 intrinsics/spin-wait fallbacks, Android window/surface
integration, conditional BC texture support, and a separate 32-bit guest COM
bridge. Original D3D9 shaders are compiled by DXVK's DXSO translator. Missing
BC texture support can use the project's DXT decoder for supported 2D/cube
lock/upload paths. Compressed copies, volume resources and all D3D9 edge cases
are not complete. Vulkan API 1.1 alone does not establish every optional
feature needed by this DXVK version; test actual target GPUs.

Host query checks: `bash ports/dxvk_android/query_check.sh`.
Actual guest/surface bring-up: `bash tests/android_runtime/run_with_surface.sh`
(requires Xvfb, Xlib, Mesa llvmpipe and the prepared dependency). The latter is
a strict incremental bring-up check, not a gameplay test; a time-budget stop
returns failure and must be investigated.

## Present effects (FSR 1 / CAS, frame generation)

`fx/present_fx.cpp` replaces the swap chain blit when enabled by environment
(the launcher sets these; all off by default, nothing changes without them):

| Variable | Values | Effect |
|---|---|---|
| `DXVK_FSR` | 0, 1, 2 | off (DXVK blit), AMD FSR 1 (EASU + RCAS, two passes), CAS (sharpen + upscale, one pass) |
| `DXVK_FSR_SHARPNESS` | 0..1 (0.8) | RCAS `2 * (1 - s)` stops; CAS `lerp(8, 5, s)` |
| `DXVK_FSR_OUT` | 0, 720 | window buffers at the screen size, or 720 lines (the compositor scales the rest) |
| `DXVK_FI` | 0, 1 | one generated frame before each game frame (30 -> 60), FIFO forced, +1 frame latency |
| `DXVK_FI_QUALITY` | 0, 1, 2 | blend only, motion search (default), motion + neighbour vectors |
| `DXVK_FI_HZ` | Hz (60) | display rate; each frame is presented `round(Hz / 60)` times |
| `DXVK_FX_DUMP`, `DXVK_FX_DUMP_DIR` | frame, dir | lab: PPM dumps of that frame's input, generated and output images |

With `DXVK_FSR` the WSI (`android_wsi.cpp`) gives the window the upscaler's
output size instead of the render size. Frame generation is the 360 port's
`patches/0092-frame-interpolation.patch` shader (block motion search on a luma
pyramid, no game motion vectors) on the back buffer, before the upscale.
Statistics go to logcat (tag `MGR`, `present-fx`) every 300 game frames: GPU ms
per upscale and per generated frame, generated / not generated frames, cuts.

Hooks are in `prepare.py`; the code and shaders are compiled from `fx/`.
Shaders: `python3 fx/fx_shaders.py` regenerates `fx/generated/*.h` (committed).
Host check (Linux GPU, headless): build with `-DDXVK_HOST_HEADLESS=ON`, run
`DXVK_FSR=1 DXVK_FI=1 ./dxvk_fx_check`.

## Third-party notices

- DXVK native: zlib license, retained in the prepared source's `LICENSE` and
  [DXVK-LICENSE](DXVK-LICENSE). Source is available at the pinned upstream
  revision above; `prepare.py` contains the reproducible modifications.
- DXVK's bundled native Windows/DirectX headers include Wine-derived material
  under LGPL 2.1 or later. Retain those headers' copyright/license notices when
  distributing the dependency or binaries; these are not relicensed to MIT.
- Supplemental `vulkan_android.h` and `vulkan_xlib.h` are from Khronos
  Vulkan-Headers tag `v1.2.189`. They retain their Apache 2.0 notices.
- Gradle wrapper launchers/JAR use Gradle's Apache 2.0 tooling license.
- `fx/ffx/ffx_a.h` and `fx/ffx/ffx_fsr1.h` are AMD FidelityFX FSR 1
  (v1.20210629, MIT, `fx/ffx/LICENSE-FidelityFX.txt`), unmodified; the
  compiled SPIR-V in `fx/generated/` contains them. `fx/dxvk_cas.frag` follows
  the CAS idea without AMD's header.

The project's own adapter code is covered by the repository license. No EA
game data or generated game C is distributed in this repository.
