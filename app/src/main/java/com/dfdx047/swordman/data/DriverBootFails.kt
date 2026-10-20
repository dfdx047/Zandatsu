package com.dfdx047.swordman.data

import java.io.File

/**
 * Arranques seguidos em que o jogo fechou antes do primeiro frame com um driver alternativo
 * (Turnip) ativo, num ficheiro (logs/driver_fails.txt) que os dois processos veem.
 *
 * As otimizacoes deste port sao feitas para o Turnip, por isso o jogo nunca volta sozinho ao
 * driver do sistema: o launcher conta as falhas e, a partir de [LIMIT], sugere o driver do
 * sistema (com o log para enviar). O jogo zera a contagem quando apresenta os primeiros frames;
 * trocar de driver tambem.
 */
object DriverBootFails {
    const val LIMIT = 3

    private fun file(appDir: File) = File(File(appDir, "logs"), "driver_fails.txt")

    fun read(appDir: File = AppPaths.appDir): Int = try {
        file(appDir).takeIf { it.isFile }?.readText()?.trim()?.toIntOrNull() ?: 0
    } catch (_: Exception) {
        0
    }

    fun increment(appDir: File = AppPaths.appDir): Int {
        val n = read(appDir) + 1
        try {
            file(appDir).also { it.parentFile?.mkdirs() }.writeText(n.toString())
        } catch (_: Exception) {
        }
        return n
    }

    @JvmStatic
    fun reset(appDir: File) {
        try {
            file(appDir).delete()
        } catch (_: Exception) {
        }
    }

    fun reset() = reset(AppPaths.appDir)
}
