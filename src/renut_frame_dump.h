// Dump de um frame do Renut (swordman_renut_frame_dump, desligado por padrão), para mapear os passes do MGR: cada
// draw, clear e resolve pela ordem, com os shaders, os alvos e as texturas que lê. Chamado pelo Renut (patch
// renut/0015) na thread de render.
//
// Pedido por ficheiro: criar files/frame_dump.req no aparelho; o frame seguinte vai para
// logs/frame_dump_<N>.txt e o pedido é apagado.
#pragma once

#include <cstdint>
#include <string>

namespace swordman::frame_dump {

// swordman_renut_frame_dump (lido uma vez).
bool On();
// No início de cada frame do Renut (frame = TextureFrameLocked()): vê o pedido e abre/fecha o ficheiro.
// Devolve se este frame é escrito.
bool Active(uint64_t frame);
// Uma linha do frame em curso (só com Active).
void Line(const std::string& line);

}  // namespace swordman::frame_dump
