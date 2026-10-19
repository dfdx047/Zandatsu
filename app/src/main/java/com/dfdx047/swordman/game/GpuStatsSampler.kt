package com.dfdx047.swordman.game

import android.util.Log
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Le o clock e a carga da GPU do kernel (KGSL nos Adreno, devfreq nos outros) 4 vezes
 * por segundo e escreve um resumo a cada 5 s em logs/gpu_stats.log. Junto com a
 * linha `dbz3: perf ... gpu_ms` do jogo mostra se uma travadinha coincide com o
 * clock a descer (governador da GPU) ou com trabalho a mais.
 *
 * So corre com "Diagnostico de desempenho" ligado. Se o SELinux nao deixar ler os
 * ficheiros, escreve isso uma vez e para.
 */
class GpuStatsSampler(private val out: File) {
    private var thread: Thread? = null
    @Volatile private var running = false

    private val clkFile = firstReadable(
        "/sys/class/kgsl/kgsl-3d0/gpuclk",
        "/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq",
    ) ?: devfreqGpu()?.let { File(it, "cur_freq") }?.takeIf { it.canRead() }
    private val busyFile = firstReadable("/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage")
        ?: devfreqGpu()?.let { File(it, "load") }?.takeIf { it.canRead() }
    private val tempFile = firstReadable("/sys/class/kgsl/kgsl-3d0/temp")

    fun start() {
        if (running) return
        running = true
        thread = Thread({ loop() }, "SwordMan-GpuStats").apply {
            isDaemon = true
            start()
        }
    }

    fun stop() {
        running = false
        thread?.interrupt()
        thread = null
    }

    private fun loop() {
        val fmt = SimpleDateFormat("HH:mm:ss", Locale.ROOT)
        out.parentFile?.mkdirs()
        out.appendText(
            "# ${Date()} clk=${clkFile ?: "-"} busy=${busyFile ?: "-"} temp=${tempFile ?: "-"}\n",
        )
        if (clkFile == null && busyFile == null) {
            out.appendText("# sem acesso aos ficheiros da GPU (SELinux ou outro driver)\n")
            Log.i(TAG, "GPU stats: sem acesso aos ficheiros da GPU")
            return
        }
        val clks = ArrayList<Int>(32)
        val busy = ArrayList<Int>(32)
        var windowStart = System.nanoTime()
        while (running) {
            readMhz()?.let { clks += it }
            readBusy()?.let { busy += it }
            if (System.nanoTime() - windowStart >= 5_000_000_000L) {
                val line = buildString {
                    append(fmt.format(Date()))
                    if (clks.isNotEmpty()) {
                        append(" clk_mhz min=").append(clks.min())
                        append(" avg=").append(clks.average().toInt())
                        append(" max=").append(clks.max())
                        append(" trocas=").append(clks.zipWithNext().count { (a, b) -> a != b })
                    }
                    if (busy.isNotEmpty()) {
                        append(" busy% avg=").append(busy.average().toInt())
                        append(" max=").append(busy.max())
                    }
                    readTemp()?.let { append(" temp=").append(it).append("C") }
                }
                try {
                    out.appendText(line + "\n")
                } catch (_: Exception) {
                }
                Log.i(TAG, "GPU stats: $line")
                clks.clear()
                busy.clear()
                windowStart = System.nanoTime()
            }
            try {
                Thread.sleep(250)
            } catch (_: InterruptedException) {
                break
            }
        }
    }

    private fun read(f: File?): String? = try {
        f?.readText()?.trim()
    } catch (_: Exception) {
        null
    }

    /** gpuclk e cur_freq estao em Hz. */
    private fun readMhz(): Int? = read(clkFile)?.toLongOrNull()?.let { (it / 1_000_000L).toInt() }

    /** "37 %" (KGSL) ou "37@600000000" (devfreq load). */
    private fun readBusy(): Int? = read(busyFile)?.takeWhile { it.isDigit() }?.toIntOrNull()

    /** Milligraus (ou graus em alguns kernels). */
    private fun readTemp(): Int? = read(tempFile)?.toIntOrNull()?.let { if (it > 1000) it / 1000 else it }

    private companion object {
        const val TAG = "swordman"

        fun firstReadable(vararg paths: String): File? =
            paths.map(::File).firstOrNull { it.canRead() && runCatching { it.readText() }.isSuccess }

        fun devfreqGpu(): File? = File("/sys/class/devfreq").listFiles()?.firstOrNull {
            val n = it.name.lowercase(Locale.ROOT)
            "gpu" in n || "mali" in n || "kgsl" in n
        }
    }
}
