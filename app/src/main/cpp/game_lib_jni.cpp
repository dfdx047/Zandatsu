// libmain.so: a imagem PE do default.xex do jogador para o GameLib.kt (a chave do libgame.so é o SHA-256 dela)
// e a tabela XDBF de conquistas do próprio default.xex (issue #91).
// Sem dependências do jogo nem do runtime: o launcher carrega-a só para isto.

#include <jni.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "xex_image.h"

namespace {

uint16_t Be16(const uint8_t* p) { return uint16_t(uint16_t(p[0]) << 8 | p[1]); }
uint32_t Be32(const uint8_t* p) {
  return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
uint64_t Be64(const uint8_t* p) {
  return (uint64_t(Be32(p)) << 32) | uint64_t(Be32(p + 4));
}

bool ReadFileAll(const char* path, std::vector<uint8_t>& out) {
  FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  uint8_t buf[1 << 16];
  for (size_t n; (n = std::fread(buf, 1, sizeof(buf), f)) > 0;) out.insert(out.end(), buf, buf + n);
  std::fclose(f);
  return !out.empty();
}

struct Slice {
  const uint8_t* data = nullptr;
  size_t size = 0;
};

// Localiza o recurso XDBF dentro da imagem PE usando o cabeçalho opcional 0x000002FF (kResourceInfo) do XEX2,
// com fallback por procura de assinatura válida "XDBF" caso o formato varie.
Slice FindXdbf(const std::vector<uint8_t>& xex, const std::vector<uint8_t>& pe) {
  if (xex.size() >= 0x18 && std::memcmp(xex.data(), "XEX2", 4) == 0) {
    const uint32_t count = Be32(xex.data() + 0x14);
    if (0x18 + uint64_t(count) * 8 <= xex.size()) {
      uint32_t base_addr = 0x82000000u;
      uint32_t res_off = 0;
      for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* h = xex.data() + 0x18 + i * 8;
        const uint32_t key = Be32(h);
        const uint32_t val = Be32(h + 4);
        if (key == 0x00010201u) base_addr = val;
        if (key == 0x000002FFu) res_off = val;
      }
      if (res_off && res_off + 4 <= xex.size()) {
        const uint32_t table_size = Be32(xex.data() + res_off);
        if (table_size >= 20 && res_off + table_size <= xex.size()) {
          const uint32_t entries = (table_size - 4) / 16;
          for (uint32_t i = 0; i < entries; ++i) {
            const uint8_t* e = xex.data() + res_off + 4 + i * 16;
            const uint32_t addr = Be32(e + 8);
            const uint32_t sz = Be32(e + 12);
            if (addr >= base_addr) {
              const size_t off = size_t(addr - base_addr);
              if (sz >= 24 && off + sz <= pe.size() && std::memcmp(pe.data() + off, "XDBF", 4) == 0) {
                return {pe.data() + off, sz};
              }
            }
          }
        }
      }
    }
  }
  for (size_t off = 0; off + 24 <= pe.size(); off += 4) {
    if (std::memcmp(pe.data() + off, "XDBF", 4) == 0) {
      const uint32_t ver = Be32(pe.data() + off + 4);
      const uint32_t ec = Be32(pe.data() + off + 8);
      const uint32_t eu = Be32(pe.data() + off + 12);
      if (ver == 1 && ec > 0 && ec < 4096 && eu <= ec) {
        return {pe.data() + off, pe.size() - off};
      }
    }
  }
  return {};
}

Slice FindXdbfEntry(const Slice& xdbf, uint16_t want_section, uint64_t want_id) {
  if (!xdbf.data || xdbf.size < 24 || std::memcmp(xdbf.data, "XDBF", 4) != 0) return {};
  const uint32_t entry_count = Be32(xdbf.data + 8);
  const uint32_t entry_used = Be32(xdbf.data + 12);
  const uint32_t free_count = Be32(xdbf.data + 16);
  const uint64_t content_off = 24ull + uint64_t(entry_count) * 18ull + uint64_t(free_count) * 8ull;
  if (entry_used > entry_count || content_off > xdbf.size) return {};
  const uint8_t* entries = xdbf.data + 24;
  for (uint32_t i = 0; i < entry_used; ++i) {
    const uint8_t* e = entries + size_t(i) * 18;
    const uint16_t sec = Be16(e);
    const uint64_t id = Be64(e + 2);
    const uint32_t off = Be32(e + 10);
    const uint32_t sz = Be32(e + 14);
    if (sec == want_section && id == want_id) {
      if (content_off + uint64_t(off) + uint64_t(sz) <= xdbf.size) {
        return {xdbf.data + content_off + off, sz};
      }
      return {};
    }
  }
  return {};
}

std::unordered_map<uint16_t, std::string> ParseStringTable(const Slice& xdbf, uint32_t lang) {
  std::unordered_map<uint16_t, std::string> out;
  const Slice tbl = FindXdbfEntry(xdbf, 0x0003, lang);
  if (!tbl.data || tbl.size < 14 || std::memcmp(tbl.data, "XSTR", 4) != 0) return out;
  const uint16_t count = Be16(tbl.data + 12);
  size_t pos = 14;
  for (uint16_t i = 0; i < count && pos + 4 <= tbl.size; ++i) {
    const uint16_t id = Be16(tbl.data + pos);
    const uint16_t len = Be16(tbl.data + pos + 2);
    pos += 4;
    if (pos + len > tbl.size) break;
    out.emplace(id, std::string(reinterpret_cast<const char*>(tbl.data + pos), len));
    pos += len;
  }
  return out;
}

void AppendJsonString(std::string& dst, const std::string& src) {
  dst.push_back('"');
  for (unsigned char c : src) {
    switch (c) {
      case '"': dst += "\\\""; break;
      case '\\': dst += "\\\\"; break;
      case '\b': dst += "\\b"; break;
      case '\f': dst += "\\f"; break;
      case '\n': dst += "\\n"; break;
      case '\r': dst += "\\r"; break;
      case '\t': dst += "\\t"; break;
      default:
        if (c < 0x20) {
          char hex[8];
          std::snprintf(hex, sizeof(hex), "\\u%04x", unsigned(c));
          dst += hex;
        } else {
          dst.push_back(char(c));
        }
        break;
    }
  }
  dst.push_back('"');
}

}  // namespace

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_dfdx047_zandatsu_data_GameLib_nativeXexImage(JNIEnv* env, jclass, jstring path, jintArray title_out,
                                                      jobjectArray error_out) {
  auto error = [&](const char* e) {
    env->SetObjectArrayElement(error_out, 0, env->NewStringUTF(e));
    return nullptr;
  };
  const char* p = env->GetStringUTFChars(path, nullptr);
  std::vector<uint8_t> xex;
  const bool ok = ReadFileAll(p, xex);
  env->ReleaseStringUTFChars(path, p);
  if (!ok) return error("não abre o default.xex");

  const auto img = zandatsu::ReadXexImage(xex.data(), xex.size());
  const jint title = jint(img.title_id);
  env->SetIntArrayRegion(title_out, 0, 1, &title);
  if (!img.error.empty()) return error(img.error.c_str());
  jbyteArray out = env->NewByteArray(jsize(img.pe.size()));
  if (!out) return error("sem memória para a imagem");
  env->SetByteArrayRegion(out, 0, jsize(img.pe.size()), reinterpret_cast<const jbyte*>(img.pe.data()));
  return out;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dfdx047_zandatsu_data_GameLib_nativeReadAchievements(JNIEnv* env, jclass, jstring path, jint lang_id) {
  const char* p = env->GetStringUTFChars(path, nullptr);
  std::vector<uint8_t> xex;
  const bool ok = ReadFileAll(p, xex);
  env->ReleaseStringUTFChars(path, p);
  if (!ok) return nullptr;

  const auto img = zandatsu::ReadXexImage(xex.data(), xex.size());
  if (!img.error.empty() || img.pe.empty()) return nullptr;

  const Slice xdbf = FindXdbf(xex, img.pe);
  if (!xdbf.data) return nullptr;

  const Slice xach = FindXdbfEntry(xdbf, 0x0001, 0x58414348ull);
  if (!xach.data || xach.size < 14 || std::memcmp(xach.data, "XACH", 4) != 0) return nullptr;

  uint32_t default_lang = 1;
  const Slice xstc = FindXdbfEntry(xdbf, 0x0001, 0x58535443ull);
  if (xstc.data && xstc.size >= 16 && std::memcmp(xstc.data, "XSTC", 4) == 0) {
    default_lang = Be32(xstc.data + 12);
  }

  auto strings = ParseStringTable(xdbf, uint32_t(lang_id > 0 ? lang_id : 1));
  const auto fallback = ParseStringTable(xdbf, default_lang);
  const auto english = (default_lang == 1) ? fallback : ParseStringTable(xdbf, 1);

  auto str = [&](uint16_t id) -> const std::string& {
    static const std::string kEmpty;
    if (auto it = strings.find(id); it != strings.end() && !it->second.empty()) return it->second;
    if (auto it = fallback.find(id); it != fallback.end() && !it->second.empty()) return it->second;
    if (auto it = english.find(id); it != english.end() && !it->second.empty()) return it->second;
    return kEmpty;
  };

  const uint16_t count = Be16(xach.data + 12);
  std::string json;
  json.reserve(size_t(count) * 160 + 16);
  json.push_back('[');
  bool first = true;
  size_t pos = 14;
  for (uint16_t i = 0; i < count && pos + 0x24 <= xach.size; ++i, pos += 0x24) {
    const uint8_t* e = xach.data + pos;
    const uint16_t id = Be16(e + 0x00);
    const uint16_t label_id = Be16(e + 0x02);
    const uint16_t desc_id = Be16(e + 0x04);
    const uint16_t unach_id = Be16(e + 0x06);
    const uint16_t score = Be16(e + 0x0C);
    const uint32_t flags = Be32(e + 0x10);
    if (!first) json.push_back(',');
    first = false;
    char head[64];
    std::snprintf(head, sizeof(head), "{\"id\":%u,\"score\":%u,\"flags\":%u,\"label\":", unsigned(id), unsigned(score),
                  unsigned(flags));
    json += head;
    AppendJsonString(json, str(label_id));
    json += ",\"desc\":";
    AppendJsonString(json, str(desc_id));
    json += ",\"unachieved\":";
    AppendJsonString(json, str(unach_id));
    json.push_back('}');
  }
  json.push_back(']');
  return env->NewStringUTF(json.c_str());
}
