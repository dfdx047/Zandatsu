#version 450
#extension GL_GOOGLE_include_directive : require
#include "pl_common.glsl"
layout(location = 0) in vec3 inPos;
layout(location = 0) out vec2 vUv;
layout(location = 1) out vec4 vColor;
layout(location = 2) out vec3 vNormal;
void main() {
  gl_Position = u.mvp * vec4(inPos, 1.0);
  vUv = inPos.xy * 0.5 + 0.5;
  vColor = u.tint;
  vNormal = normalize(vec3(inPos.xy, 1.0));
}
