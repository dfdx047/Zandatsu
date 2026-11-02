package com.dfdx047.zandatsu.data

import android.app.ActivityManager
import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import java.io.File
import java.io.InputStream
import java.io.OutputStream
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream
import java.util.zip.ZipOutputStream

/** Saves do jogo: exportar em zip, listar, restaurar (com backup automatico antes). */
object SaveManager {
    /** Pasta do perfil do jogo: <app>/mgr/<perfil>/4B4E080A/00000001/SaveData. */
    private const val TITLE_ID = "4B4E080A"
    private const val MARKER = "zandatsu_saves.txt"

    data class Backup(val name: String, val size: Long, val time: Long, val uri: Uri?, val file: File?)

    private val autoDir get() = File(AppPaths.internalDir, "save_backups")

    /** Todas as pastas SaveData (normalmente uma, por perfil). */
    fun saveDirs(): List<File> =
        File(AppPaths.appDir, "mgr").listFiles()?.mapNotNull { profile ->
            File(profile, "$TITLE_ID/00000001/SaveData").takeIf { it.isDirectory }
        }.orEmpty()

    private fun saveFiles(): List<Pair<File, String>> = saveDirs().flatMap { dir ->
        val prefix = dir.parentFile!!.parentFile!!.parentFile!!.name + "/SaveData/"
        dir.walkTopDown().filter { it.isFile && it.name != ".swordman_test" }.map { it to prefix + it.relativeTo(dir).path }.toList()
    }

    fun hasSaves() = saveFiles().isNotEmpty()

    /** O jogo corre noutro processo: restaurar com ele aberto seria sobrescrito ao fechar. */
    fun gameRunning(ctx: Context): Boolean {
        val am = ctx.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        return am.runningAppProcesses.orEmpty().any { it.processName.endsWith(":game") }
    }

    private fun stamp() = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date())

    private fun writeZip(out: OutputStream): Int {
        var n = 0
        ZipOutputStream(out.buffered()).use { z ->
            z.putNextEntry(ZipEntry(MARKER))
            z.write("Zandatsu saves ${stamp()}\n".toByteArray())
            z.closeEntry()
            for ((f, name) in saveFiles()) {
                z.putNextEntry(ZipEntry(name).apply { time = f.lastModified() })
                f.inputStream().use { it.copyTo(z) }
                z.closeEntry()
                n++
            }
        }
        return n
    }

    fun exportTo(ctx: Context, tree: Uri): String {
        val dir = DocumentFile.fromTreeUri(ctx, tree) ?: error("folder")
        val name = "Zandatsu-saves-${stamp()}.zip"
        val doc = dir.createFile("application/zip", name) ?: error("createFile")
        val n = ctx.contentResolver.openOutputStream(doc.uri)?.use { writeZip(it) } ?: error("open")
        AppLog.i("saves", "exportado $name: $n arquivos")
        return name
    }

    /** Backup interno feito antes de restaurar (guarda os 5 mais novos). */
    fun autoBackup(): File? {
        if (!hasSaves()) return null
        autoDir.mkdirs()
        val f = File(autoDir, "auto-${stamp()}.zip")
        f.outputStream().use { writeZip(it) }
        autoDir.listFiles()?.sortedByDescending { it.lastModified() }?.drop(5)?.forEach { it.delete() }
        return f
    }

    fun list(ctx: Context, tree: Uri?): List<Backup> {
        val out = ArrayList<Backup>()
        if (tree != null) {
            runCatching {
                DocumentFile.fromTreeUri(ctx, tree)?.listFiles()?.forEach { d ->
                    val n = d.name.orEmpty()
                    if (n.startsWith("Zandatsu-saves-") && n.endsWith(".zip")) out += Backup(n, d.length(), d.lastModified(), d.uri, null)
                }
            }
        }
        autoDir.listFiles()?.forEach { f -> out += Backup(f.name, f.length(), f.lastModified(), null, f) }
        return out.sortedByDescending { it.time }
    }

    private fun open(ctx: Context, b: Backup): InputStream =
        b.file?.inputStream() ?: ctx.contentResolver.openInputStream(b.uri!!) ?: error("open")

    /** Confere o zip sem tocar em nada: tem de ser nosso, sem caminhos a sair da pasta, com pelo menos um save. */
    private fun validate(ctx: Context, b: Backup): Int {
        var marker = false
        var n = 0
        ZipInputStream(open(ctx, b)).use { z ->
            while (true) {
                val e = z.nextEntry ?: break
                if (e.name == MARKER) marker = true
                else if (!e.isDirectory) {
                    check(!e.name.contains("..") && !e.name.startsWith("/") && e.name.contains("/SaveData/")) { "entry ${e.name}" }
                    n++
                }
            }
        }
        check(marker && n > 0) { "not a Zandatsu backup" }
        return n
    }

    /** Devolve o numero de arquivos restaurados. So apaga os saves atuais depois de validar e fazer o backup. */
    fun restore(ctx: Context, b: Backup): Int {
        check(!gameRunning(ctx)) { "game running" }
        validate(ctx, b)
        autoBackup()
        val root = File(AppPaths.appDir, "mgr")
        val extract = File(AppPaths.cacheDir, "save_restore").apply { deleteRecursively(); mkdirs() }
        var n = 0
        ZipInputStream(open(ctx, b)).use { z ->
            while (true) {
                val e = z.nextEntry ?: break
                if (e.isDirectory || e.name == MARKER) continue
                val f = File(extract, e.name)
                check(f.canonicalPath.startsWith(extract.canonicalPath + File.separator)) { "entry ${e.name}" }
                f.parentFile?.mkdirs()
                f.outputStream().use { z.copyTo(it) }
                n++
            }
        }
        extract.listFiles()?.forEach { profile ->
            val src = File(profile, "SaveData")
            if (!src.isDirectory) return@forEach
            val dst = File(root, "${profile.name}/$TITLE_ID/00000001/SaveData")
            dst.deleteRecursively()
            dst.mkdirs()
            src.copyRecursively(dst, overwrite = true)
        }
        extract.deleteRecursively()
        AppLog.i("saves", "restaurado ${b.name}: $n arquivos")
        return n
    }
}
