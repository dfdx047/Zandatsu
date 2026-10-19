// dbz3: copies the scene depth (depth plane of the host depth render target)
// into an R32_FLOAT texture of the guest output size, as the temporal upscaler
// depth input. Uses the resolve-downscale root signature layout:
// b0 = constants, t0 = source, u0 = destination.
//
// Rebuild the header with:
//   fxc /nologo /T cs_5_1 /E main /O3 /Vn dbz3_depth_copy_cs
//       /Fh bytecode/d3d12_5_1/dbz3_depth_copy_cs.h dbz3_depth_copy.cs.hlsl

cbuffer Dbz3DepthCopyConstants : register(b0) {
  uint2 dbz3_size;     // Destination size in pixels.
  uint2 dbz3_offset;   // Source texel of destination (0, 0).
  uint dbz3_unused;
};

Texture2D<float> dbz3_source : register(t0);
RWTexture2D<float> dbz3_dest : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
  if (any(id.xy >= dbz3_size)) {
    return;
  }
  dbz3_dest[id.xy] = dbz3_source.Load(int3(id.xy + dbz3_offset, 0));
}
