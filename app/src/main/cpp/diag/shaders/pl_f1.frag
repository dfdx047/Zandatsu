#version 450
#extension GL_GOOGLE_include_directive : require
#include "pl_common.glsl"
layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec3 vNormal;
layout(location = 0) out vec4 o;
// Textura x cor do vertice; ITER amostras com deslocamento.
void main() {
  vec4 acc = vec4(0.0);
  for (int i = 0; i < ITER; ++i) {
    vec2 off = vec2(float(i) * pc.a.x, float(i & 3) * pc.a.y);
    acc += texture(tex, vUv + off);
  }
  acc /= float(ITER);
  if (MODE >= 2) acc *= texture(tex, vUv * 2.0).a;
  o = acc * vColor;
}
