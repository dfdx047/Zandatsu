// Zandatsu - a GPU reinicia sem o jogo fechar: ver gpu_reset.cpp.
#pragma once

namespace swordman::gpu_reset {
// Liga o vigia (espera pelo primeiro frame).
void Start();
// A GPU travou ou falhou: o Renut deixa de submeter e o app volta ao launcher com o aviso.
void Trigger(const char* why);
bool Stopped();
}  // namespace swordman::gpu_reset
