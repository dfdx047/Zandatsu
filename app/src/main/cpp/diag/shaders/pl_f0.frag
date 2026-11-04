#version 450
#extension GL_GOOGLE_include_directive : require
#include "pl_common.glsl"
layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec3 vNormal;
layout(location = 0) out vec4 o;
// Cor lisa + tint.
void main() {
  vec4 c = vColor * pc.a;
  if (MODE == 1) c = c.bgra;
  if (MODE == 2) c.rgb = vec3(dot(c.rgb, vec3(0.299, 0.587, 0.114)));
  if (MODE == 3) c = 1.0 - c;
  o = c;
}
