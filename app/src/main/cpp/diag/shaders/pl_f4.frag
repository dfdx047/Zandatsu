#version 450
#extension GL_GOOGLE_include_directive : require
#include "pl_common.glsl"
layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec3 vNormal;
layout(location = 0) out vec4 o;
// Desfoque separavel de (2*ITER+1) taps com pesos gaussianos.
void main() {
  vec2 dir = MODE == 1 ? vec2(0.0, pc.a.y) : vec2(pc.a.x, 0.0);
  vec4 acc = texture(tex, vUv);
  float wsum = 1.0;
  for (int i = 1; i <= ITER; ++i) {
    float w = exp(-float(i * i) / (2.0 * pc.a.z * pc.a.z + 0.01));
    acc += w * (texture(tex, vUv + dir * float(i)) + texture(tex, vUv - dir * float(i)));
    wsum += 2.0 * w;
  }
  acc /= wsum;
  if (MODE >= 2) acc = max(acc - pc.b, 0.0) * pc.c.x;  // bloom threshold
  o = acc;
}
