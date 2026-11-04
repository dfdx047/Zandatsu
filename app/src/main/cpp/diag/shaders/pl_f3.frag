#version 450
#extension GL_GOOGLE_include_directive : require
#include "pl_common.glsl"
layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec3 vNormal;
layout(location = 0) out vec4 o;
// Blinn-Phong com ate 8 luzes do UBO.
void main() {
  vec3 n = normalize(vNormal);
  vec3 view = normalize(vec3(vUv - 0.5, 1.0));
  vec3 albedo = texture(tex, vUv).rgb * vColor.rgb;
  vec3 c = albedo * u.params.x;
  for (int i = 0; i < min(ITER, 8); ++i) {
    vec3 l = u.lights[i].xyz - vec3(vUv, 0.0);
    float d = length(l);
    l /= d;
    vec3 h = normalize(l + view);
    float att = 1.0 / (1.0 + u.lights[i].w * d * d);
    float spec = pow(max(dot(n, h), 0.0), MODE == 3 ? 128.0 : 32.0);
    c += att * (albedo * max(dot(n, l), 0.0) + (MODE >= 1 ? spec : 0.0));
  }
  if (MODE == 2) c = c / (c + 1.0);
  o = vec4(c, vColor.a);
}
