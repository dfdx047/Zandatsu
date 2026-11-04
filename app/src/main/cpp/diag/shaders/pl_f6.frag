#version 450
#extension GL_GOOGLE_include_directive : require
#include "pl_common.glsl"
layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec3 vNormal;
layout(location = 0) out vec4 o;
// Ramificacao dependente de dados + discard (alpha test).
void main() {
  vec4 t = texture(tex, vUv);
  if (MODE == 0 && t.a < pc.a.w) discard;
  vec4 c;
  int sel = int(t.r * 4.0) & 3;
  if (sel == 0) c = t * vColor;
  else if (sel == 1) c = vec4(t.gbr, 1.0);
  else if (sel == 2) c = mix(t, vColor, pc.a.x);
  else c = vec4(fract(t.rgb * float(ITER)), t.a);
  if (MODE == 2 && c.a < 0.5) discard;
  if (MODE == 3) c.rgb = floor(c.rgb * float(ITER)) / float(ITER);
  o = c;
}
