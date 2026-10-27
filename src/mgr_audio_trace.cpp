// O que o MGR pede ao Wwise, para o log (audio_trace, desligada por defeito; issues #3, #5: sons dos inimigos que
// nunca tocam). Enderecos achados no codigo do jogo (Wwise 2012, bancos versao 72):
//   0x825554A8  AK::SoundEngine::GetIDFromString(const char*)    nome -> ID (FNV-1 32 bits do nome em minusculas)
//   0x82555368  AK::SoundEngine::GetIDFromString(const wchar_t*)
//   0x8256EC68  CAkBankMgr::QueueBankCommand(item)               pedido de banco para a thread de bancos
//   0x82570A80  CAkBankMgr::LoadBank(item, ...)                  carregamento na thread de bancos (chama o
//                                                                ProcessBankHeader 0x8256D7D0); devolve AKRESULT
//   0x825578E0  PostEvent interno (evento, objeto, ...)          procura o evento no indice dos bancos carregados;
//                                                                devolve o playing ID, 0 se o evento nao existe
// Estado de cada objeto de som (o que torna um som decodificado audivel ou mudo na mistura):
//   0x825542C8, 0x82554338  registo do objeto (mensagem 11)      r3 = objeto
//   0x825543A8  remocao do objeto (mensagem 12)                  r3 = objeto
//   0x825555A8  SetPosition (mensagem 13)                        r3 = objeto, r4 = 6 floats (posicao, orientacao)
//   0x82553B70  SetListenerPosition (mensagem 16)                r3 = 9 floats (frente, cima, posicao), r4 = ouvinte
//   0x82553B28  ouvintes ativos (mensagem 14)                    r3 = objeto, r4 = mascara
//   0x82553A58  SetAttenuationScalingFactor (mensagem 33)        r3 = objeto, f1 = escala (> 0)
//   0x82553EE8  volume de saida do objeto (mensagem 19)          r3 = objeto, f1 = volume
//   0x82553E78  aux sends (mensagem 18)                          r3 = objeto, r4 = valores, r5 = quantos (<= 4)
// O item dos bancos vai por valor em r4..r8 (palavras de 32 bits: r4 alto = tipo, r6 alto = ID do banco).

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

#include <rex/audio/audio_stats.h>
#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc.h>

REXCVAR_DECLARE(bool, audio_trace);  // SDK, src/audio/sdl/sdl_audio_driver.cpp (patch 0182)

namespace {

using Clock = std::chrono::steady_clock;

struct PendingBank {
  uint32_t type;
  Clock::time_point since;
  bool warned = false;
};

std::mutex g_mutex;
std::unordered_map<uint32_t, std::string> g_names;           // ID -> nome
std::unordered_map<uint32_t, PendingBank> g_pending_banks;   // ID do banco -> pedido sem conclusao
std::set<uint32_t> g_load_types;                             // tipos de pedido que chegam ao LoadBank
std::unordered_map<uint32_t, Clock::time_point> g_load_start;

struct ObjState {
  float pos[3] = {0, 0, 0};
  bool has_pos = false, registered = false;
  float scaling = -1, volume = -1;
  int64_t listeners = -1, aux = -1;
  uint32_t events = 0;  // eventos no ultimo segundo
  std::unordered_map<std::string, uint32_t> names;
};
std::unordered_map<uint32_t, ObjState> g_objs;
float g_listener[8][3] = {};
bool g_listener_set[8] = {};
Clock::time_point g_last_dump = Clock::now();

bool On() { return REXCVAR_GET(audio_trace); }

float GuestFloat(uint8_t* base, uint32_t addr) {
  const uint32_t v = uint32_t(base[addr]) << 24 | uint32_t(base[addr + 1]) << 16 | uint32_t(base[addr + 2]) << 8 |
                     base[addr + 3];
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}

// Uma vez por segundo: o estado dos objetos que receberam eventos (os 10 com mais eventos).
void DumpObjects() {
  std::lock_guard<std::mutex> lock(g_mutex);
  const auto now = Clock::now();
  if (now - g_last_dump < std::chrono::seconds(1)) return;
  g_last_dump = now;
  std::vector<std::pair<uint32_t, ObjState*>> act;
  for (auto& [id, o] : g_objs) {
    if (o.events) act.push_back({id, &o});
  }
  std::sort(act.begin(), act.end(), [](auto& a, auto& b) { return a.second->events > b.second->events; });
  if (act.size() > 10) act.resize(10);
  for (auto& [id, o] : act) {
    std::string evs;
    for (auto& [n, k] : o->names) evs += fmt::format("{}{} x{}", evs.empty() ? "" : ", ", n, k);
    std::string dist = "sem posicao do ouvinte 0";
    if (o->has_pos && g_listener_set[0]) {
      const float dx = o->pos[0] - g_listener[0][0], dy = o->pos[1] - g_listener[0][1],
                  dz = o->pos[2] - g_listener[0][2];
      dist = fmt::format("{:.1f} do ouvinte 0", std::sqrt(dx * dx + dy * dy + dz * dz));
    }
    REXLOG_INFO("[audio-trace] objeto {:#x}: {} eventos ({}) | posicao {} | {} | escala atenuacao {} | volume saida {} | "
                "ouvintes {} | aux {} | {}",
                id, o->events, evs,
                o->has_pos ? fmt::format("({:.1f}, {:.1f}, {:.1f})", o->pos[0], o->pos[1], o->pos[2]) : "nunca",
                dist, o->scaling < 0 ? "nunca" : fmt::format("{:.3f}", o->scaling),
                o->volume < 0 ? "nunca" : fmt::format("{:.3f}", o->volume),
                o->listeners < 0 ? "nunca" : fmt::format("{:#x}", o->listeners),
                o->aux < 0 ? "nunca" : fmt::format("{}", o->aux), o->registered ? "registado" : "NAO REGISTADO");
  }
  for (auto& [id, o] : g_objs) {
    o.events = 0;
    o.names.clear();
  }
}

std::string Name(uint32_t id) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_names.find(id);
  return it == g_names.end() ? fmt::format("{:#010x}", id) : fmt::format("{} ({:#010x})", it->second, id);
}

void RememberName(uint32_t id, std::string name) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto [it, added] = g_names.emplace(id, name);
  if (added) {
    REXLOG_INFO("[audio-trace] nome '{}' = {:#010x}", name, id);
  }
}

const char* AkResult(uint32_t r) {
  switch (r) {
    case 1: return "ok";
    case 2: return "AK_Fail";
    case 7: return "AK_InvalidFile";
    case 15: return "AK_IDNotFound";
    case 31: return "AK_InvalidParameter";
    case 52: return "AK_InsufficientMemory";
    case 53: return "AK_Cancelled";
    case 54: return "AK_UnknownBankID";
    case 56: return "AK_BankReadError";
    case 64: return "AK_WrongBankVersion";
    case 66: return "AK_FileNotFound";
    case 69: return "AK_BankAlreadyLoaded";
    default: return "AKRESULT";
  }
}

// Pedidos de carregamento sem conclusao ha mais de 2 s (so os tipos que ja se viu chegar ao LoadBank).
void CheckPending() {
  std::lock_guard<std::mutex> lock(g_mutex);
  const auto now = Clock::now();
  for (auto& [id, p] : g_pending_banks) {
    if (!p.warned && g_load_types.count(p.type) && now - p.since > std::chrono::seconds(2)) {
      p.warned = true;
      auto it = g_names.find(id);
      REXLOG_WARN("[audio-trace] banco {} (tipo {}) pedido ha {} ms e AINDA NAO CARREGOU",
                  it == g_names.end() ? fmt::format("{:#010x}", id) : it->second, p.type,
                  std::chrono::duration_cast<std::chrono::milliseconds>(now - p.since).count());
    }
  }
}

// So enderecos das areas de memoria do jogo (heaps 0x40000000+ e a imagem 0x82000000+).
bool GuestAddr(uint32_t addr) { return addr >= 0x40000000u && addr < 0xC0000000u; }

std::string GuestString(uint8_t* base, uint32_t addr) {
  std::string s;
  if (!GuestAddr(addr)) return s;
  for (uint32_t i = 0; i < 260; ++i) {
    const char c = char(base[addr + i]);
    if (!c) break;
    s += c;
  }
  return s;
}

std::string GuestWideString(uint8_t* base, uint32_t addr) {
  std::string s;
  if (!GuestAddr(addr)) return s;
  for (uint32_t i = 0; i < 260; ++i) {
    const uint16_t c = uint16_t(base[addr + 2 * i] << 8 | base[addr + 2 * i + 1]);
    if (!c) break;
    s += c < 0x80 ? char(c) : '?';
  }
  return s;
}

uint32_t Hi(uint64_t v) { return uint32_t(v >> 32); }
uint32_t Lo(uint64_t v) { return uint32_t(v); }

}  // namespace

REX_EXTERN(__imp__sub_825554A8);
REX_HOOK_RAW(sub_825554A8) {
  const uint32_t str = ctx.r3.u32;
  __imp__sub_825554A8(ctx, base);
  if (On() && str) {
    RememberName(ctx.r3.u32, GuestString(base, str));
  }
}

REX_EXTERN(__imp__sub_82555368);
REX_HOOK_RAW(sub_82555368) {
  const uint32_t str = ctx.r3.u32;
  __imp__sub_82555368(ctx, base);
  if (On() && str) {
    RememberName(ctx.r3.u32, GuestWideString(base, str));
  }
}

REX_EXTERN(__imp__sub_8256EC68);
REX_HOOK_RAW(sub_8256EC68) {
  const uint64_t w0 = ctx.r4.u64, w1 = ctx.r5.u64, w2 = ctx.r6.u64, w3 = ctx.r7.u64, w4 = ctx.r8.u64;
  __imp__sub_8256EC68(ctx, base);
  if (!On()) return;
  const uint32_t type = Hi(w0), bank = Hi(w2), result = ctx.r3.u32;
  rex::audio::stats::Get().trace_bank_cmds++;
  if (result == 1) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_pending_banks.emplace(bank, PendingBank{type, Clock::now()}).second) {
      rex::audio::stats::Get().trace_bank_pending++;
    }
  } else {
    rex::audio::stats::Get().trace_bank_fail++;
  }
  REXLOG_INFO("[audio-trace] pedido de banco: tipo {} banco {} -> {} ({}) | item {:08x} {:08x} {:08x} {:08x} {:08x} "
              "{:08x} {:08x} {:08x}",
              type, Name(bank), AkResult(result), result, Lo(w0), Hi(w1), Lo(w1), Lo(w2), Hi(w3), Lo(w3), Hi(w4),
              Lo(w4));
}

REX_EXTERN(__imp__sub_82570A80);
REX_HOOK_RAW(sub_82570A80) {
  const uint32_t type = Hi(ctx.r4.u64), bank = Hi(ctx.r6.u64), mode = ctx.r10.u32;
  const auto start = Clock::now();
  if (On()) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_load_types.insert(type);
  }
  __imp__sub_82570A80(ctx, base);
  if (!On()) return;
  const uint32_t result = ctx.r3.u32;
  auto& c = rex::audio::stats::Get();
  bool was_pending = false;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    was_pending = g_pending_banks.erase(bank) > 0;
  }
  if (was_pending) c.trace_bank_pending--;
  const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
  if (result == 1 || result == 69) {
    c.trace_bank_ok++;
    REXLOG_INFO("[audio-trace] banco {} carregado: {} em {:.1f} ms (tipo {}, modo {})", Name(bank), AkResult(result),
                ms, type, mode);
  } else {
    c.trace_bank_fail++;
    REXLOG_WARN("[audio-trace] banco {} FALHOU: {} ({}) em {:.1f} ms (tipo {}, modo {})", Name(bank),
                AkResult(result), result, ms, type, mode);
  }
}

REX_EXTERN(__imp__sub_825578E0);
REX_HOOK_RAW(sub_825578E0) {
  const uint32_t event = ctx.r3.u32, object = ctx.r4.u32;
  __imp__sub_825578E0(ctx, base);
  if (!On()) return;
  const uint32_t playing = ctx.r3.u32;
  auto& c = rex::audio::stats::Get();
  c.trace_events++;
  {
    std::string n = Name(event);
    std::lock_guard<std::mutex> lock(g_mutex);
    auto& o = g_objs[object];
    o.events++;
    o.names[n.substr(0, n.find(" ("))]++;
  }
  if (playing) {
    REXLOG_INFO("[audio-trace] evento {} objeto {:#x} -> playing {}", Name(event), object, playing);
  } else {
    c.trace_events_failed++;
    REXLOG_WARN("[audio-trace] evento {} objeto {:#x} -> SEM EVENTO CARREGADO (playing 0)", Name(event), object);
  }
  CheckPending();
  DumpObjects();
}


// Registo/remocao dos objetos de som.
REX_EXTERN(__imp__sub_825542C8);
REX_HOOK_RAW(sub_825542C8) {
  const uint32_t object = ctx.r3.u32;
  __imp__sub_825542C8(ctx, base);
  if (!On()) return;
  rex::audio::stats::Get().trace_gameobj_reg++;
  std::lock_guard<std::mutex> lock(g_mutex);
  g_objs[object].registered = true;
}

REX_EXTERN(__imp__sub_82554338);
REX_HOOK_RAW(sub_82554338) {
  const uint32_t object = ctx.r3.u32;
  __imp__sub_82554338(ctx, base);
  if (!On()) return;
  rex::audio::stats::Get().trace_gameobj_reg++;
  std::lock_guard<std::mutex> lock(g_mutex);
  g_objs[object].registered = true;
}

REX_EXTERN(__imp__sub_825543A8);
REX_HOOK_RAW(sub_825543A8) {
  const uint32_t object = ctx.r3.u32;
  __imp__sub_825543A8(ctx, base);
  if (!On()) return;
  rex::audio::stats::Get().trace_gameobj_unreg++;
  std::lock_guard<std::mutex> lock(g_mutex);
  g_objs.erase(object);
}

REX_EXTERN(__imp__sub_825555A8);
REX_HOOK_RAW(sub_825555A8) {
  const uint32_t object = ctx.r3.u32, ptr = ctx.r4.u32;
  if (On() && GuestAddr(ptr)) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto& o = g_objs[object];
    for (int i = 0; i < 3; ++i) o.pos[i] = GuestFloat(base, ptr + 4 * i);
    o.has_pos = true;
  }
  __imp__sub_825555A8(ctx, base);
}

REX_EXTERN(__imp__sub_82553B70);
REX_HOOK_RAW(sub_82553B70) {
  const uint32_t ptr = ctx.r3.u32, index = ctx.r4.u32;
  if (On() && GuestAddr(ptr) && index < 8) {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (int i = 0; i < 3; ++i) g_listener[index][i] = GuestFloat(base, ptr + 24 + 4 * i);
    g_listener_set[index] = true;
  }
  __imp__sub_82553B70(ctx, base);
}

REX_EXTERN(__imp__sub_82553B28);
REX_HOOK_RAW(sub_82553B28) {
  if (On()) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_objs[ctx.r3.u32].listeners = ctx.r4.u32;
  }
  __imp__sub_82553B28(ctx, base);
}

REX_EXTERN(__imp__sub_82553A58);
REX_HOOK_RAW(sub_82553A58) {
  if (On()) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_objs[ctx.r3.u32].scaling = float(ctx.f1.f64);
  }
  __imp__sub_82553A58(ctx, base);
}

REX_EXTERN(__imp__sub_82553EE8);
REX_HOOK_RAW(sub_82553EE8) {
  if (On()) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_objs[ctx.r3.u32].volume = float(ctx.f1.f64);
  }
  __imp__sub_82553EE8(ctx, base);
}

REX_EXTERN(__imp__sub_82553E78);
REX_HOOK_RAW(sub_82553E78) {
  if (On()) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_objs[ctx.r3.u32].aux = ctx.r5.u32;
  }
  __imp__sub_82553E78(ctx, base);
}

// Vozes XMA do Wwise (CAkSrcXMA, biblioteca XMAPlayback do XDK em 0x825FAxxx):
//   0x825B0C60  CAkSrcXMA::GetBuffer(src, io)   desliga o contexto, sincroniza, ve as amostras prontas (0x825B0848),
//               consome-as e volta a ligar o contexto; resultado em io+816: 45 pronto, 46 sem dados, 17 acabou, 2 erro.
//               src+20 = posicao (amostras), src+16 = fim, src+36 = fim do loop, src+48 = loops, src+52 = XMAPlayback
//   0x825B07B8, 0x825B0958, 0x825B18A0  fim da voz: XMAPlaybackDestroy (0x825FAF10) -> XMAReleaseContext
// Uma voz que nunca chega ao fim segura o seu contexto XMA e o Wwise continua a dar-lhe kicks.
namespace {

uint32_t Be32(uint8_t* base, uint32_t addr) {
  return uint32_t(base[addr]) << 24 | uint32_t(base[addr + 1]) << 16 | uint32_t(base[addr + 2]) << 8 | base[addr + 3];
}
uint16_t Be16(uint8_t* base, uint32_t addr) { return uint16_t(base[addr] << 8 | base[addr + 1]); }

struct XmaVoice {
  Clock::time_point start, progress;
  uint32_t pos = 0, calls = 0, ready = 0, starved = 0, ctx_id = 0;
  bool warned = false;
};
std::unordered_map<uint32_t, XmaVoice> g_xma_voices;  // src -> estado

std::string XmaState(uint8_t* base, uint32_t src) {
  const uint32_t xp = Be32(base, src + 52);
  if (!xp) return "sem XMAPlayback";
  const uint32_t entry = Be32(base, xp + 8), flags = Be32(base, xp + 4);
  const uint32_t hw = Be32(base, entry + 64);
  // Sombra (flag 0x20000) ou o contexto do hardware: palavras 0, 1, 2 e 9 do XMA_CONTEXT_DATA.
  const uint32_t d = (flags & 0x20000) ? entry : hw;
  if (!d) return "sem contexto";
  const uint32_t w0 = Be32(base, d), w1 = Be32(base, d + 4), w2 = Be32(base, d + 8), w9 = Be32(base, d + 36);
  const uint32_t h0 = hw ? Be32(base, hw) : 0, h1 = hw ? Be32(base, hw + 4) : 0, h9 = hw ? Be32(base, hw + 36) : 0;
  return fmt::format(
      "contexto c{} ({}): entrada valida {}/{}, leitura {} bits, saida escrita {} lida {} de {} blocos, saida valida {}, "
      "erro {} | hardware: entrada {}/{}, escrita {} lida {}, valida {} | resto {} amostras {}",
      Be16(base, entry + 80), (flags & 0x20000) ? "sombra" : "direto", (w0 >> 20) & 1, (w0 >> 21) & 1,
      w2 & 0x3FFFFFF, w0 >> 27, w9 & 31, (w0 >> 22) & 31, w1 >> 31, (w2 >> 26) & 31, (h0 >> 20) & 1, (h0 >> 21) & 1,
      h0 >> 27, h9 & 31, h1 >> 31, Be16(base, entry + 82), Be32(base, entry + 76));
}

void XmaVoiceEnd(uint8_t* base, uint32_t src, const char* where) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_xma_voices.find(src);
  if (it == g_xma_voices.end()) return;
  const auto& v = it->second;
  const double ms = std::chrono::duration<double, std::milli>(Clock::now() - v.start).count();
  REXLOG_INFO("[audio-trace] voz XMA {:#x} c{} terminou ({}): posicao {} de {}, {} chamadas ({} prontas, {} sem dados) "
              "em {:.0f} ms",
              src, v.ctx_id, where, Be32(base, src + 20), Be32(base, src + 16), v.calls, v.ready, v.starved, ms);
  g_xma_voices.erase(it);
}

}  // namespace

REX_EXTERN(__imp__sub_825B0C60);
REX_HOOK_RAW(sub_825B0C60) {
  const uint32_t src = ctx.r3.u32, io = ctx.r4.u32;
  __imp__sub_825B0C60(ctx, base);
  if (!On()) return;
  const uint32_t result = Be32(base, io + 816), pos = Be32(base, src + 20);
  const auto now = Clock::now();
  std::lock_guard<std::mutex> lock(g_mutex);
  auto [it, fresh] = g_xma_voices.try_emplace(src);
  auto& v = it->second;
  if (fresh) {
    v.start = v.progress = now;
    const uint32_t xp = Be32(base, src + 52);
    v.ctx_id = xp ? Be16(base, Be32(base, xp + 8) + 80) : 0;
    REXLOG_INFO("[audio-trace] voz XMA {:#x} c{} comeca: fim {} amostras (loop ate {}, {} loops)", src, v.ctx_id,
                Be32(base, src + 16), Be32(base, src + 36), Be16(base, src + 48));
  }
  v.calls++;
  if (result == 45) v.ready++;
  if (result == 46) v.starved++;
  if (pos != v.pos) {
    v.pos = pos;
    v.progress = now;
  }
  if (result == 17 || result == 2) {
    REXLOG_INFO("[audio-trace] voz XMA {:#x} c{}: GetBuffer -> {} ({}) na posicao {} de {}", src, v.ctx_id,
                result == 17 ? "acabou" : "ERRO", result, pos, Be32(base, src + 16));
  } else if (!v.warned && now - v.progress > std::chrono::seconds(1)) {
    v.warned = true;
    REXLOG_WARN("[audio-trace] voz XMA {:#x} PRESA ha {:.0f} ms: posicao {} de {}, GetBuffer -> {} | {}", src,
                std::chrono::duration<double, std::milli>(now - v.progress).count(), pos, Be32(base, src + 16), result,
                XmaState(base, src));
  }
}

REX_EXTERN(__imp__sub_825B07B8);
REX_HOOK_RAW(sub_825B07B8) {
  if (On()) XmaVoiceEnd(base, ctx.r3.u32, "07B8");
  __imp__sub_825B07B8(ctx, base);
}

REX_EXTERN(__imp__sub_825B0958);
REX_HOOK_RAW(sub_825B0958) {
  if (On()) XmaVoiceEnd(base, ctx.r3.u32, "0958");
  __imp__sub_825B0958(ctx, base);
}

REX_EXTERN(__imp__sub_825B18A0);
REX_HOOK_RAW(sub_825B18A0) {
  if (On()) XmaVoiceEnd(base, ctx.r3.u32, "18A0");
  __imp__sub_825B18A0(ctx, base);
}

// Todas as mensagens do jogo para a thread do Wwise (CAkAudioMgr::Enqueue 0x82589268: r4 = mensagem, tipo em +2),
// menos as que vao a cada frame (1 eventos, ja no log; 2 RTPC global; 13 posicoes; 16 ouvintes; 18 aux; 19 volume;
// 33 escala da atenuacao): stop/pause de eventos, switches, estados.
REX_EXTERN(__imp__sub_82589268);
REX_HOOK_RAW(sub_82589268) {
  if (On()) {
    const uint32_t msg = ctx.r4.u32;
    const uint16_t type = Be16(base, msg + 2);
    if (type != 1 && type != 2 && type != 13 && type != 16 && type != 18 && type != 19 && type != 33) {
      REXLOG_INFO("[audio-trace] mensagem {} ({} B): {:08x} {:08x} {:08x} {:08x} {:08x}", type, ctx.r5.u32,
                  Be32(base, msg + 4), Be32(base, msg + 8), Be32(base, msg + 12), Be32(base, msg + 16),
                  Be32(base, msg + 20));
    }
  }
  __imp__sub_82589268(ctx, base);
}
