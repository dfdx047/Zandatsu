package com.dfdx047.zandatsu.data

import java.io.File

/**
 * Estado da ultima sessao do jogo, num ficheiro pequeno (logs/session.txt) que o processo do
 * jogo escreve e o launcher le ao voltar:
 *   STARTING  o jogo abriu mas ainda nao apresentou frames
 *   RUNNING   ja apresentou frames
 *   ENDED     saiu pelo menu (normal)
 * Se o launcher encontrar STARTING ou RUNNING, o jogo fechou sozinho: "nao abriu" ou "crash".
 */
object SessionState {
    enum class State { NONE, STARTING, RUNNING, ENDED }

    private fun file(appDir: File) = File(File(appDir, "logs"), "session.txt")

    private fun write(appDir: File, s: State) {
        try {
            val f = file(appDir)
            f.parentFile?.mkdirs()
            f.writeText(s.name + " " + System.currentTimeMillis())
        } catch (_: Exception) {
        }
    }

    fun markStarting(appDir: File) = write(appDir, State.STARTING)
    fun markRunning(appDir: File) = write(appDir, State.RUNNING)
    fun markEnded(appDir: File) = write(appDir, State.ENDED)

    fun read(appDir: File = AppPaths.appDir): State = try {
        val t = file(appDir).takeIf { it.isFile }?.readText()?.substringBefore(' ')?.trim()
        State.entries.firstOrNull { it.name == t } ?: State.NONE
    } catch (_: Exception) {
        State.NONE
    }

    /** Hora (ms) da ultima mudanca de estado, 0 sem ficheiro. */
    fun time(appDir: File = AppPaths.appDir): Long = try {
        file(appDir).takeIf { it.isFile }?.readText()?.substringAfter(' ')?.trim()?.toLongOrNull() ?: 0L
    } catch (_: Exception) {
        0L
    }

    /** O launcher ja tratou o fim anormal: nao voltar a avisar. */
    fun acknowledge(appDir: File = AppPaths.appDir) = write(appDir, State.ENDED)
}
