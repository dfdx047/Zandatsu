package com.dfdx047.zandatsu.data

import android.util.Log
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.Executors

/**
 * Registo de tudo o que o app faz (launcher, importacao, drivers, menu do jogo, controles),
 * em logs/app.log e no logcat (tag "swordman"). Vai no zip do "Exportar logs".
 *
 * Nao pesa no jogo: as linhas vao para uma fila e uma unica thread de baixa prioridade
 * escreve-as no ficheiro; o jogo nunca espera pelo disco. O ficheiro e cortado a meio
 * quando passa de [MAX_BYTES] (fica a metade mais recente).
 */
object AppLog {
    private const val TAG = "swordman"
    private const val MAX_BYTES = 2L shl 20

    private val writer = Executors.newSingleThreadExecutor { r ->
        Thread(r, "SwordManLog").apply {
            isDaemon = true
            priority = Thread.MIN_PRIORITY
        }
    }
    private val time = object : ThreadLocal<SimpleDateFormat>() {
        override fun initialValue() = SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.ROOT)
    }

    /** Processo que escreve (o launcher e o jogo correm em processos diferentes). */
    @Volatile var process: String = "app"

    fun i(area: String, message: String) = write('I', area, message, null)
    fun w(area: String, message: String, error: Throwable? = null) = write('W', area, message, error)
    fun e(area: String, message: String, error: Throwable? = null) = write('E', area, message, error)

    private fun write(level: Char, area: String, message: String, error: Throwable?) {
        val text = "[$area] $message" + (error?.let { " (${it.javaClass.simpleName}: ${it.message})" } ?: "")
        when (level) {
            'E' -> Log.e(TAG, text, error)
            'W' -> Log.w(TAG, text, error)
            else -> Log.i(TAG, text)
        }
        val line = "${time.get()!!.format(Date())} $level $process $text\n"
        try {
            writer.execute { append(line) }
        } catch (_: Exception) {
        }
    }

    private fun append(line: String) {
        try {
            val f = AppPaths.appLog
            f.parentFile?.mkdirs()
            if (f.length() > MAX_BYTES) trim(f)
            f.appendText(line)
        } catch (_: Exception) {
            // Sem pasta (AppPaths ainda nao iniciado) ou sem espaco: fica so no logcat.
        }
    }

    private fun trim(f: File) {
        val bytes = f.readBytes()
        val keep = bytes.copyOfRange(bytes.size / 2, bytes.size)
        val nl = keep.indexOf('\n'.code.toByte()).coerceAtLeast(0)
        f.writeBytes(keep.copyOfRange(nl + 1, keep.size))
    }
}
