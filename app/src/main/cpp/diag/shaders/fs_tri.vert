#version 450
// Triangulo que cobre o alvo inteiro (sem vertex buffer); z vem das push constants.
layout(push_constant) uniform P { vec4 color; vec4 z; } pc;
void main() {
  vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(p * 2.0 - 1.0, pc.z.x, 1.0);
}
