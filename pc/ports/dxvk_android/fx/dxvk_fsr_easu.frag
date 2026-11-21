#version 450
// FSR 1, first pass (EASU, edge-adaptive upscale): the game's back buffer to the window size, into an
// intermediate image that dxvk_fsr_rcas.frag then sharpens onto the swap chain image (present_fx.cpp).
// ffx/ffx_a.h and ffx/ffx_fsr1.h: AMD FidelityFX FSR 1 (MIT, ffx/LICENSE-FidelityFX.txt). The constants
// come from FsrEasuCon on the CPU. 32-bit path: DXVK 1.9.2b does not enable shaderFloat16.
#extension GL_GOOGLE_include_directive : require
layout(binding = 0) uniform sampler2D s_image;   // normalized coordinates, linear, clamp to edge
layout(location = 0) in vec2 i_coord;
layout(location = 0) out vec4 o_color;
layout(push_constant) uniform easu_t { uvec4 con0, con1, con2, con3; } u;
#define A_GPU 1
#define A_GLSL 1
#include "ffx/ffx_a.h"
#define FSR_EASU_F 1
AF4 FsrEasuRF(AF2 p) { return textureGather(s_image, p, 0); }
AF4 FsrEasuGF(AF2 p) { return textureGather(s_image, p, 1); }
AF4 FsrEasuBF(AF2 p) { return textureGather(s_image, p, 2); }
#include "ffx/ffx_fsr1.h"
void main() {
  AF3 c;
  FsrEasuF(c, AU2(gl_FragCoord.xy), u.con0, u.con1, u.con2, u.con3);
  o_color = vec4(c, 1.0);
}
