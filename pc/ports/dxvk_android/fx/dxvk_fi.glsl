// Frame generation for DXVK_FI=1 (30 -> 60): the body of dxvk_fi_p1..p4.comp (each defines PASS first).
// Ported from the Xbox 360 port's patches/0092-frame-interpolation.patch (dbz3_frame_interp.comp, same
// maths, validated there with a numpy model); only the bindings changed (DXVK resource slots, no set).
// No motion vectors from the game: motion is estimated from the two frames themselves.
//
// It runs on the game's back buffer (its render size, e.g. 854x480), before the present upscale, so the
// generated frame then goes through the same FSR / CAS / blit as the real ones (present_fx.cpp).
//
// PASS 1  luma+chroma pyramid of the new frame: Q (1/4k) and S (1/16k), k = round(width / 1280),
//         as (Y, U + 0.5, V + 0.5) in rgba8; resets the counters.
// PASS 2  coarse motion on S: symmetric search +-3 texels (prev(c - v/2) against cur(c + v/2)),
//         3x3 window, then half a texel of refinement.
// PASS 3  motion per 8k x 8k block on Q: candidates = the S vector of the block and its 4
//         neighbours (cross) + zero, then +-1 and +-0.5 Q texel; 4x4 window. Blocks without a
//         match count for the scene cut test.
// PASS 4  per pixel: unchanged pixel -> the new frame (HUD, still background); scene cut -> the
//         new frame; otherwise the best of {interpolated vector, block vector (, 4 neighbours)}
//         by the colour error between the two samples plus a coherence penalty, and on a mismatch
//         (occlusion) the side that is coherent, or a plain blend. Also copies the new frame into
//         the history. mode 1 = only the history copy (first frame, size change, long gap).
//         quality 0 = plain blend of the two frames (passes 2 and 3 are then skipped).

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D u_cur;    // guest output (new frame)
layout(binding = 1) uniform sampler2D u_prev;   // history (previous frame)
layout(binding = 2, rgba8) uniform writeonly image2D u_hist_out;
layout(binding = 3, rgba8) uniform writeonly image2D u_gen;
layout(binding = 4) uniform sampler2D u_q_prev;
layout(binding = 5) uniform sampler2D u_q_cur;
layout(binding = 6, rgba8) uniform writeonly image2D u_q_out;
layout(binding = 7) uniform sampler2D u_s_prev;
layout(binding = 8) uniform sampler2D u_s_cur;
layout(binding = 9, rgba8) uniform writeonly image2D u_s_out;
layout(binding = 10, rgba16f) uniform image2D u_vs;
layout(binding = 11, rgba16f) uniform writeonly image2D u_v8_out;
layout(binding = 12) uniform sampler2D u_v8;
layout(binding = 13, std430) buffer Counters {
  uint bad_blocks;
  uint fallback_pixels;
  uint static_pixels;
  uint cut;
} u_counters;

layout(push_constant) uniform Constants {
  ivec2 full_size;
  ivec2 q_size;
  ivec2 s_size;
  ivec2 b_size;
  int k;
  int quality;
  int mode;
  int pad;
} pc;

const int R_COARSE = 3;
const vec3 YUV_OFFSET = vec3(0.0, 0.5, 0.5);
const float LAMBDA_COARSE = 0.002;
const float LAMBDA_REFINE = 0.0005;
const float T_CUT_BLOCK = 0.15;
const float EPS_STATIC = 1.5 / 255.0;
const float T_LO = 0.03;
const float T_HI = 0.10;
const float K_COHERENCE = 0.004;
const float COHERENCE_CAP = 32.0;
const float COHERENT_PX = 4.0;

// (Y, (B - Y) / 2, (R - Y) / 2)
vec3 Yuv(vec3 rgb) {
  float y = dot(rgb, vec3(0.299, 0.587, 0.114));
  return vec3(y, (rgb.b - y) * 0.5, (rgb.r - y) * 0.5);
}

float YuvDiff(vec3 a, vec3 b) {
  vec3 d = abs(a - b);
  return d.x + d.y + d.z;
}

// Pixel coordinates (texel centres at i + 0.5) -> linear sample with the clamp sampler.
vec3 At(sampler2D t, vec2 p, ivec2 size) {
  return textureLod(t, p / vec2(size), 0.0).rgb;
}

#if PASS == 1

shared vec3 s_q[8][8];

void main() {
  ivec2 lid = ivec2(gl_LocalInvocationID.xy);
  ivec2 q = ivec2(gl_GlobalInvocationID.xy);
  if (gl_GlobalInvocationID.x == 0u && gl_GlobalInvocationID.y == 0u) {
    u_counters.bad_blocks = 0u;
    u_counters.fallback_pixels = 0u;
    u_counters.static_pixels = 0u;
    u_counters.cut = 0u;
  }
  int d = 4 * pc.k;
  vec3 acc = vec3(0.0);
  for (int b = 0; b < 2 * pc.k; ++b) {
    for (int a = 0; a < 2 * pc.k; ++a) {
      vec2 p = vec2(float(q.x * d + 1 + 2 * a), float(q.y * d + 1 + 2 * b));
      acc += Yuv(At(u_cur, p, pc.full_size));
    }
  }
  acc /= float(4 * pc.k * pc.k);
  if (all(lessThan(q, pc.q_size))) {
    imageStore(u_q_out, q, vec4(acc + YUV_OFFSET, 0.0));
  }
  s_q[lid.y][lid.x] = acc;
  barrier();
  if (lid.x < 2 && lid.y < 2) {
    vec3 sum = vec3(0.0);
    for (int y = 0; y < 4; ++y) {
      for (int x = 0; x < 4; ++x) {
        sum += s_q[lid.y * 4 + y][lid.x * 4 + x];
      }
    }
    ivec2 s = ivec2(gl_WorkGroupID.xy) * 2 + lid;
    if (all(lessThan(s, pc.s_size))) {
      imageStore(u_s_out, s, vec4(sum / 16.0 + YUV_OFFSET, 0.0));
    }
  }
}

#elif PASS == 2

float CoarseCost(vec2 c, vec2 v) {
  float cost = 0.0;
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      vec2 o = c + vec2(float(dx), float(dy));
      cost += YuvDiff(At(u_s_prev, o - v * 0.5, pc.s_size), At(u_s_cur, o + v * 0.5, pc.s_size));
    }
  }
  return cost / 9.0 + LAMBDA_COARSE * (abs(v.x) + abs(v.y));
}

void main() {
  ivec2 s = ivec2(gl_GlobalInvocationID.xy);
  if (any(greaterThanEqual(s, pc.s_size))) {
    return;
  }
  vec2 c = vec2(s) + 0.5;
  float best = 1.0e30;
  vec2 bv = vec2(0.0);
  for (int vy = -R_COARSE; vy <= R_COARSE; ++vy) {
    for (int vx = -R_COARSE; vx <= R_COARSE; ++vx) {
      vec2 v = vec2(float(vx), float(vy));
      float cost = CoarseCost(c, v);
      if (cost < best) {
        best = cost;
        bv = v;
      }
    }
  }
  vec2 cv = bv;
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      vec2 v = cv + vec2(float(dx), float(dy)) * 0.5;
      float cost = CoarseCost(c, v);
      if (cost < best) {
        best = cost;
        bv = v;
      }
    }
  }
  imageStore(u_vs, s, vec4(bv * float(16 * pc.k), best, 0.0));
}

#elif PASS == 3

shared uint s_bad;

float RefineCost(vec2 c, vec2 v) {
  float cost = 0.0;
  for (int oy = 0; oy < 4; ++oy) {
    for (int ox = 0; ox < 4; ++ox) {
      vec2 o = c + vec2(float(ox) - 1.5, float(oy) - 1.5);
      cost += YuvDiff(At(u_q_prev, o - v * 0.5, pc.q_size), At(u_q_cur, o + v * 0.5, pc.q_size));
    }
  }
  return cost / 16.0;
}

void main() {
  if (gl_LocalInvocationIndex == 0u) {
    s_bad = 0u;
  }
  barrier();
  ivec2 b = ivec2(gl_GlobalInvocationID.xy);
  bool inside = all(lessThan(b, pc.b_size));
  if (inside) {
    vec2 c = vec2(b) * 2.0 + 1.0;
    float q2px = float(4 * pc.k);
    float best = RefineCost(c, vec2(0.0));
    float best_raw = best;
    vec2 bv = vec2(0.0);
    ivec2 s = b / 2;
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx != 0 && dy != 0) {
          continue;
        }
        ivec2 n = clamp(s + ivec2(dx, dy), ivec2(0), pc.s_size - 1);
        vec2 v = imageLoad(u_vs, n).xy / q2px;
        float raw = RefineCost(c, v);
        float cost = raw + LAMBDA_REFINE * (abs(v.x) + abs(v.y));
        if (cost < best) {
          best = cost;
          best_raw = raw;
          bv = v;
        }
      }
    }
    for (int step_i = 0; step_i < 2; ++step_i) {
      float step = step_i == 0 ? 1.0 : 0.5;
      vec2 cv = bv;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) {
            continue;
          }
          vec2 v = cv + vec2(float(dx), float(dy)) * step;
          float raw = RefineCost(c, v);
          float cost = raw + LAMBDA_REFINE * (abs(v.x) + abs(v.y));
          if (cost < best) {
            best = cost;
            best_raw = raw;
            bv = v;
          }
        }
      }
    }
    imageStore(u_v8_out, b, vec4(bv * q2px, best_raw, 0.0));
    if (best_raw > T_CUT_BLOCK) {
      atomicAdd(s_bad, 1u);
    }
  }
  barrier();
  if (gl_LocalInvocationIndex == 0u && s_bad != 0u) {
    atomicAdd(u_counters.bad_blocks, s_bad);
  }
}

#elif PASS == 4

shared uint s_fallback;
shared uint s_static;

vec2 BlockVec(vec2 p, float bsz) {
  ivec2 i = clamp(ivec2(floor(p / bsz)), ivec2(0), pc.b_size - 1);
  return texelFetch(u_v8, i, 0).xy;
}

void main() {
  if (gl_LocalInvocationIndex == 0u) {
    s_fallback = 0u;
    s_static = 0u;
  }
  barrier();
  ivec2 px = ivec2(gl_GlobalInvocationID.xy);
  if (all(lessThan(px, pc.full_size))) {
    vec3 c0 = texelFetch(u_cur, px, 0).rgb;
    imageStore(u_hist_out, px, vec4(c0, 1.0));
    if (pc.mode == 0) {
      vec3 p0 = texelFetch(u_prev, px, 0).rgb;
      vec3 d0 = abs(p0 - c0);
      vec3 blend0 = 0.5 * (p0 + c0);
      vec3 result;
      bool cut = u_counters.bad_blocks * 2u > uint(pc.b_size.x * pc.b_size.y);
      if (max(d0.r, max(d0.g, d0.b)) <= EPS_STATIC || cut) {
        result = c0;
        atomicAdd(s_static, 1u);
      } else if (pc.quality <= 0) {
        result = blend0;
      } else {
        vec2 p = vec2(px) + 0.5;
        float bsz = float(8 * pc.k);
        vec2 cands[6];
        int count = 2;
        cands[0] = textureLod(u_v8, p / bsz / vec2(pc.b_size), 0.0).xy;
        ivec2 ib = clamp(ivec2(floor(p / bsz)), ivec2(0), pc.b_size - 1);
        cands[1] = texelFetch(u_v8, ib, 0).xy;
        if (pc.quality >= 2) {
          cands[2] = texelFetch(u_v8, clamp(ib + ivec2(-1, 0), ivec2(0), pc.b_size - 1), 0).xy;
          cands[3] = texelFetch(u_v8, clamp(ib + ivec2(1, 0), ivec2(0), pc.b_size - 1), 0).xy;
          cands[4] = texelFetch(u_v8, clamp(ib + ivec2(0, -1), ivec2(0), pc.b_size - 1), 0).xy;
          cands[5] = texelFetch(u_v8, clamp(ib + ivec2(0, 1), ivec2(0), pc.b_size - 1), 0).xy;
          count = 6;
        }
        // The zero vector first, without a coherence penalty.
        float best_s = (d0.r + d0.g + d0.b) / 3.0;
        float best_e = best_s;
        vec3 best_p = p0;
        vec3 best_c = c0;
        bool ok_p = true;
        bool ok_c = true;
        for (int i = 0; i < count; ++i) {
          vec2 v = cands[i];
          vec2 sp = p - v * 0.5;
          vec2 sc = p + v * 0.5;
          vec3 pw = At(u_prev, sp, pc.full_size);
          vec3 cw = At(u_cur, sc, pc.full_size);
          vec3 d = abs(pw - cw);
          float e = (d.r + d.g + d.b) / 3.0;
          vec2 vp = BlockVec(sp, bsz) - v;
          vec2 vc = BlockVec(sc, bsz) - v;
          float dp = abs(vp.x) + abs(vp.y);
          float dc = abs(vc.x) + abs(vc.y);
          float score = e + K_COHERENCE * min(dp + dc, COHERENCE_CAP);
          if (score < best_s) {
            best_s = score;
            best_e = e;
            best_p = pw;
            best_c = cw;
            ok_p = dp <= COHERENT_PX;
            ok_c = dc <= COHERENT_PX;
          }
        }
        vec3 avg = 0.5 * (best_p + best_c);
        vec3 fallback = ok_p != ok_c ? (ok_c ? best_c : best_p) : blend0;
        float w = clamp((best_e - T_LO) / (T_HI - T_LO), 0.0, 1.0);
        result = mix(avg, fallback, w);
        if (w > 0.5) {
          atomicAdd(s_fallback, 1u);
        }
      }
      imageStore(u_gen, px, vec4(result, 1.0));
    }
  }
  barrier();
  if (gl_LocalInvocationIndex == 0u) {
    if (s_fallback != 0u) {
      atomicAdd(u_counters.fallback_pixels, s_fallback);
    }
    if (s_static != 0u) {
      atomicAdd(u_counters.static_pixels, s_static);
    }
  }
}

#endif
