#version 450
#extension GL_GOOGLE_include_directive : require
#include "pl_common.glsl"
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inInst;  // por instancia: xyz deslocamento, w escala
layout(location = 0) out vec2 vUv;
layout(location = 1) out vec4 vColor;
layout(location = 2) out vec3 vNormal;
void main() {
  vec3 p = inPos * inInst.w + inInst.xyz;
  gl_Position = u.mvp * vec4(p, 1.0);
  vUv = p.xy;
  vColor = vec4(fract(inInst.xyz * 7.31), 1.0);
  vNormal = vec3(0.0, 0.0, 1.0);
}
