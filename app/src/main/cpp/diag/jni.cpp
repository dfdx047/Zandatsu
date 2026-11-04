// zdiag: entradas JNI para com.dfdx047.zandatsu.diag.DiagNative (metodos @JvmStatic external).
//   tests(): String  -> array JSON com os ids pela ordem de execucao
//   run(test, outDir, nativeLibDir, driverDir?, driverName?, options): String -> JSON do resultado
//     (tambem escrito em <outDir>/<test>.json). Cada teste deve correr num processo novo.
#include <jni.h>

#include "zdiag.h"

namespace {
std::string Str(JNIEnv* env, jstring s) {
  if (!s) return {};
  const char* c = env->GetStringUTFChars(s, nullptr);
  std::string r = c ? c : "";
  if (c) env->ReleaseStringUTFChars(s, c);
  return r;
}

// new String(bytes, "UTF-8"): os nomes/infos dos drivers podem trazer bytes que nao sao UTF-8 modificado
// (o NewStringUTF abortaria com CheckJNI).
jstring JStr(JNIEnv* env, const std::string& s) {
  jbyteArray bytes = env->NewByteArray(jsize(s.size()));
  env->SetByteArrayRegion(bytes, 0, jsize(s.size()), reinterpret_cast<const jbyte*>(s.data()));
  jclass cls = env->FindClass("java/lang/String");
  jmethodID ctor = env->GetMethodID(cls, "<init>", "([BLjava/lang/String;)V");
  jstring charset = env->NewStringUTF("UTF-8");
  jstring r = static_cast<jstring>(env->NewObject(cls, ctor, bytes, charset));
  env->DeleteLocalRef(charset);
  env->DeleteLocalRef(bytes);
  env->DeleteLocalRef(cls);
  return r;
}
}  // namespace

extern "C" JNIEXPORT jstring JNICALL Java_com_dfdx047_zandatsu_diag_DiagNative_tests(JNIEnv* env, jclass) {
  return JStr(env, zdiag::TestsJson());
}

extern "C" JNIEXPORT jstring JNICALL Java_com_dfdx047_zandatsu_diag_DiagNative_run(
    JNIEnv* env, jclass, jstring test, jstring out_dir, jstring native_lib_dir, jstring driver_dir,
    jstring driver_name, jstring options) {
  zdiag::LoaderInfo li{Str(env, native_lib_dir), Str(env, driver_dir), Str(env, driver_name)};
  std::string json = zdiag::RunTest(Str(env, test), Str(env, out_dir), li, Str(env, options));
  return JStr(env, json);
}

// Teste sem Vulkan no processo de quem chama (cpu_cores, no app: o :diag fica no cpuset de fundo, so nucleos 0-3).
extern "C" JNIEXPORT jstring JNICALL Java_com_dfdx047_zandatsu_diag_DiagNative_runInProcess(JNIEnv* env, jclass,
                                                                                         jstring test, jstring out_dir,
                                                                                         jstring options) {
  std::string json = zdiag::RunTest(Str(env, test), Str(env, out_dir), {}, Str(env, options), true);
  return JStr(env, json);
}

// Perfil de CPU no processo do jogo (teste de desempenho): profileStart(dir) -> true se arrancou.
extern "C" JNIEXPORT jboolean JNICALL Java_com_dfdx047_zandatsu_diag_DiagNative_profileStart(JNIEnv* env, jclass,
                                                                                         jstring dir) {
  return zdiag::StartProfiler(Str(env, dir)) ? JNI_TRUE : JNI_FALSE;
}
