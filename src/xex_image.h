// A imagem PE desencriptada e descomprimida de um default.xex (XEX2), sem o runtime: o mesmo código no aparelho
// (libmain.so, JNI do import) e no PC (tools/xex-image, build de release). A chave do libgame.so é o SHA-256 dela.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zandatsu {

struct XexImage {
  std::vector<uint8_t> pe;  // a imagem (security_info.image_size bytes)
  uint32_t title_id = 0;    // do cabeçalho de execução (0 se não houver)
  std::string error;        // vazio = ok
};

// Lê o XEX da memória. Só a chave retail (é o disco do jogador).
XexImage ReadXexImage(const uint8_t* xex, size_t size);

}  // namespace zandatsu
