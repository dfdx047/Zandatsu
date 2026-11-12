package com.dfdx047.zandatsu.data

import java.io.File

/**
 * 12/11: swordman_job_serial (mgr_cpu_hooks.cpp) vem ligado. Em ~1 de 25 cargas de luta (Odin 2 Mini) uma thread do jogo
 * leu um ponteiro nulo + 0xD0 (sub_82CCCE18) e o jogo parou: nao e um crash do processo, por isso o aviso de erro
 * nao o apanha. Ao voltar a launcher, se o log da sessao anterior tem "job serial ligado" e um "FATAL guest memory
 * fault", o serial fica desligado neste aparelho (job_serial_off.txt) ate o jogador o ligar de novo.
 */
object JobSerialGuard {
    private fun marker() = File(AppPaths.appDir, "job_serial_off.txt")
    private fun checked() = File(AppPaths.appDir, "job_serial_checked.txt")

    fun tripped(): Boolean = try {
        marker().isFile
    } catch (_: Exception) {
        false
    }

    /** O jogador voltou a ligar a opcao. */
    fun reset() {
        try {
            marker().delete()
        } catch (_: Exception) {
        }
    }

    /** Ve o log da ultima sessao uma vez; devolve true se acabou de desligar o serial. */
    @Synchronized
    fun checkLastSession(): Boolean {
        try {
            if (tripped()) return false
            val log = File(File(AppPaths.appDir, "logs"), "swordman_last.log")
            if (!log.isFile) return false
            val stamp = log.lastModified().toString() + " " + log.length()
            val seen = checked()
            if (seen.isFile && seen.readText().trim() == stamp) return false
            seen.writeText(stamp)
            var serial = false
            var fatal: String? = null
            log.useLines { lines ->
                for (l in lines) {
                    if (!serial && l.contains("job serial ligado")) serial = true
                    if (fatal == null && l.contains("FATAL guest memory fault")) fatal = l.substringAfter("FATAL ").take(120)
                    if (serial && fatal != null) break
                }
            }
            if (!serial || fatal == null) return false
            marker().writeText(fatal + "\n")
            AppLog.w("cpu", "job serial desligado: o jogo travou com ele ligado ($fatal)")
            return true
        } catch (e: Exception) {
            AppLog.w("cpu", "job serial: falha a ver o log", e)
            return false
        }
    }
}
