package com.dfdx047.zandatsu.data

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import java.io.File
import java.io.RandomAccessFile

/**
 * DLC do 360 importados pelo jogador (12/11). Os pacotes (STFS "LIVE", tal como saem da pasta Content do console:
 * Content/0000000000000000/4B4E080A/00000002/<nome>) ficam em <appDir>/dlc/. No arranque do jogo, src/mgr_dlc.cpp
 * instala cada pacote novo com o ContentManager do SDK (extrai para mgr/0000000000000000/4B4E080A/00000002/<nome>/)
 * apaga o pacote e deixa a marca "<nome>.instalado" (com o nome do DLC). Os DLC do MGR so tem dados (data101..108, videos, som): o codigo ja esta no
 * default.xex.
 */
object DlcFiles {
    const val TITLE_ID = "4B4E080A"

    data class Package(val file: String, val name: String, val bytes: Long, val installed: Boolean)

    val dir: File get() = File(AppPaths.appDir, "dlc")
    private val installRoot: File get() = File(AppPaths.appDir, "mgr/0000000000000000/$TITLE_ID/00000002")

    /**
     * Pacotes por instalar (o arquivo ainda em [dir]) e instalados (so a marca: o jogo apaga o pacote depois de o
     * extrair, para um DLC de 1 GB nao ocupar 2 GB; a marca guarda o nome).
     */
    fun list(): List<Package> = (dir.listFiles() ?: emptyArray())
        .filter { it.isFile && !it.name.endsWith(".part") }
        .map { f ->
            if (f.name.endsWith(MARKER)) {
                val file = f.name.removeSuffix(MARKER)
                val name = runCatching { f.readText().trim() }.getOrNull().orEmpty().ifEmpty { file }
                Package(file, name, File(installRoot, file).walkBottomUp().filter { it.isFile }.sumOf { it.length() }, true)
            } else {
                Package(f.name, displayName(f) ?: f.name, f.length(), File(dir, f.name + MARKER).isFile)
            }
        }
        .distinctBy { it.file }
        .sortedBy { it.name.lowercase() }

    /** O nome do pacote no cabecalho STFS (0x411, UTF-16 big-endian), ou null se nao for um pacote do 360. */
    fun displayName(file: File): String? = try {
        RandomAccessFile(file, "r").use { f ->
            val head = ByteArray(0x500)
            f.readFully(head)
            if (!isPackage(head)) return null
            String(head, 0x411, 0x80, Charsets.UTF_16BE).substringBefore('\u0000').trim().ifEmpty { null }
        }
    } catch (_: Exception) {
        null
    }

    private fun isPackage(head: ByteArray): Boolean {
        val magic = String(head, 0, 4, Charsets.US_ASCII)
        return magic == "LIVE" || magic == "PIRS" || magic == "CON "
    }

    class NotAPackage(val fileName: String) : Exception(fileName)

    /**
     * Copia um pacote escolhido no seletor de arquivos para [dir]. Lanca [NotAPackage] se nao comecar por
     * LIVE/PIRS/CON (um .zip ou uma pasta extraida, por exemplo).
     */
    fun import(context: Context, uri: Uri, progress: (done: Long, total: Long, name: String) -> Unit): Package {
        val resolver = context.contentResolver
        var name = "dlc_${System.currentTimeMillis()}"
        var total = -1L
        resolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE), null, null, null)?.use { c ->
            if (c.moveToFirst()) {
                c.getString(0)?.let { name = it }
                if (!c.isNull(1)) total = c.getLong(1)
            }
        }
        name = name.replace('/', '_')
        dir.mkdirs()
        val tmp = File(dir, "$name.part")
        resolver.openInputStream(uri)?.use { input ->
            val head = ByteArray(4)
            var got = 0
            while (got < 4) {
                val n = input.read(head, got, 4 - got)
                if (n < 0) break
                got += n
            }
            if (got < 4 || !isPackage(head)) throw NotAPackage(name)
            tmp.outputStream().use { out ->
                out.write(head)
                val buf = ByteArray(1 shl 20)
                var done = 4L
                while (true) {
                    val n = input.read(buf)
                    if (n < 0) break
                    out.write(buf, 0, n)
                    done += n
                    progress(done, total, name)
                }
            }
        } ?: throw NotAPackage(name)
        val dest = File(dir, name)
        dest.delete()
        File(dir, name + MARKER).delete()  // um pacote novo com o mesmo nome volta a ser instalado
        File(installRoot, name).deleteRecursively()
        tmp.renameTo(dest)
        return Package(name, displayName(dest) ?: name, dest.length(), false)
    }

    /** Remove o pacote, a marca e o que foi instalado dele. */
    fun remove(file: String) {
        File(dir, file).delete()
        File(dir, file + MARKER).delete()
        File(installRoot, file).deleteRecursively()
        File(installRoot.parentFile, "Headers/00000002/$file.header").delete()
    }

    private const val MARKER = ".instalado"
}
