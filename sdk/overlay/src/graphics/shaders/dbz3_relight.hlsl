// dbz3: "relight" (dbz3_relight_level), a subtle screen-space ambient
// occlusion on the 3D scene. It runs once per frame on the composed scene
// (after the bloom), before the HUD and the menus are drawn over it, with the
// scene depth copied earlier. Only the depth exists (no normals): positions are
// rebuilt from it with an assumed field of view; every distance is relative to
// the depth of the shaded pixel, so the unknown camera units cancel out.
//
//   ao_cs     depth -> occlusion (R8, 0 = open, 1 = fully occluded), horizon
//             based, directions rotated by a 4x4 pattern.
//   vs        full-screen triangle.
//   apply_ps  4x4 depth-aware blur (cancels the 4x4 pattern) and the factor
//             the scene color is multiplied by (blend ZERO / SRC_COLOR).
//
// Rebuild the headers (Windows SDK fxc) with:
//   fxc /nologo /T cs_5_1 /E ao_cs /O3 /Vn dbz3_relight_ao_cs
//       /Fh bytecode/d3d12_5_1/dbz3_relight_ao_cs.h dbz3_relight.hlsl
//   fxc /nologo /T vs_5_1 /E vs /O3 /Vn dbz3_relight_vs
//       /Fh bytecode/d3d12_5_1/dbz3_relight_vs.h dbz3_relight.hlsl
//   fxc /nologo /T ps_5_1 /E apply_ps /O3 /Vn dbz3_relight_apply_ps
//       /Fh bytecode/d3d12_5_1/dbz3_relight_apply_ps.h dbz3_relight.hlsl

cbuffer Dbz3RelightConstants : register(b0) {
  uint2 rl_size;         // Pixels processed (from the render target origin).
  float2 rl_tan_half;    // tan(fov / 2) horizontally and vertically.
  float rl_radius_px;    // Sampling radius in render target pixels.
  float rl_strength;     // Maximum darkening (0..1).
  uint rl_samples;       // Samples per pixel: directions x 4 steps (16 or 32).
  float rl_far_depth;    // Depth (0..1, 1 = far) where the effect has faded out.
  uint rl_debug;         // 1: the apply pass shows the factor, 2: the depth (no blending).
  uint3 rl_unused;
};

Texture2D<float> rl_depth : register(t0);
Texture2D<float> rl_ao : register(t1);
RWTexture2D<unorm float> rl_ao_out : register(u0);

// Distance along the view axis in units of the near plane (far plane at
// infinity, Direct3D 9 style: 0 = near, 1 = far).
float LinearZ(float d) { return 1.0 / max(1.0 - d, 1.0e-7); }

float3 ViewPos(int2 p, float d) {
  float2 ndc = (float2(p) + 0.5) / float2(rl_size) * float2(2.0, -2.0) + float2(-1.0, 1.0);
  float z = LinearZ(d);
  return float3(ndc * rl_tan_half * z, z);
}

float DepthAt(int2 p) {
  return rl_depth.Load(int3(clamp(p, int2(0, 0), int2(rl_size) - 1), 0));
}

static const float kBayer4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

[numthreads(8, 8, 1)]
void ao_cs(uint3 id : SV_DispatchThreadID) {
  if (any(id.xy >= rl_size)) {
    return;
  }
  int2 p = int2(id.xy);
  float d = DepthAt(p);
  if (d >= rl_far_depth) {
    rl_ao_out[id.xy] = 0.0;
    return;
  }
  float3 c = ViewPos(p, d);
  // Normal from the neighbours on the side with the smaller depth step, so
  // silhouettes don't bend it.
  float3 l = ViewPos(p - int2(1, 0), DepthAt(p - int2(1, 0)));
  float3 r = ViewPos(p + int2(1, 0), DepthAt(p + int2(1, 0)));
  float3 u = ViewPos(p - int2(0, 1), DepthAt(p - int2(0, 1)));
  float3 b = ViewPos(p + int2(0, 1), DepthAt(p + int2(0, 1)));
  float3 dx = abs(r.z - c.z) < abs(c.z - l.z) ? r - c : c - l;
  float3 dy = abs(b.z - c.z) < abs(c.z - u.z) ? b - c : c - u;
  float3 n = normalize(cross(dx, dy));
  if (dot(n, c) > 0.0) {
    n = -n;  // Toward the camera (at the origin).
  }
  // World radius matching the pixel radius at this depth.
  float radius = rl_radius_px * 2.0 * rl_tan_half.y * c.z / float(rl_size.y);
  float jitter = (kBayer4[(id.y & 3) * 4 + (id.x & 3)] + 0.5) / 16.0;
  // Horizon based: in each direction, how much the samples within the radius
  // rise above the surface (the increase of the highest one so far counts).
  uint directions = rl_samples >> 2, steps = rl_samples / directions;
  float occlusion = 0.0;
  for (uint k = 0; k < directions; ++k) {
    float2 dir;
    sincos((float(k) + jitter) * (6.2831853 / float(directions)), dir.y, dir.x);
    float horizon = 0.1;  // Bias: shallow bumps of a flat surface don't count.
    for (uint i = 0; i < steps; ++i) {
      float dist = 1.0 + (float(i) + jitter) / float(steps) * (rl_radius_px - 1.0);
      int2 q = p + int2(round(dir * dist));
      float3 v = ViewPos(q, DepthAt(q)) - c;
      float len = length(v);
      if (len < 1.0e-6 * c.z) {
        continue;
      }
      float elevation = dot(v, n) / len;
      if (elevation > horizon) {
        float rr = len / radius;
        occlusion += (elevation - horizon) * saturate(1.0 - rr * rr);
        horizon = elevation;
      }
    }
  }
  occlusion = saturate(occlusion / float(directions));
  // Fade out toward the far background (sky domes, distant scenery): from
  // half the distance of rl_far_depth to it.
  float z_far = LinearZ(rl_far_depth);
  occlusion *= saturate((z_far - c.z) / (0.5 * z_far));
  rl_ao_out[id.xy] = occlusion;
}

float4 vs(uint vertex_id : SV_VertexID) : SV_Position {
  float2 t = float2((vertex_id << 1) & 2, vertex_id & 2);
  return float4(t * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

float4 apply_ps(float4 position : SV_Position) : SV_Target {
  int2 p = int2(position.xy);
  float zc = LinearZ(DepthAt(p));
  if (rl_debug == 2) {
    // Depth view: red = log2 distance / 16, green = bands per doubling,
    // blue = beyond rl_far_depth (no effect).
    float lz = log2(zc);
    return float4(saturate(lz / 16.0), frac(lz), DepthAt(p) >= rl_far_depth ? 1.0 : 0.0, 1.0);
  }
  float sum = 0.0, weight = 0.0;
  [unroll] for (int y = -2; y < 2; ++y) {
    [unroll] for (int x = -2; x < 2; ++x) {
      int2 q = clamp(p + int2(x, y), int2(0, 0), int2(rl_size) - 1);
      float w = exp(-abs(LinearZ(DepthAt(q)) - zc) / zc * 40.0);
      sum += rl_ao.Load(int3(q, 0)) * w;
      weight += w;
    }
  }
  float occlusion = sum / max(weight, 1.0e-4);
  // Soft band rather than a gradient: reads as a contact shadow, closer to the
  // flat cel shading than a photographic falloff.
  float factor = 1.0 - rl_strength * smoothstep(0.03, 0.25, occlusion);
  return float4(factor, factor, factor, 1.0);
}
