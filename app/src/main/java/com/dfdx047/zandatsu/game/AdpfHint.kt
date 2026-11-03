package com.dfdx047.zandatsu.game

import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.PerformanceHintManager
import android.os.SystemClock
import com.dfdx047.zandatsu.data.AppLog
import java.io.File

/**
 * ADPF (Android Performance Hint, Android 12+), como o ARMSX2: diz ao sistema que as threads que
 * mais trabalham no jogo precisam de um quadro a cada [targetNs], e vai reportando quanto cada
 * quadro demorou de facto. O governador de CPU deixa de baixar o clock no meio da luta.
 *
 * As threads sao escolhidas pelo tempo de CPU (as [MAX_THREADS] que mais gastaram em 3 s), depois
 * de o jogo arrancar; o tempo de cada quadro e a media dos quadros desenhados (os da GPU, nao os
 * apresentados: esses ficam a 60 com quadros repetidos) em cada meio segundo.
 *
 * 03/11 (issues #19, #24 e #35): a escolha era uma so, aos 20 s, ainda nos menus, com 4 threads; as threads
 * de trabalho do combate ficavam de fora. Na Mali-G615 (Dimensity 8300) a soma das threads gasta ~94 ms de
 * CPU por quadro (241% de um nucleo a 25,7 fps), contra ~36 ms no RP5, sem nenhuma thread acima de 40%.
 * Agora ate 8 threads, reescolhidas a cada [REPICK_MS] (setThreads no Android 14+, senao uma sessao nova).
 */
class AdpfHint(private val context: Context, private val targetNs: Long) {
    private val main = Handler(Looper.getMainLooper())
    private var session: PerformanceHintManager.Session? = null
    private var lastFrames = 0L
    private var lastTime = 0L
    private var stopped = false

    fun start(delayMs: Long = 20_000) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) return
        main.postDelayed({ pickThreads() }, delayMs)
    }

    fun stop() {
        stopped = true
        main.removeCallbacksAndMessages(null)
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) session?.close()
        } catch (_: Throwable) {
        }
        session = null
    }

    private fun pickThreads() {
        if (stopped) return
        Thread({
            val first = cpuTimes()
            SystemClock.sleep(3000)
            val second = cpuTimes()
            val busy = second.entries
                .map { (tid, t) -> tid to (t - (first[tid] ?: t)) }
                .filter { it.second > 0 }
                .sortedByDescending { it.second }
                .take(MAX_THREADS)
                .map { it.first }
            main.post { createSession(busy.toIntArray()) }
        }, "adpf-pick").start()
    }

    private var lastTids: Set<Int> = emptySet()

    private fun createSession(tids: IntArray) {
        if (stopped || tids.isEmpty() || Build.VERSION.SDK_INT < Build.VERSION_CODES.S) return
        main.postDelayed({ pickThreads() }, REPICK_MS)
        if (tids.toSet() == lastTids) return
        val old = session
        if (old != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            try {
                old.setThreads(tids)
                lastTids = tids.toSet()
                AppLog.i("adpf", "threads trocadas: ${tids.size} ${names(tids)}")
                return
            } catch (t: Throwable) {
                AppLog.i("adpf", "setThreads falhou: ${t.message}")
            }
        }
        if (old != null) {
            main.removeCallbacks(report)
            try {
                old.close()
            } catch (_: Throwable) {
            }
            session = null
        }
        try {
            val mgr = context.getSystemService(PerformanceHintManager::class.java) ?: return
            if (mgr.preferredUpdateRateNanos < 0) return
            session = mgr.createHintSession(tids, targetNs)
            if (session == null) {
                AppLog.i("adpf", "sem sessao (o sistema nao suporta)")
                return
            }
            lastTids = tids.toSet()
            AppLog.i("adpf", "sessao com ${tids.size} threads ${names(tids)}, alvo ${targetNs / 1000} us")
            lastFrames = NativeGameState.gpuStats()[0]
            lastTime = SystemClock.elapsedRealtimeNanos()
            main.postDelayed(report, REPORT_MS)
        } catch (t: Throwable) {
            AppLog.i("adpf", "falhou: ${t.message}")
        }
    }

    private val report = object : Runnable {
        override fun run() {
            val s = session ?: return
            if (stopped) return
            val frames = NativeGameState.gpuStats()[0]
            val now = SystemClock.elapsedRealtimeNanos()
            val df = frames - lastFrames
            if (df > 0 && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val perFrame = (now - lastTime) / df
                try {
                    s.reportActualWorkDuration(perFrame)
                } catch (_: Throwable) {
                }
            }
            lastFrames = frames
            lastTime = now
            main.postDelayed(this, REPORT_MS)
        }
    }

    /** tid -> tempo de CPU (utime + stime, em ticks) de cada thread do processo. */
    private fun cpuTimes(): Map<Int, Long> {
        val out = HashMap<Int, Long>()
        File("/proc/self/task").listFiles()?.forEach { dir ->
            val tid = dir.name.toIntOrNull() ?: return@forEach
            try {
                val stat = File(dir, "stat").readText()
                val rest = stat.substring(stat.lastIndexOf(')') + 2).split(' ')
                // Campos 14 e 15 do stat (utime, stime) sao os indices 11 e 12 depois do nome.
                out[tid] = rest[11].toLong() + rest[12].toLong()
            } catch (_: Exception) {
            }
        }
        return out
    }

    private fun names(tids: IntArray): String = tids.joinToString(",", "[", "]") { tid ->
        try {
            File("/proc/self/task/$tid/comm").readText().trim()
        } catch (_: Exception) {
            tid.toString()
        }
    }

    private companion object {
        const val MAX_THREADS = 8
        const val REPICK_MS = 30_000L
        const val REPORT_MS = 500L
    }
}
