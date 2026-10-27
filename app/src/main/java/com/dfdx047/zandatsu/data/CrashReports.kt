package com.dfdx047.zandatsu.data

import android.app.ActivityManager
import android.app.ApplicationExitInfo
import android.content.Context
import java.io.File

/** Tipo de falha mostrado no aviso de erro do launcher (cada um com o seu texto). */
enum class ErrorKind {
    /** O jogo fechou sozinho depois de comecar a desenhar. */
    CRASH,
    /** O jogo fechou antes de apresentar o primeiro frame. */
    NO_BOOT,
    /** O driver Vulkan alternativo fechou o jogo no arranque (o seguinte usou o do sistema). */
    DRIVER_BOOT,
    /** Falha a descarregar/instalar/importar um driver. */
    DRIVER_INSTALL,
    /** A ISO ou os arquivos importados estao corrompidos/incompletos. */
    BAD_ISO,
    /** Outra falha da importacao do jogo (sem espaco, pasta errada...). */
    IMPORT,
    /** A GPU travou/falhou e o jogo voltou ao launcher em vez de fechar (logs/gpu_reset.txt, gpu_reset.cpp). */
    GPU_RESET,
}

/**
 * Um aviso de erro: o tipo, uma linha tecnica (motivo do Android, nome do ficheiro...) e as
 * ultimas linhas dos logs, para o jogador ver logo e para a mensagem no Discord.
 */
data class CrashReport(
    val kind: ErrorKind,
    val detail: String,
    val tail: String,
    val time: Long = System.currentTimeMillis(),
)

object CrashReports {
    private const val GAME_PROCESS_SUFFIX = ":game"

    /**
     * Ve se a ultima sessao do jogo acabou mal. So devolve um aviso uma vez por saida
     * ([LauncherPrefs.lastCrashShown] guarda a hora da ultima mostrada).
     *
     * Precisa das duas coisas: a sessao nao marcou "ENDED" (SessionState) e o Android registou
     * uma saida anormal do processo :game depois de ela comecar (crash Java ou nativo, ANR,
     * falta de memoria, sinal que nao o SIGKILL do proprio menu). Fechar o jogo pelos recentes
     * nao conta: o motivo e "remove task" (REASON_OTHER / USER_REQUESTED).
     */
    @Synchronized
    fun lastGameExit(context: Context, prefs: LauncherPrefs): CrashReport? {
        gpuReset(prefs)?.let { return it }
        val state = SessionState.read()
        if (state != SessionState.State.STARTING && state != SessionState.State.RUNNING) return null
        val started = SessionState.time()
        val am = context.getSystemService(ActivityManager::class.java) ?: return null
        val exits = try {
            am.getHistoricalProcessExitReasons(context.packageName, 0, 8)
        } catch (e: Exception) {
            AppLog.w("crash", "sem historico de saidas", e)
            return null
        }
        val exit = exits.firstOrNull { it.processName.endsWith(GAME_PROCESS_SUFFIX) && it.timestamp >= started - 2000 }
            ?: return null
        if (exit.timestamp <= prefs.lastCrashShown) return null
        if (!isAbnormal(exit)) {
            // Saiu de outra forma (recentes, sistema): nao e erro, mas fica registado.
            AppLog.i("crash", "jogo terminou: ${describe(exit)}")
            SessionState.acknowledge()
            return null
        }
        prefs.lastCrashShown = exit.timestamp
        SessionState.acknowledge()
        // Com driver alternativo (Turnip) ativo, conta os arranques seguidos sem frames; a partir
        // do 3o sugere o driver do sistema (o jogo nunca troca sozinho: otimizamos para o Turnip).
        val custom = com.dfdx047.zandatsu.drivers.DriverRepository.activeDir() != null
        val fails = if (state == SessionState.State.STARTING && custom) DriverBootFails.increment() else 0
        val kind = when {
            fails >= DriverBootFails.LIMIT -> ErrorKind.DRIVER_BOOT
            state == SessionState.State.STARTING -> ErrorKind.NO_BOOT
            else -> ErrorKind.CRASH
        }
        val detail = describe(exit) + if (fails > 0) " | driver alternativo: falha $fails/${DriverBootFails.LIMIT}" else ""
        val report = CrashReport(kind, detail, tail(), exit.timestamp)
        AppLog.e("crash", "${report.kind}: ${report.detail}")
        return report
    }

    /**
     * 27/10: a ultima sessao acabou com a GPU a reiniciar (o jogo parou de desenhar e voltou ao launcher, ou o driver
     * caiu e o tratador de sinais deixou o motivo): logs/gpu_reset.txt = "<hora ms>\n<motivo>". Mostra-se uma vez.
     */
    private fun gpuReset(prefs: LauncherPrefs): CrashReport? {
        val f = File(File(AppPaths.appDir, "logs"), "gpu_reset.txt")
        val lines = try {
            if (f.isFile) f.readLines() else return null
        } catch (_: Exception) {
            return null
        }
        val time = lines.firstOrNull()?.trim()?.toLongOrNull() ?: return null
        if (time <= prefs.lastCrashShown || time < SessionState.time() - 2000) return null
        prefs.lastCrashShown = time
        SessionState.acknowledge()
        val reason = lines.getOrNull(1)?.trim().orEmpty()
        AppLog.e("crash", "GPU_RESET: $reason")
        return CrashReport(ErrorKind.GPU_RESET, reason, tail(), time)
    }

    private fun isAbnormal(e: ApplicationExitInfo): Boolean = when (e.reason) {
        ApplicationExitInfo.REASON_CRASH,
        ApplicationExitInfo.REASON_CRASH_NATIVE,
        ApplicationExitInfo.REASON_ANR,
        ApplicationExitInfo.REASON_INITIALIZATION_FAILURE,
        ApplicationExitInfo.REASON_LOW_MEMORY,
        ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE,
        -> true
        // killProcess do menu do jogo e SIGKILL (9); outros sinais (SIGABRT, SIGSEGV) sao crash.
        ApplicationExitInfo.REASON_SIGNALED -> e.status != 9
        else -> false
    }

    fun describe(e: ApplicationExitInfo): String {
        val reason = when (e.reason) {
            ApplicationExitInfo.REASON_CRASH -> "crash (Java)"
            ApplicationExitInfo.REASON_CRASH_NATIVE -> "crash nativo"
            ApplicationExitInfo.REASON_ANR -> "ANR (sem resposta)"
            ApplicationExitInfo.REASON_INITIALIZATION_FAILURE -> "falha ao iniciar"
            ApplicationExitInfo.REASON_LOW_MEMORY -> "sem memoria"
            ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE -> "uso excessivo de recursos"
            ApplicationExitInfo.REASON_SIGNALED -> "sinal ${e.status}"
            ApplicationExitInfo.REASON_EXIT_SELF -> "saida (${e.status})"
            ApplicationExitInfo.REASON_USER_REQUESTED -> "fechado pelo utilizador"
            else -> "motivo ${e.reason}"
        }
        val desc = e.description?.takeIf { it.isNotBlank() }?.let { " - $it" } ?: ""
        return reason + desc
    }

    /** Ultimas linhas uteis: avisos/erros do log do jogo, depois o fim do registo do app. */
    fun tail(maxLines: Int = 14): String {
        val out = ArrayList<String>()
        lastLines(AppPaths.lastRunLog, 400)
            .filter { l -> ERROR_HINTS.any { l.contains(it, ignoreCase = true) } }
            .takeLast(maxLines - 4)
            .forEach { out += it.take(220) }
        lastLines(AppPaths.appLog, 4).forEach { out += it.take(220) }
        return out.joinToString("\n")
    }

    private val ERROR_HINTS = listOf(" E ", "[E]", "error", "erro", "fatal", "fail", "falh", "abort", "crash", "unregistered", "vk_error")

    private fun lastLines(f: File, n: Int): List<String> = try {
        if (!f.isFile) emptyList() else {
            // So o fim do ficheiro (o log do jogo pode ter dezenas de MB).
            java.io.RandomAccessFile(f, "r").use { raf ->
                val len = raf.length()
                val start = (len - 128 * 1024).coerceAtLeast(0)
                raf.seek(start)
                val bytes = ByteArray((len - start).toInt())
                raf.readFully(bytes)
                String(bytes).lines().filter { it.isNotBlank() }.takeLast(n)
            }
        }
    } catch (_: Exception) {
        emptyList()
    }

    /** Aviso para uma falha de importacao, driver, etc. que o launcher apanhou. */
    fun of(kind: ErrorKind, detail: String): CrashReport {
        AppLog.e("erro", "$kind: $detail")
        return CrashReport(kind, detail, tail(6))
    }
}
