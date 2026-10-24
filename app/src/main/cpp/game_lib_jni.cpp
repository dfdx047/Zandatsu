// libmain.so: a imagem PE do default.xex do jogador para o GameLib.kt (a chave do libgame.so é o SHA-256 dela).
// Sem dependências do jogo nem do runtime: o launcher carrega-a só para isto.

#include <jni.h>

#include <cstdio>
#include <vector>

#include "xex_image.h"

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_dfdx047_zandatsu_data_GameLib_nativeXexImage(JNIEnv* env, jclass, jstring path, jintArray title_out,
                                                      jobjectArray error_out) {
  auto error = [&](const char* e) {
    env->SetObjectArrayElement(error_out, 0, env->NewStringUTF(e));
    return nullptr;
  };
  const char* p = env->GetStringUTFChars(path, nullptr);
  FILE* f = std::fopen(p, "rb");
  env->ReleaseStringUTFChars(path, p);
  if (!f) return error("não abre o default.xex");
  std::vector<uint8_t> xex;
  uint8_t buf[1 << 16];
  for (size_t n; (n = std::fread(buf, 1, sizeof(buf), f)) > 0;) xex.insert(xex.end(), buf, buf + n);
  std::fclose(f);

  const auto img = zandatsu::ReadXexImage(xex.data(), xex.size());
  const jint title = jint(img.title_id);
  env->SetIntArrayRegion(title_out, 0, 1, &title);
  if (!img.error.empty()) return error(img.error.c_str());
  jbyteArray out = env->NewByteArray(jsize(img.pe.size()));
  if (!out) return error("sem memória para a imagem");
  env->SetByteArrayRegion(out, 0, jsize(img.pe.size()), reinterpret_cast<const jbyte*>(img.pe.data()));
  return out;
}
