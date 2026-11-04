package com.dfdx047.zandatsu.diag

/**
 * libzdiag.so (app/src/main/cpp/diag): os micro-testes de Vulkan do teste de GPU, sem o jogo. So e carregada no
 * processo :diag ([DiagService]); se o driver derrubar o processo, o launcher continua.
 */
object DiagNative {
    init {
        System.loadLibrary("zdiag")
    }

    /** JSON com os ids dos testes, pela ordem. */
    @JvmStatic external fun tests(): String

    /** Corre um teste; devolve o JSON do resultado (tambem gravado em outDir/<test>.json). */
    @JvmStatic external fun run(
        test: String,
        outDir: String,
        nativeLibDir: String,
        driverDir: String?,
        driverName: String?,
        options: String,
    ): String

    /** Teste sem Vulkan neste processo (cpu_cores, no app em primeiro plano). */
    @JvmStatic external fun runInProcess(test: String, outDir: String, options: String): String

    /** Perfil de CPU do processo do jogo (no :game, durante o teste de desempenho): escreve em dir. */
    @JvmStatic external fun profileStart(dir: String): Boolean
}
