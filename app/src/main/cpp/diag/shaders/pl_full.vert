#version 450
#extension GL_GOOGLE_include_directive : require
#include "pl_common.glsl"
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec4 inNormal;
layout(location = 0) out vec2 vUv;
layout(location = 1) out vec4 vColor;
layout(location = 2) out vec3 vNormal;
void main() {
  vec4 p = u.mvp * vec4(inPos, 1.0);
  if (MODE == 1) p.xy += sin(inPos.yx * pc.a.x) * pc.a.y;
  if (MODE >= 2) p.z = p.z * 0.5 + 0.25 * p.w;
  gl_Position = p;
  vUv = inUv * pc.b.xy + pc.b.zw;
  vColor = inColor * u.tint;
  vNormal = mat3(u.mvp) * inNormal.xyz;
}
