// dbz3: motion replay pixel shader. The motion variant of the guest vertex
// shader outputs the host clip position computed with the previous frame's
// constants (TEXCOORD0) and with the current ones (TEXCOORD1), both with the
// same viewport transform and jitter, so the difference is unjittered. The
// result is in NDC units (current to previous); the upscaler scales it to
// pixels with (0.5 * width, -0.5 * height).
//
// Target 1 is the reactive mask. The pipeline decides what reaches it: opaque
// draws blend it to 0 (they cover whatever was translucent there), translucent
// draws (shadows, effects) write this value and leave the motion untouched.
//
// Rebuild the header with:
//   fxc /nologo /T ps_5_1 /E main /O3 /Vn dbz3_motion_ps
//       /Fh bytecode/d3d12_5_1/dbz3_motion_ps.h dbz3_motion.ps.hlsl

struct Output {
  float2 motion : SV_Target0;
  float reactive : SV_Target1;
};

Output main(float4 prev_clip : TEXCOORD0, float4 curr_clip : TEXCOORD1) {
  Output output;
  output.motion = float2(0.0, 0.0);
  if (prev_clip.w > 0.0 && curr_clip.w > 0.0) {
    output.motion = prev_clip.xy / prev_clip.w - curr_clip.xy / curr_clip.w;
  }
  output.reactive = 0.9;
  return output;
}
