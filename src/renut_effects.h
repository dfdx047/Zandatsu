// Opções gráficas do MGR no Renut (menu Desempenho): sombras e brilho, ligados ou desligados ao vivo. Chamado pelo
// Renut (patch renut/0016) na thread de render.
//
// Mapa do frame (dumps de 24/10, swordman_renut_frame_dump; título, menus, pausa, luta, MG RAY):
// - Sombras: o sol desenha 2 cascatas num alvo de profundidade 1024x1024, cada uma resolvida para um atlas
//   1024x2048 (profundidade). Um quad de ecrã inteiro (PS 15796848…, B59EE8F6… no título) lê o atlas e a
//   profundidade linear e escreve a máscara de sombra 1280x720, limpa a branco antes; os materiais leem a máscara.
//   Só esse quad lê o atlas. Desligadas: os draws das cascatas não são desenhados (os clears e os resolves ficam) e
//   o quad passa a ser um clear a branco (1,1,1,1) do seu alvo: a máscara fica escrita e "sem sombra".
// - Brilho: cadeia em 320x176 (redução 07A5E482…, desfoque 0AD31892… e 240A635A…, soma F71DC5AC…) lida pela
//   composição final (2173EC47…) como textura 1. Desligado: cada draw da cadeia passa a ser um clear a preto do
//   seu alvo, as texturas da cadeia ficam escritas a 0 e a composição soma 0 (a imagem original).
// - SSAO, profundidade de campo e motion blur: nenhum passe nos frames do jogo (a abertura do R-00 é vídeo).
#pragma once

#include <cstdint>

namespace swordman::renut_effects {

// swordman_renut_shadows / swordman_renut_bloom (lidos a cada chamada: mudam ao vivo).
bool ShadowsOff();
bool BloomOff();

// Cada resolve: um de profundidade de um alvo quadrado >= 512 é uma cascata (alvo e atlas ficam conhecidos).
void NoteResolve(bool depth, uint32_t width, uint32_t height, uint32_t source_va, uint32_t dest_texture_va);
// O alvo de profundidade das cascatas.
bool IsShadowDepth(uint32_t depth_va);
// O atlas das cascatas.
bool IsShadowAtlas(uint32_t texture_va);
// Um pixel shader da cadeia do brilho.
bool IsBloomShader(uint64_t ps_hash);

}  // namespace swordman::renut_effects
