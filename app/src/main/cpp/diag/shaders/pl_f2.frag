#version 450
#extension GL_GOOGLE_include_directive : require
#include "pl_common.glsl"
layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec3 vNormal;
layout(location = 0) out vec4 o;
// Matematica transcendental pesada.
void main() {
  vec3 v = vec3(vUv, pc.a.x);
  for (int i = 0; i < ITER * 4; ++i) {
    v = sin(v * 1.7 + v.yzx) + cos(v.zxy * 2.3);
    if (MODE == 1) v = exp2(v * 0.25) - log2(abs(v) + 1.0);
    if (MODE >= 2) v = v / (1.0 + abs(v)) + pow(abs(v), vec3(1.3));
  }
  o = vec4(v * 0.5 + 0.5, 1.0) * vColor;
}
