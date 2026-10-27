// Zandatsu - estado do jogo e comandos ao vivo para o app (NativeGameState.kt).
//
// A mesma interface JNI do Dragon Burst, para o launcher, o menu do jogo e os controles
// por toque. O que dependia de enderecos do Burst Limit (lado do P1, luta/menus) ainda
// nao tem equivalente no MGR e devolve "desconhecido": os controles ficam no layout
// escolhido a mao (botao de troca no menu) ate se mapear onde o MGR guarda o estado
// "jogo/menu" (ver mgr_state.h quando existir).
//
// Nao escreve nada na memoria do jogo.

#include <jni.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>

#include <rex/audio/audio_system.h>
#include <rex/audio/downmix.h>
#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/perf/frame_rate.h>
#include <rex/runtime.h>
#include <rex/ui/overlay/quick_menu.h>

// android_hooks.cpp: pausa do jogo no VdSwap.
extern std::atomic<bool> g_swordman_pause;
extern std::mutex g_swordman_pause_mu;
extern std::condition_variable g_swordman_pause_cv;
extern std::atomic<uint64_t> g_swordman_ticks;

namespace {

// O runtime le e grava as cvars neste ficheiro (ReXApp: <pasta do exe>/<nome>.toml; no
// Android a pasta do exe e a pasta do app, filesystem_posix.cpp).
constexpr const char* kConfigName = "mgr.toml";

std::mutex g_pause_mu;
bool g_paused = false;

}  // namespace

// Lado do P1 (Dragon Burst). Sem sentido no MGR: sempre desconhecido.
extern "C" JNIEXPORT jint JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeP1Side(JNIEnv*, jclass) {
  return 0;
}

// ---------------------------------------------------------------------------
// Pausa do jogo enquanto um menu do app esta aberto (menu do jogo, editor dos
// controles). O jogo para no VdSwap (android_hooks.cpp) e o audio com
// AudioSystem::Pause(). O Kotlin chama isto a cada tick do HelperEngine (60 Hz).
// ---------------------------------------------------------------------------
extern "C" JNIEXPORT jboolean JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeSyncPause(JNIEnv*, jclass, jboolean overlay_open,
                                                               jboolean pause_quick_menu) {
  std::lock_guard<std::mutex> lock(g_pause_mu);
  auto* rt = rex::Runtime::instance();
  if (!rt || !rt->memory()) return JNI_FALSE;
  const bool want = overlay_open || (pause_quick_menu && rex::ui::QuickMenuDialog::IsOpen());
  if (want == g_paused) return g_paused ? JNI_TRUE : JNI_FALSE;

  auto* audio = static_cast<rex::audio::AudioSystem*>(rt->audio_system());
  if (want) {
    g_swordman_pause.store(true, std::memory_order_release);
    if (audio) audio->Pause();
    REXLOG_INFO("swordman: pausa (menu do app)");
  } else {
    if (audio) audio->Resume();
    {
      std::lock_guard<std::mutex> l(g_swordman_pause_mu);
      g_swordman_pause.store(false, std::memory_order_release);
    }
    g_swordman_pause_cv.notify_all();
    REXLOG_INFO("swordman: retoma");
  }
  g_paused = want;
  return g_paused ? JNI_TRUE : JNI_FALSE;
}

/** Ao sair do jogo: nunca deixar a thread do jogo presa na pausa. */
extern "C" JNIEXPORT void JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeForceResume(JNIEnv*, jclass) {
  {
    std::lock_guard<std::mutex> l(g_swordman_pause_mu);
    g_swordman_pause.store(false, std::memory_order_release);
  }
  g_swordman_pause_cv.notify_all();
}

// ---------------------------------------------------------------------------
// Jogo ou menus (troca do layout dos controles). 1 = jogo, 0 = menus, -1 = desconhecido.
// No MGR ainda nao se sabe: -1 sempre que o jogo corre (o layout fica o escolhido).
// ---------------------------------------------------------------------------
// 27/10: achado comparando a secao de dados do XEX em titulo, slots, menu principal, luta e pausa (AutoPilot
// "memdump", celular): 0x83931FE0 = 0x01000000 nos menus e na pausa, 0x01000111 com o Raiden a controlar (os 3 bits
// baixos: ligados so na luta). 0x836D2860 (0 menus, 1 luta, 3 pausa) vai no diagnostico.
namespace {
constexpr uint32_t kPlayStateFlags = 0x83931FE0u;
constexpr uint32_t kPlayStateMode = 0x836D2860u;

uint32_t LoadGuestU32(uint32_t address) {
  auto* rt = rex::Runtime::instance();
  if (!rt || !rt->memory()) return 0xFFFFFFFFu;
  const auto* p = rt->memory()->TranslateVirtual<const uint8_t*>(address);
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}
}  // namespace

extern "C" JNIEXPORT jint JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeFightFlag(JNIEnv*, jclass) {
  if (!rex::Runtime::instance() || g_swordman_ticks.load(std::memory_order_relaxed) == 0) return -1;
  // As duas juntas: nos ecras de slots os bits piscavam por um frame (troca falsa de 1,4 s no celular).
  return (LoadGuestU32(kPlayStateFlags) & 0x111u) == 0x111u && LoadGuestU32(kPlayStateMode) == 1 ? 1 : 0;
}

// Camara livre / modo foto (Dragon Burst). Nao existe no MGR.
extern "C" JNIEXPORT jint JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeMapFlag(JNIEnv*, jclass) {
  return rex::Runtime::instance() ? 0 : -1;
}

// Diagnostico para logs/controles.log: frames apresentados e se esta pausado.
extern "C" JNIEXPORT jstring JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeBattleDebug(JNIEnv* env, jclass) {
  if (!rex::Runtime::instance()) return env->NewStringUTF("sem jogo");
  char text[160];
  std::snprintf(text, sizeof(text), "estado=%08X modo=%u pausa=%d",
                LoadGuestU32(kPlayStateFlags), LoadGuestU32(kPlayStateMode),
                g_swordman_pause.load(std::memory_order_relaxed) ? 1 : 0);
  return env->NewStringUTF(text);
}

// Frames apresentados (VdSwap) desde o arranque: o app marca a sessao como "a correr".
extern "C" JNIEXPORT jlong JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeFrames(JNIEnv*, jclass) {
  return static_cast<jlong>(g_swordman_ticks.load(std::memory_order_relaxed));
}

// 0107: tempo de GPU acumulado e censo de pixel shaders (autoteste de efeitos).
extern "C" void rex_swordman_gpu_stats(uint64_t* out);
extern "C" size_t rex_swordman_ps_census(char* buf, size_t size, int reset);

extern "C" void rex_swordman_set_skip_ps(const char* text);

extern "C" JNIEXPORT void JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeSetSkipPs(JNIEnv* env, jclass, jstring text) {
  const char* t = env->GetStringUTFChars(text, nullptr);
  rex_swordman_set_skip_ps(t);
  REXLOG_INFO("swordman: pixel shaders saltados: '{}'", t);
  env->ReleaseStringUTFChars(text, t);
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeGpuStats(JNIEnv* env, jclass) {
  uint64_t v[2] = {0, 0};
  rex_swordman_gpu_stats(v);
  jlongArray out = env->NewLongArray(2);
  const jlong j[2] = {static_cast<jlong>(v[0]), static_cast<jlong>(v[1])};
  env->SetLongArrayRegion(out, 0, 2, j);
  return out;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativePsCensus(JNIEnv* env, jclass, jboolean reset) {
  std::string text(rex_swordman_ps_census(nullptr, 0, 0) + 4096, '\0');
  const size_t n = rex_swordman_ps_census(text.data(), text.size(), reset ? 1 : 0);
  text.resize(std::min(n, text.size() - 1));
  return env->NewStringUTF(text.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeDumpFight(JNIEnv* env, jclass, jstring path, jint, jlong) {
  // 27/10: diagnostico para achar o estado "jogo/menus" do MGR (AutoPilot "memdump"): grava a secao de dados do XEX
  // (depois do codigo, ate ao fim da imagem) tal como esta na memoria do jogo, para comparar estados.
  constexpr uint32_t kBegin = 0x83200000u, kEnd = 0x83A40000u;
  auto* rt = rex::Runtime::instance();
  if (!rt || !rt->memory()) return JNI_FALSE;
  const char* p = env->GetStringUTFChars(path, nullptr);
  FILE* f = std::fopen(p, "wb");
  env->ReleaseStringUTFChars(path, p);
  if (!f) return JNI_FALSE;
  const auto* data = rt->memory()->TranslateVirtual<const uint8_t*>(kBegin);
  const bool ok = std::fwrite(data, 1, kEnd - kBegin, f) == kEnd - kBegin;
  std::fclose(f);
  return ok ? JNI_TRUE : JNI_FALSE;
}

// ---------------------------------------------------------------------------
// Menu do jogo do app (InGameMenu.kt / AndroidMenu.kt): muda cvars ao vivo e grava o
// mgr.toml, para o launcher ver as mesmas opcoes na proxima abertura.
// ---------------------------------------------------------------------------
extern "C" JNIEXPORT jboolean JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeSetCvars(JNIEnv* env, jclass, jobjectArray names,
                                                              jobjectArray values, jboolean persist) {
  if (!rex::Runtime::instance()) return JNI_FALSE;
  const jsize n = env->GetArrayLength(names);
  bool ok = true;
  std::string changed;
  for (jsize i = 0; i < n; ++i) {
    auto jn = static_cast<jstring>(env->GetObjectArrayElement(names, i));
    auto jv = static_cast<jstring>(env->GetObjectArrayElement(values, i));
    const char* cn = env->GetStringUTFChars(jn, nullptr);
    const char* cv = env->GetStringUTFChars(jv, nullptr);
    const bool set = rex::cvar::SetFlagByName(cn, cv);
    ok = set && ok;
    if (!changed.empty()) changed += ", ";
    changed += std::string(cn) + "=" + cv + (set ? "" : " (falhou)");
    env->ReleaseStringUTFChars(jn, cn);
    env->ReleaseStringUTFChars(jv, cv);
    env->DeleteLocalRef(jn);
    env->DeleteLocalRef(jv);
  }
  REXLOG_INFO("swordman: cvars ao vivo{}: {}", persist ? "" : " (sem gravar)", changed);
  // Autoteste e congelamento: so ao vivo. Gravados no mgr.toml ficavam ligados no arranque
  // seguinte (20/10: a imagem congelada ativa durante o jogo inteiro no RP5).
  if (persist) {
    rex::cvar::SaveConfig(rex::filesystem::GetExecutableFolder() / kConfigName);
  }
  return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeGetCvar(JNIEnv* env, jclass, jstring name) {
  if (!rex::Runtime::instance()) return env->NewStringUTF("");
  const char* cn = env->GetStringUTFChars(name, nullptr);
  const std::string value = rex::cvar::GetFlagByName(cn);
  env->ReleaseStringUTFChars(name, cn);
  return env->NewStringUTF(value.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeInvokeCommand(JNIEnv* env, jclass, jstring name,
                                                                   jstring args) {
  if (!rex::Runtime::instance()) return JNI_FALSE;
  const char* cn = env->GetStringUTFChars(name, nullptr);
  const char* ca = env->GetStringUTFChars(args, nullptr);
  const bool ok = rex::cvar::InvokeCommand(cn, ca);
  REXLOG_INFO("swordman: comando {} {} -> {}", cn, ca, ok ? "ok" : "falhou");
  env->ReleaseStringUTFChars(name, cn);
  env->ReleaseStringUTFChars(args, ca);
  return ok ? JNI_TRUE : JNI_FALSE;
}

// Sons de menu do proprio jogo (Dragon Burst). O MGR ainda nao: o menu do app fica mudo.
extern "C" JNIEXPORT void JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeMenuSound(JNIEnv*, jclass, jint) {}

// Linha no log do jogo (autoteste do render, menu do app, controles).
extern "C" JNIEXPORT void JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeLogLine(JNIEnv* env, jclass, jstring line) {
  const char* c = env->GetStringUTFChars(line, nullptr);
  REXLOG_INFO("swordman: {}", c);
  env->ReleaseStringUTFChars(line, c);
}

// ---------------------------------------------------------------------------
// Volume geral (0-100) do menu do app e do launcher. Ganho linear do SDK
// (rex::audio::SetOutputGain), aplicado ao som que sai para o Android. Pode ser
// chamado antes de o jogo arrancar (o ganho e global).
// ---------------------------------------------------------------------------
extern "C" JNIEXPORT void JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeSetVolume(JNIEnv*, jclass, jint percent) {
  const int p = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
  // Curva quadratica: o meio do controlo soa a "metade" (o ouvido e logaritmico).
  const float linear = (p / 100.0f) * (p / 100.0f);
  rex::audio::SetOutputGain(linear);
  if (rex::Runtime::instance()) {
    REXLOG_INFO("swordman: volume {}%", p);
  }
}

// ---------------------------------------------------------------------------
// Patch 0092 (120 FPS): taxa real da tela para o presenter e a linha "como os quadros
// chegam a tela" do autoteste.
// ---------------------------------------------------------------------------
extern "C" JNIEXPORT void JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeSetDisplayHz(JNIEnv*, jclass, jfloat hz) {
  static std::atomic<int> logged_hz{-1};
  rex::perf::SetDisplayRefreshRate(hz);
  const int rounded = int(hz + 0.5f);
  if (rex::Runtime::instance() && logged_hz.exchange(rounded) != rounded) {
    REXLOG_INFO("swordman: tela a {:.2f} Hz", hz);
  }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativePresentInfo(JNIEnv* env, jclass) {
  return env->NewStringUTF(rex::perf::GetPresentInfo().c_str());
}

// gpu_reset.cpp: o motivo do reinicio da GPU, ou "" enquanto ela estiver bem (GameActivity volta ao launcher).
extern "C" const char* rex_swordman_gpu_reset_reason();

extern "C" JNIEXPORT jstring JNICALL
Java_com_dfdx047_zandatsu_game_NativeGameState_nativeGpuReset(JNIEnv* env, jclass) {
  return env->NewStringUTF(rex_swordman_gpu_reset_reason());
}
