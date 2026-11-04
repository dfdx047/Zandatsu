// Interface comum dos shaders do teste de pipelines (pipelines_serial/parallel).
// MODE e ITER sao constantes de especializacao: o driver compila caminhos diferentes por variante.
layout(constant_id = 0) const int MODE = 0;
layout(constant_id = 1) const int ITER = 4;
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(set = 0, binding = 1) uniform U {
  mat4 mvp;
  vec4 lights[8];
  vec4 params;
  vec4 tint;
} u;
layout(push_constant) uniform P { vec4 a; vec4 b; vec4 c; vec4 d; } pc;
