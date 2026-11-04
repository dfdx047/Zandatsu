#version 450
#extension GL_GOOGLE_include_directive : require
#include "pl_common.glsl"
layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec3 vNormal;
layout(location = 0) out vec4 o;
// Ruido de valor fbm (ITER oitavas).
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  vec2 s = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash(i), hash(i + vec2(1, 0)), s.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), s.x), s.y);
}
void main() {
  vec2 p = vUv * pc.a.xy;
  float v = 0.0, amp = 0.5;
  for (int i = 0; i < ITER; ++i) {
    v += amp * (MODE == 1 ? abs(noise(p) * 2.0 - 1.0) : noise(p));
    p = mat2(1.6, 1.2, -1.2, 1.6) * p;
    amp *= 0.5;
  }
  if (MODE >= 2) v = smoothstep(pc.b.x, pc.b.y, v);
  o = vec4(vec3(v), 1.0) * vColor;
}
