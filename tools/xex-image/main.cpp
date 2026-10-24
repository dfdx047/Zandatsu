// Ferramenta de PC do build de release: escreve a imagem PE de um default.xex (src/xex_image.cpp, o mesmo código do
// aparelho). Uso: xex-image <default.xex> <saída.bin>. Imprime o title ID.
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

#include "xex_image.h"

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "uso: %s <default.xex> <saida.bin>\n", argv[0]);
    return 2;
  }
  std::ifstream in(argv[1], std::ios::binary);
  std::vector<uint8_t> xex((std::istreambuf_iterator<char>(in)), {});
  const auto img = zandatsu::ReadXexImage(xex.data(), xex.size());
  if (!img.error.empty()) {
    std::fprintf(stderr, "%s: %s\n", argv[1], img.error.c_str());
    return 1;
  }
  std::ofstream(argv[2], std::ios::binary).write(reinterpret_cast<const char*>(img.pe.data()), img.pe.size());
  std::printf("%08X %zu\n", img.title_id, img.pe.size());
  return 0;
}
