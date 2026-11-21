#version 450
// FSR 1, second pass (RCAS, robust contrast-adaptive sharpening): the EASU output (or the back buffer
// itself when it already has the window size) onto the swap chain image, with the D3D9 gamma ramp.
// ffx/ffx_a.h and ffx/ffx_fsr1.h: AMD FidelityFX FSR 1 (MIT, ffx/LICENSE-FidelityFX.txt).
#extension GL_GOOGLE_include_directive : require
layout(binding = 0) uniform sampler2D s_image;   // read by whole texels
layout(binding = 1) uniform sampler1D s_gamma;
layout(location = 0) in vec2 i_coord;
layout(location = 0) out vec4 o_color;
layout(push_constant) uniform rcas_t { uvec4 con; uint gamma; } u;
#define A_GPU 1
#define A_GLSL 1
#include "ffx/ffx_a.h"
#define FSR_RCAS_F 1
AF4 FsrRcasLoadF(ASU2 p) { return texelFetch(s_image, p, 0); }
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}
#include "ffx/ffx_fsr1.h"
void main() {
  AF3 c;
  FsrRcasF(c.r, c.g, c.b, AU2(gl_FragCoord.xy), u.con);
  if (u.gamma != 0u)
    c = vec3(texture(s_gamma, c.r).r, texture(s_gamma, c.g).g, texture(s_gamma, c.b).b);
  o_color = vec4(c, 1.0);
}
