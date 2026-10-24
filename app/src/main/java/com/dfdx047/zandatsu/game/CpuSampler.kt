package com.dfdx047.zandatsu.game

import java.io.File

/**
 * Uso de CPU por thread do processo do jogo, a cada 5 s, no log ("swordman: cpu ...").
 * Le /proc/self/task/<tid>/stat (utime + stime, em ticks de 10 ms) e mostra as threads que mais
 * gastaram, em % de UM nucleo: uma thread a ~100% e o gargalo (jogo, GPU/CP, audio...).
 * Serve para saber se um aparelho esta preso na CPU ou na GPU (RG Cube, 21/10).
 */
object CpuSampler {
    private const val PERIOD_MS = 5000L
    private const val TICKS_PER_S = 100.0
    @Volatile private var running = false

    @JvmStatic
    fun start() {
        if (running) return
        running = true
        Thread({
            var last = sample()
            var lastTime = System.nanoTime()
            while (running) {
                try {
                    Thread.sleep(PERIOD_MS)
                } catch (_: InterruptedException) {
                    return@Thread
                }
                val now = sample()
                val t = System.nanoTime()
                val secs = (t - lastTime) / 1e9
                val usage = now.mapNotNull { (tid, e) ->
                    val prev = last[tid] ?: return@mapNotNull null
                    val pct = (e.second - prev.second) / TICKS_PER_S / secs * 100.0
                    if (pct >= 3.0) e.first to pct else null
                }.sortedByDescending { it.second }
                val total = usage.sumOf { it.second }
                val text = usage.take(8).joinToString(" | ") { (name, pct) ->
                    String.format(java.util.Locale.ROOT, "%s %.0f%%", name, pct)
                }
                NativeGameState.logLine(String.format(java.util.Locale.ROOT,
                    "cpu %.0fs total %.0f%% (de 1 nucleo): %s", secs, total, text))
                last = now
                lastTime = t
            }
        }, "SwordManCpu").apply { isDaemon = true; priority = Thread.MIN_PRIORITY }.start()
    }

    /** tid -> (nome, ticks de CPU). */
    private fun sample(): Map<Int, Pair<String, Long>> {
        val out = HashMap<Int, Pair<String, Long>>()
        File("/proc/self/task").listFiles()?.forEach { dir ->
            val tid = dir.name.toIntOrNull() ?: return@forEach
            try {
                val stat = File(dir, "stat").readText()
                // "tid (nome com espacos) S ..." - os campos depois do ")" comecam no 3.
                val close = stat.lastIndexOf(')')
                val name = stat.substring(stat.indexOf('(') + 1, close)
                val f = stat.substring(close + 2).split(' ')
                val ticks = f[11].toLong() + f[12].toLong()  // utime (14) + stime (15)
                out[tid] = "$name/$tid" to ticks
            } catch (_: Exception) {
            }
        }
        return out
    }
}
