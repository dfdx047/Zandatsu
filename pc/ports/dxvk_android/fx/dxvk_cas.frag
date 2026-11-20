#version 450
// "CAS only" mode: contrast adaptive sharpening (the idea of AMD FidelityFX CAS, written here without its
// header) with the upscale in the same pass, straight onto the swap chain image, plus the D3D9 gamma ramp.
// For each output pixel: the 2x2 back buffer texels around its position are each sharpened with their
// cross neighbours (12 distinct texels), and the 4 results are blended bilinearly. The sharpening weight
// per texel: w = -amp / lerp(8, 5, sharpness), amp = sqrt(clamp(min(mn, 1 - mx) / mx)), out =
// (centre + w * sum(cross)) / (1 + 4 w): strong on soft detail, weak where the contrast is already high.
// Without scaling (back buffer as big as the window) only the centre texel is used.
// DXVK pipeline spec constant 0 (id 1225, DxvkSpecConstantId::FirstPipelineConstant): upscaling or 1:1.
layout(constant_id = 1225) const bool s_scaling = true;
layout(binding = 0) uniform sampler2D s_image;   // read by whole texels
layout(binding = 1) uniform sampler1D s_gamma;
layout(location = 0) in vec2 i_coord;
layout(location = 0) out vec4 o_color;
layout(push_constant) uniform cas_t {
  vec2 scale;     // back buffer pixels per output pixel
  ivec2 size;     // back buffer size
  float peak;     // -1 / lerp(8, 5, sharpness)
  uint gamma;
} u;

vec3 Load(ivec2 p) { return texelFetch(s_image, clamp(p, ivec2(0), u.size - 1), 0).rgb; }

vec3 Sharpen(vec3 c, vec3 n, vec3 w, vec3 e, vec3 s) {
  vec3 mn = min(c, min(min(n, w), min(e, s)));
  vec3 mx = max(c, max(max(n, w), max(e, s)));
  vec3 amp = sqrt(clamp(min(mn, 1.0 - mx) / max(mx, vec3(1.0 / 65536.0)), 0.0, 1.0));
  vec3 wt = amp * u.peak;
  return clamp((c + wt * (n + w + e + s)) / (1.0 + 4.0 * wt), 0.0, 1.0);
}

void main() {
  vec3 c;
  if (s_scaling) {
    vec2 p = gl_FragCoord.xy * u.scale - 0.5;
    ivec2 i = ivec2(floor(p));
    vec2 f = p - vec2(i);
    // a b      the 2x2 texels; each is sharpened with its cross neighbours
    // c d
    vec3 a = Load(i), b = Load(i + ivec2(1, 0)), cc = Load(i + ivec2(0, 1)), d = Load(i + ivec2(1, 1));
    vec3 an = Load(i + ivec2(0, -1)), bn = Load(i + ivec2(1, -1));
    vec3 aw = Load(i + ivec2(-1, 0)), cw = Load(i + ivec2(-1, 1));
    vec3 be = Load(i + ivec2(2, 0)), de = Load(i + ivec2(2, 1));
    vec3 cs = Load(i + ivec2(0, 2)), ds = Load(i + ivec2(1, 2));
    vec3 sa = Sharpen(a, an, aw, b, cc);
    vec3 sb = Sharpen(b, bn, a, be, d);
    vec3 sc = Sharpen(cc, a, cw, d, cs);
    vec3 sd = Sharpen(d, b, cc, de, ds);
    c = mix(mix(sa, sb, f.x), mix(sc, sd, f.x), f.y);
  } else {
    ivec2 i = ivec2(gl_FragCoord.xy);
    c = Sharpen(Load(i), Load(i + ivec2(0, -1)), Load(i + ivec2(-1, 0)), Load(i + ivec2(1, 0)), Load(i + ivec2(0, 1)));
  }
  if (u.gamma != 0u)
    c = vec3(texture(s_gamma, c.r).r, texture(s_gamma, c.g).g, texture(s_gamma, c.b).b);
  o_color = vec4(c, 1.0);
}
