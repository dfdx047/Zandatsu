// Imagem PE de um XEX2: ver xex_image.h. Os passos são os do XexModule::ReadImage* do SDK (Xenia), sem a memória do
// guest: chave de sessão (AES-128 da chave retail), desencriptação AES-128-CBC e, conforme o formato, cópia, blocos
// "basic" (dados + zeros) ou LZX ("normal": blocos com SHA-1, pedaços de 16 bits, depois lzxd do libmspack).

#include "xex_image.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>

extern "C" {
#include <lzx.h>
#include <mspack.h>
}

#include "crypto/TinySHA1.hpp"
#include "crypto/rijndael-alg-fst.c"
#include "crypto/rijndael-alg-fst.h"

namespace zandatsu {
namespace {

constexpr uint8_t kRetailKey[16] = {0x20, 0xB1, 0x85, 0xA5, 0x9D, 0x28, 0xFD, 0xC3,
                                    0x40, 0x58, 0x3F, 0xBB, 0x08, 0x96, 0xBF, 0x91};
constexpr uint32_t kFileFormatInfo = 0x000003FF;
constexpr uint32_t kExecutionInfo = 0x00040006;

uint32_t Be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
uint16_t Be16(const uint8_t* p) { return uint16_t(p[0] << 8 | p[1]); }

// AES-128-CBC com IV a zero (tamanho múltiplo de 16; o resto fica como está).
void AesCbcDecrypt(const uint8_t* key, const uint8_t* in, size_t size, uint8_t* out) {
  uint32_t rk[4 * (MAXNR + 1)];
  const int nr = rijndaelKeySetupDec(rk, key, 128);
  uint8_t iv[16] = {}, ct[16];
  for (size_t n = 0; n + 16 <= size; n += 16) {
    std::memcpy(ct, in + n, 16);
    rijndaelDecrypt(rk, nr, ct, out + n);
    for (int i = 0; i < 16; ++i) out[n + i] ^= iv[i];
    std::memcpy(iv, ct, 16);
  }
}

// libmspack sobre memória (como o lzx.cpp do SDK).
struct MemFile {
  mspack_system sys;
  uint8_t* buf;
  off_t size, pos;
};
int MemRead(mspack_file* f, void* b, int n) {
  auto* m = reinterpret_cast<MemFile*>(f);
  const off_t c = std::min<off_t>(n, m->size - m->pos);
  std::memcpy(b, m->buf + m->pos, c);
  m->pos += c;
  return int(c);
}
int MemWrite(mspack_file* f, void* b, int n) {
  auto* m = reinterpret_cast<MemFile*>(f);
  const off_t c = std::min<off_t>(n, m->size - m->pos);
  std::memcpy(m->buf + m->pos, b, c);
  m->pos += c;
  return int(c);
}
void* MemAlloc(mspack_system*, size_t n) { return std::calloc(n, 1); }
void MemFree(void* p) { std::free(p); }
void MemCopy(void* s, void* d, size_t n) { std::memcpy(d, s, n); }

bool Lzx(const uint8_t* src, size_t src_size, uint8_t* dst, size_t dst_size, uint32_t window) {
  if (!window || (window & (window - 1)) || src_size >= INT_MAX) return false;
  const int bits = __builtin_ctz(window);
  mspack_system sys = {};
  sys.read = MemRead;
  sys.write = MemWrite;
  sys.alloc = MemAlloc;
  sys.free = MemFree;
  sys.copy = MemCopy;
  MemFile in{sys, const_cast<uint8_t*>(src), off_t(src_size), 0};
  MemFile out{sys, dst, off_t(dst_size), 0};
  lzxd_stream* s = lzxd_init(&sys, reinterpret_cast<mspack_file*>(&in), reinterpret_cast<mspack_file*>(&out), bits, 0,
                             0x8000, off_t(dst_size), 0);
  if (!s) return false;
  const int r = lzxd_decompress(s, off_t(dst_size));
  lzxd_free(s);
  return r == MSPACK_ERR_OK;
}

}  // namespace

XexImage ReadXexImage(const uint8_t* xex, size_t size) {
  XexImage r;
  auto fail = [&](const char* e) {
    r.pe.clear();
    r.error = e;
    return r;
  };
  if (size < 0x18 || std::memcmp(xex, "XEX2", 4)) return fail("não é um XEX2");
  const uint32_t header_size = Be32(xex + 0x8), security = Be32(xex + 0x10), count = Be32(xex + 0x14);
  if (header_size > size || security + 0x184 > size || 0x18 + uint64_t(count) * 8 > size) return fail("cabeçalho cortado");

  const uint8_t* format = nullptr;
  for (uint32_t i = 0; i < count; ++i) {
    const uint8_t* h = xex + 0x18 + i * 8;
    const uint32_t key = Be32(h), value = Be32(h + 4);
    if (key == kFileFormatInfo && value + 8 <= size) format = xex + value;
    if (key == kExecutionInfo && value + 0x10 <= size) r.title_id = Be32(xex + value + 0xC);
  }
  if (!format) return fail("sem informação de formato");
  const uint32_t info_size = Be32(format);
  const uint16_t encryption = Be16(format + 4), compression = Be16(format + 6);
  const uint32_t image_size = Be32(xex + security + 0x4);
  if (encryption > 1) return fail("encriptação desconhecida");

  uint8_t session[16];
  AesCbcDecrypt(kRetailKey, xex + security + 0x150, 16, session);

  // Os dados depois do cabeçalho, desencriptados.
  const size_t data_size = size - header_size;
  std::vector<uint8_t> data(xex + header_size, xex + size);
  if (encryption == 1) AesCbcDecrypt(session, xex + header_size, data_size & ~size_t(15), data.data());

  switch (compression) {
    case 0:  // sem compressão
      r.pe = std::move(data);
      break;
    case 1: {  // basic: blocos de dados seguidos de zeros
      if (info_size < 8) return fail("blocos basic inválidos");
      const uint32_t blocks = (info_size - 8) / 8;
      size_t total = 0;
      for (uint32_t i = 0; i < blocks; ++i) total += Be32(format + 8 + i * 8) + Be32(format + 12 + i * 8);
      r.pe.assign(total, 0);
      size_t src = 0, dst = 0;
      for (uint32_t i = 0; i < blocks; ++i) {
        const uint32_t d = Be32(format + 8 + i * 8), z = Be32(format + 12 + i * 8);
        if (src + d > data.size()) return fail("blocos basic cortados");
        std::memcpy(r.pe.data() + dst, data.data() + src, d);
        src += d;
        dst += d + z;
      }
      break;
    }
    case 2: {  // normal: LZX em blocos encadeados (tamanho e SHA-1 do bloco seguinte no início de cada um)
      const uint32_t window = Be32(format + 8);
      uint32_t block_size = Be32(format + 12);
      uint8_t block_hash[20];
      std::memcpy(block_hash, format + 16, 20);
      std::vector<uint8_t> lzx;
      lzx.reserve(data.size());
      size_t p = 0;
      while (block_size) {
        if (p + block_size > data.size() || block_size < 24) return fail("blocos LZX cortados");
        const uint8_t* b = data.data() + p;
        uint8_t digest[20];
        sha1::SHA1 s;
        s.processBytes(b, block_size);
        s.finalize(digest);
        if (std::memcmp(digest, block_hash, 20)) return fail("SHA-1 de um bloco errado (XEX corrompido)");
        const uint32_t next_size = Be32(b);
        std::memcpy(block_hash, b + 4, 20);
        size_t q = 24;
        while (q + 2 <= block_size) {
          const size_t chunk = Be16(b + q);
          q += 2;
          if (!chunk) break;
          if (q + chunk > block_size) return fail("pedaço LZX cortado");
          lzx.insert(lzx.end(), b + q, b + q + chunk);
          q += chunk;
        }
        p += block_size;
        block_size = next_size;
      }
      r.pe.assign(image_size, 0);
      if (!Lzx(lzx.data(), lzx.size(), r.pe.data(), r.pe.size(), window)) return fail("LZX falhou");
      break;
    }
    default:
      return fail("compressão desconhecida");
  }
  if (r.pe.size() < 2 || r.pe[0] != 'M' || r.pe[1] != 'Z') return fail("imagem sem cabeçalho MZ (chave errada?)");
  return r;
}

}  // namespace zandatsu
