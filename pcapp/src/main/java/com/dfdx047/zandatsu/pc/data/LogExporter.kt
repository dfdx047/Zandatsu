package com.dfdx047.zandatsu.pc.data

import android.content.Context
import android.net.Uri
import android.os.Build
import com.dfdx047.zandatsu.pc.BuildConfig
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

/**
 * "Guardar log" (como na app do 360): um zip com a pasta logs/, o logcat desta app (launcher e jogo, o mesmo uid) e
 * um resumo do aparelho, gravado onde o jogador escolher (SAF).
 */
object LogExporter {
    fun suggestedName(): String =
        "zandatsu-pc-logs-" + SimpleDateFormat("yyyyMMdd-HHmm", Locale.ROOT).format(Date()) + ".zip"

    fun writeTo(context: Context, uri: Uri) {
        val out = context.contentResolver.openOutputStream(uri) ?: error("sem saida")
        ZipOutputStream(out.buffered()).use { zip ->
            fun put(name: String, bytes: ByteArray) {
                zip.putNextEntry(ZipEntry(name)); zip.write(bytes); zip.closeEntry()
            }
            AppPaths.logsDir.listFiles().orEmpty().filter(File::isFile).forEach { f -> put("logs/${f.name}", f.readBytes()) }
            val logcat = runCatching {
                Runtime.getRuntime().exec(arrayOf("logcat", "-d", "-v", "threadtime", "-t", "20000")).inputStream.readBytes()
            }.getOrNull()
            if (logcat != null) put("logcat.txt", logcat)
            val info = buildString {
                appendLine("app ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE})")
                appendLine("device ${Build.MANUFACTURER} ${Build.MODEL} (${Build.DEVICE})")
                appendLine("android ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
                appendLine("soc ${Build.SOC_MANUFACTURER} ${Build.SOC_MODEL}")
            }
            put("device.txt", info.toByteArray())
        }
    }
}
