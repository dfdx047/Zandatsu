package com.dfdx047.zandatsu.data

import android.content.ContentValues
import android.content.Context
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import com.dfdx047.zandatsu.BuildConfig
import com.dfdx047.zandatsu.drivers.DriverRepository
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

/**
 * Junta num .zip tudo o que ajuda a depurar: logcat da app (launcher e jogo, linhas
 * `swordman`), o log da ultima sessao, o registo do app (logs/app.log), o mgr.toml, o
 * cvars.txt, o driver ativo e as propriedades do aparelho.
 * Grava em Download/ (visivel no PC por adb ou MTP), ou no destino escolhido ([exportTo]).
 */
object LogExporter {

    private fun run(vararg cmd: String): String = try {
        val p = ProcessBuilder(*cmd).redirectErrorStream(true).start()
        val out = p.inputStream.bufferedReader().use { it.readText() }
        p.waitFor()
        out
    } catch (e: Exception) {
        "erro: ${e.message}"
    }

    private fun deviceReport(): String = buildString {
        appendLine("Zandatsu ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE})")
        appendLine("Aparelho: ${Build.MANUFACTURER} ${Build.MODEL} (${Build.DEVICE})")
        appendLine("Android: ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
        appendLine("SoC: ${DriverRepository.socName()}  Adreno: ${DriverRepository.isAdreno()}")
        appendLine("Hardware: ${Build.HARDWARE}  Board: ${Build.BOARD}")
        appendLine("Driver Vulkan ativo: ${DriverRepository.activeDir() ?: "sistema"}")
        appendLine("Drivers instalados: " + DriverRepository.installed().joinToString { it.name })
        appendLine()
        appendLine("Arquivos do jogo:")
        try {
            GameFiles.check().checks.forEach { appendLine("  ${it.name}: ${it.status}") }
        } catch (e: Exception) {
            appendLine("  erro: ${e.message}")
        }
        appendLine()
        appendLine("Opcoes: " + GameConfig.load().toString())
        appendLine("Sessao anterior: " + SessionState.read().name)
        appendLine()
        appendLine("getprop (graficos):")
        run("getprop").lines().filter { l ->
            listOf("vulkan", "egl", "gfx", "gpu", "opengles", "hwui", "soc", "board.platform", "adreno", "mali")
                .any { l.contains(it, ignoreCase = true) }
        }.forEach { appendLine("  $it") }
    }

    /** Nome sugerido para o .zip (Zandatsu-logs-AAAAMMDD-HHMMSS.zip). */
    fun suggestedName(): String =
        "Zandatsu-logs-" + SimpleDateFormat("yyyyMMdd-HHmmss", Locale.ROOT).format(Date()) + ".zip"

    /** Grava o .zip num destino escolhido pelo utilizador (SAF, aviso de erro "Guardar log"). */
    fun exportTo(context: Context, uri: android.net.Uri) {
        val tmp = build(context, suggestedName())
        try {
            context.contentResolver.openOutputStream(uri)?.use { out -> tmp.inputStream().use { it.copyTo(out) } }
                ?: error("sem acesso ao destino")
        } finally {
            tmp.delete()
        }
        AppLog.i("logs", "exportado para $uri")
    }

    /** Cria o .zip e devolve o nome com que ficou em Download/. */
    fun export(context: Context): String {
        val name = suggestedName()
        val tmp = build(context, name)
        val values = ContentValues().apply {
            put(MediaStore.Downloads.DISPLAY_NAME, name)
            put(MediaStore.Downloads.MIME_TYPE, "application/zip")
            put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS)
        }
        val resolver = context.contentResolver
        try {
            val uri = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
                ?: error("sem acesso a Download/")
            resolver.openOutputStream(uri)?.use { out -> tmp.inputStream().use { it.copyTo(out) } }
        } finally {
            tmp.delete()
        }
        AppLog.i("logs", "exportado: Download/$name")
        return name
    }

    private fun build(context: Context, name: String): File {
        val tmp = File(context.cacheDir, name)
        ZipOutputStream(tmp.outputStream().buffered()).use { zip ->
            fun put(entry: String, text: String) {
                zip.putNextEntry(ZipEntry(entry))
                zip.write(text.toByteArray())
                zip.closeEntry()
            }
            fun putFile(entry: String, f: File) {
                if (!f.isFile) return
                zip.putNextEntry(ZipEntry(entry))
                f.inputStream().use { it.copyTo(zip) }
                zip.closeEntry()
            }
            put("aparelho.txt", deviceReport())
            put("logcat.txt", run("logcat", "-d", "-v", "threadtime"))
            AppPaths.logsDir.listFiles()?.forEach { putFile("logs/${it.name}", it) }
            // Capturas de frames (.dfc): so a mais recente, que pode ter dezenas de MB.
            File(AppPaths.logsDir, "capture").listFiles { f -> f.extension == "dfc" }
                ?.maxByOrNull { it.lastModified() }?.let { putFile("logs/capture/${it.name}", it) }
            // Tempos por draw de cada captura (pequenos): todos.
            File(AppPaths.logsDir, "capture").listFiles { f -> f.name.endsWith(".timing.txt") }
                ?.forEach { putFile("logs/capture/${it.name}", it) }
            putFile("mgr.toml", AppPaths.userToml)
            putFile("cvars.txt", File(AppPaths.appDir, "cvars.txt"))
            File(AppPaths.appDir, "cache").takeIf { it.isDirectory }?.walkTopDown()
                ?.filter { it.isFile && it.extension == "log" }
                ?.forEach { putFile("cache/${it.name}", it) }
        }
        return tmp
    }
}
