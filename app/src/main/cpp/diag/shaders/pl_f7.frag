#version 450
#extension GL_GOOGLE_include_directive : require
#include "pl_common.glsl"
layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec3 vNormal;
layout(location = 0) out vec4 o;
// Tonemap ACES + gradacao de cor + dithering.
vec3 aces(vec3 x) { return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }
void main() {
  vec3 c = texture(tex, vUv).rgb * pc.a.x;
  for (int i = 0; i < ITER / 4 + 1; ++i) c += texture(tex, vUv + pc.b.xy * float(i)).rgb * pc.b.z;
  c = MODE == 1 ? c / (1.0 + c) : aces(c);
  c = pow(c, vec3(1.0 / 2.2));
  if (MODE >= 2) {
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(vec3(l), c, pc.c.x) * pc.c.yzw;
  }
  if (MODE == 3) c += (fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
  o = vec4(c, 1.0) * vColor;
}
