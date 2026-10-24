package com.dfdx047.zandatsu.data

import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import java.io.File
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.channels.FileChannel
import java.security.MessageDigest

/**
 * Arquivos do jogo: o disco "Metal Gear Rising - Revengeance (USA, Europe)" (Redump). So
 * esse default.xex funciona, porque o codigo foi recompilado a partir dele (o SHA-1 esta
 * fixado em tools/versions.env).
 */
object BaseRom {
    const val XEX_SIZE = 7_446_528L
    const val XEX_SHA1_US = "2dffd4d46e727d4a64c331d54251c4d9a0a26edf"
    /** Title ID do Metal Gear Rising: Revengeance e media ID do disco USA/Europa. */
    const val TITLE_ID = 0x4B4E080AL
    const val MEDIA_ID_US = 0x6CC18291L

    /** Caminho relativo a pasta do jogo. */
    data class Entry(val path: String, val required: Boolean, val directory: Boolean = false)

    val FILES = listOf(
        Entry("media/data000.cpk", true),
        Entry("media/data001.cpk", true),
        Entry("media/sound/stream.cpk", true),
        Entry("media/sound/streamEnglish(US).cpk", true),
        Entry("media/sound/Init.bnk", true),
        Entry("media/movie", true, directory = true),
        Entry("media/movie_ui", false, directory = true),
    )
}

enum class FileStatus { OK, MISSING, DIFFERENT, WRONG_SIZE }

data class FileCheck(val name: String, val required: Boolean, val status: FileStatus)

/** Estado da instalacao do jogo na pasta da app. */
data class GameInstall(val checks: List<FileCheck>) {
    val xex: FileCheck? get() = checks.firstOrNull { it.name == "default.xex" }
    /**
     * Bloqueia o JOGAR: obrigatorio em falta ou executavel que nao e o do disco suportado.
     * Com outro default.xex o jogo fecha logo ao abrir ("No function registered at ...").
     */
    val playable: Boolean
        get() = checks.none { c ->
            (c.required && c.status == FileStatus.MISSING) ||
                (c.name == "default.xex" && c.status != FileStatus.OK)
        }
    val installed: Boolean get() = xex?.status != FileStatus.MISSING
}

sealed class ImportProgress {
    data class Copying(val file: String, val done: Long, val total: Long, val bytesPerSecond: Long = 0) : ImportProgress()
    data class Hashing(val file: String, val done: Long, val total: Long) : ImportProgress()
    data class Reading(val file: String) : ImportProgress()
    /** O libgame.so a ser desencriptado (GameLib). */
    data class Unpacking(val done: Long, val total: Long) : ImportProgress()
}

class ImportException(val reason: Reason, val detail: String = "") : Exception(detail) {
    enum class Reason {
        NO_XEX, NO_DATA_FOLDER, WRONG_REGION, BAD_XEX, NOT_A_DISC, MISSING_REQUIRED, NO_SPACE, IO,
        /** Outro jogo (title ID diferente). */
        OTHER_GAME,
        /** Metal Gear Rising, mas o executavel foi alterado (mesmo disco, SHA-1 diferente). */
        MODIFIED_XEX,
        /** A imagem acaba antes dos ficheiros (download ou copia incompleta). */
        TRUNCATED,
        /** Um ficheiro de dados copiado nao tem o formato esperado (CPK danificado). */
        BAD_DATA,
        /** O default.xex nao abre o libgame.so deste APK (outra versao do jogo): detalhe com o esperado. */
        UNSUPPORTED_VERSION,
    }
}

object GameFiles {

    /** Lista do que foi instalado (caminho e tamanho), para o Verificar notar ficheiros cortados. */
    private val manifestFile get() = File(AppPaths.gameDir, ".swordman-install")

    /**
     * Le o title ID e o media ID do cabecalho XEX2 (opcional 0x00040006, "execution info").
     * O cabecalho nao e cifrado. Devolve null se nao for um XEX2.
     */
    fun xexIds(header: ByteArray): Pair<Long, Long>? {
        if (header.size < 0x18 || String(header, 0, 4, Charsets.US_ASCII) != "XEX2") return null
        val b = ByteBuffer.wrap(header).order(ByteOrder.BIG_ENDIAN)
        val count = b.getInt(0x14)
        if (count < 0 || count > 64) return null
        for (i in 0 until count) {
            val at = 0x18 + i * 8
            if (at + 8 > header.size) return null
            val key = b.getInt(at)
            val value = b.getInt(at + 4)
            if (key == 0x00040006) {
                if (value < 0 || value + 16 > header.size) return null
                val media = b.getInt(value).toLong() and 0xFFFFFFFFL
                val title = b.getInt(value + 12).toLong() and 0xFFFFFFFFL
                return title to media
            }
        }
        return null
    }

    /** Explica um default.xex que nao e o USA: outro jogo, outra versao ou alterado. */
    private fun rejectXex(header: ByteArray, hash: String): Nothing {
        val ids = xexIds(header)
        when {
            ids == null -> throw ImportException(ImportException.Reason.BAD_XEX, hash.take(12))
            ids.first != BaseRom.TITLE_ID ->
                throw ImportException(ImportException.Reason.OTHER_GAME, "%08X".format(ids.first))
            else -> throw ImportException(
                ImportException.Reason.UNSUPPORTED_VERSION,
                "title ID %08X, media %08X, SHA-1 %s; esperado title ID %08X, media %08X, SHA-1 %s".format(
                    ids.first, ids.second, hash.take(12), BaseRom.TITLE_ID, BaseRom.MEDIA_ID_US, BaseRom.XEX_SHA1_US.take(12),
                ),
            )
        }
    }

    private fun writeManifest(entries: List<Pair<String, Long>>) {
        try {
            manifestFile.writeText(entries.joinToString("\n") { (p, n) -> "$n\t$p" })
        } catch (_: Exception) {
        }
    }

    /** Ficheiros instalados com o tamanho errado (ou em falta), segundo a lista da instalacao. */
    private fun manifestProblems(): List<String> {
        val f = manifestFile
        if (!f.isFile) return emptyList()
        return try {
            f.readLines().mapNotNull { line ->
                val tab = line.indexOf('\t')
                if (tab <= 0) return@mapNotNull null
                val size = line.substring(0, tab).toLongOrNull() ?: return@mapNotNull null
                val path = line.substring(tab + 1)
                val target = File(AppPaths.gameDir, path)
                if (!target.isFile || target.length() != size) path else null
            }
        } catch (_: Exception) {
            emptyList()
        }
    }

    /** Os .cpk (CRI) comecam por "CPK ": um que nao comece esta danificado. */
    private fun cpkLooksOk(file: File): Boolean = try {
        java.io.FileInputStream(file).use { input ->
            val m = ByteArray(4)
            input.read(m) == 4 && String(m, Charsets.US_ASCII) == "CPK "
        }
    } catch (_: Exception) {
        false
    }

    /**
     * Copia [length] bytes de [src] (a partir de [offset]) para [dst] pelo kernel
     * (transferTo / sendfile, sem passar pela memoria da app), em blocos de 64 MB para o
     * progresso; cai para uma copia com buffer se o canal nao suportar.
     */
    private fun copyRange(src: FileChannel, offset: Long, length: Long, dst: FileChannel, onBytes: (Long) -> Unit) {
        var pos = offset
        var left = length
        val chunk = 64L shl 20
        var fallback = false
        while (left > 0 && !fallback) {
            val n = try {
                src.transferTo(pos, minOf(left, chunk), dst)
            } catch (_: java.io.IOException) {
                0L
            }
            if (n <= 0) {
                fallback = true
            } else {
                pos += n
                left -= n
                onBytes(n)
            }
        }
        if (left > 0) {
            val buf = ByteBuffer.allocateDirect(8 shl 20)
            while (left > 0) {
                buf.clear()
                buf.limit(minOf(left, buf.capacity().toLong()).toInt())
                val n = src.read(buf, pos)
                if (n <= 0) throw ImportException(ImportException.Reason.TRUNCATED, "")
                buf.flip()
                while (buf.hasRemaining()) dst.write(buf)
                pos += n
                left -= n
                onBytes(n.toLong())
            }
        }
    }

    /** Progresso com velocidade media (para o tempo que falta). */
    private class Meter(val total: Long, val onProgress: (ImportProgress) -> Unit) {
        private val start = System.nanoTime()
        var done = 0L
        private var last = 0L
        fun add(file: String, n: Long) {
            done += n
            if (done - last >= (8L shl 20) || done >= total) {
                last = done
                val secs = (System.nanoTime() - start) / 1e9
                val rate = if (secs > 0.2) (done / secs).toLong() else 0L
                onProgress(ImportProgress.Copying(file, done, total, rate))
            }
        }
    }

    fun sha1File(f: File): String = sha1(f.inputStream(), f.length()) { _, _ -> }

    private fun sha1(input: InputStream, total: Long, onProgress: (Long, Long) -> Unit): String {
        val md = MessageDigest.getInstance("SHA-1")
        val buf = ByteArray(1 shl 20)
        var done = 0L
        input.use {
            while (true) {
                val n = it.read(buf)
                if (n < 0) break
                md.update(buf, 0, n)
                done += n
                onProgress(done, total)
            }
        }
        return md.digest().joinToString("") { b -> "%02x".format(b) }
    }

    /** Verifica o que esta na pasta da app (o .xex por SHA-1, o resto por presenca). */
    fun check(onProgress: (ImportProgress) -> Unit = {}): GameInstall {
        val list = ArrayList<FileCheck>()
        val xex = AppPaths.xexFile
        list += FileCheck(
            "default.xex", true,
            when {
                !xex.isFile -> FileStatus.MISSING
                xex.length() != BaseRom.XEX_SIZE -> FileStatus.WRONG_SIZE
                else -> {
                    val hash = sha1(xex.inputStream(), xex.length()) { d, t ->
                        onProgress(ImportProgress.Hashing(xex.name, d, t))
                    }
                    if (hash == BaseRom.XEX_SHA1_US) FileStatus.OK else FileStatus.DIFFERENT
                }
            },
        )
        val cut = manifestProblems()
        for (e in BaseRom.FILES) {
            val f = File(AppPaths.gameDir, e.path)
            val present = if (e.directory) f.isDirectory && (f.list()?.isNotEmpty() == true) else f.isFile
            val damaged = cut.any { p ->
                if (e.directory) p.lowercase().startsWith(e.path.lowercase() + "/") else p.equals(e.path, true)
            } || (present && !e.directory && e.path.endsWith(".CPK", true) && !cpkLooksOk(f))
            list += FileCheck(
                e.path, e.required,
                when {
                    !present -> FileStatus.MISSING
                    damaged -> FileStatus.WRONG_SIZE
                    else -> FileStatus.OK
                },
            )
        }
        return GameInstall(list)
    }

    private fun DocumentFile.child(name: String): DocumentFile? =
        listFiles().firstOrNull { it.name.equals(name, ignoreCase = true) }

    /**
     * Importa o jogo de uma pasta ja extraida (SAF): a pasta com o default.xex e a
     * media/ (como sai do extract-xiso), ou uma pasta acima com game/ dentro. Copia tudo
     * para a pasta do app, o default.xex por ultimo (e ele que marca a instalacao como
     * completa).
     */
    fun importFolder(context: Context, tree: Uri, onProgress: (ImportProgress) -> Unit) {
        val root = DocumentFile.fromTreeUri(context, tree)
            ?: throw ImportException(ImportException.Reason.IO, "pasta invalida")
        val base = root.child("game")?.takeIf { it.isDirectory } ?: root
        val xexDoc = base.child("default.xex") ?: throw ImportException(ImportException.Reason.NO_XEX)
        val dataDoc = base.child("media")?.takeIf { it.isDirectory }
            ?: throw ImportException(ImportException.Reason.NO_DATA_FOLDER)

        if (xexDoc.length() != BaseRom.XEX_SIZE) {
            throw ImportException(ImportException.Reason.BAD_XEX, xexDoc.name ?: "")
        }
        val resolver = context.contentResolver
        val hash = sha1(resolver.openInputStream(xexDoc.uri)!!, xexDoc.length()) { d, t ->
            onProgress(ImportProgress.Hashing("default.xex", d, t))
        }
        if (hash != BaseRom.XEX_SHA1_US) {
            val header = resolver.openInputStream(xexDoc.uri)!!.use { input ->
                val h = ByteArray(64 * 1024)
                var n = 0
                while (n < h.size) {
                    val r = input.read(h, n, h.size - n)
                    if (r < 0) break
                    n += r
                }
                h.copyOf(n)
            }
            rejectXex(header, hash)
        }

        // Lista recursiva da media/ (o resto do disco nao e lido pelo jogo).
        val files = ArrayList<Pair<DocumentFile, String>>()
        fun walk(dir: DocumentFile, rel: String) {
            for (c in dir.listFiles()) {
                val name = c.name ?: continue
                if (c.isDirectory) walk(c, "$rel/$name") else files += c to "$rel/$name"
            }
        }
        onProgress(ImportProgress.Reading("media"))
        walk(dataDoc, "media")
        requireFiles(files.map { it.second })

        val total = xexDoc.length() + files.sumOf { it.first.length() }
        // So o que vai ser copiado: os ficheiros que ja existem com o mesmo tamanho saltam (24/10: reimportar
        // pedia o jogo inteiro de espaco livre).
        ensureSpace(xexDoc.length() + files.filterNot { (d, rel) -> sameSize(rel, d.length()) }.sumOf { it.first.length() })
        val meter = Meter(total, onProgress)
        val buf = ByteArray(1 shl 20)
        fun copy(doc: DocumentFile, rel: String) {
            val target = File(AppPaths.gameDir, rel)
            if (target.isFile && target.length() == doc.length() && rel != "default.xex") {
                meter.add(rel, doc.length())
                return
            }
            target.parentFile?.mkdirs()
            val tmp = File(target.parentFile, target.name + ".part")
            try {
                val fd = try {
                    resolver.openFileDescriptor(doc.uri, "r")
                } catch (_: Exception) {
                    null
                }
                if (fd != null) {
                    // Copia pelo kernel (muito mais rapida que por streams do SAF).
                    fd.use { pf ->
                        java.io.FileInputStream(pf.fileDescriptor).channel.use { src ->
                            java.io.FileOutputStream(tmp).use { out ->
                                copyRange(src, 0, doc.length(), out.channel) { n -> meter.add(rel, n) }
                            }
                        }
                    }
                } else {
                    resolver.openInputStream(doc.uri)!!.use { input ->
                        tmp.outputStream().use { out ->
                            while (true) {
                                val n = input.read(buf)
                                if (n < 0) break
                                out.write(buf, 0, n)
                                meter.add(rel, n.toLong())
                            }
                        }
                    }
                }
                if (target.exists()) target.delete()
                if (!tmp.renameTo(target)) throw ImportException(ImportException.Reason.IO, rel)
            } catch (e: ImportException) {
                tmp.delete()
                throw e
            } catch (e: Exception) {
                tmp.delete()
                throw ImportException(ImportException.Reason.IO, "$rel: ${e.message}")
            }
        }
        AppPaths.xexFile.delete()
        for ((doc, rel) in files) copy(doc, rel)
        copy(xexDoc, "default.xex")
        writeManifest(files.map { (d, rel) -> rel to d.length() } + ("default.xex" to xexDoc.length()))
        onProgress(ImportProgress.Copying("", total, total))
        GameLib.install(context, onProgress)
    }

    /**
     * Importa direto da imagem do disco (.iso do Xbox 360, XDVDFS): confere o SHA-1 do
     * default.xex antes de copiar e copia o default.xex e a media/ (o resto do disco -
     * $SystemUpdate, AvatarAssetPack, nxeart - nao e usado), o default.xex por ultimo.
     */
    /** O ficheiro do jogo ja esta no app com este tamanho (o import salta-o). */
    private fun sameSize(rel: String, size: Long): Boolean =
        File(AppPaths.gameDir, rel).let { it.isFile && it.length() == size }

    fun importIso(context: Context, uri: Uri, onProgress: (ImportProgress) -> Unit) {
        val pfd = context.contentResolver.openFileDescriptor(uri, "r")
            ?: throw ImportException(ImportException.Reason.IO, "iso")
        pfd.use {
            java.io.FileInputStream(it.fileDescriptor).channel.use { ch ->
                onProgress(ImportProgress.Reading("ISO"))
                val disc = Xdvdfs.open(ch) ?: throw ImportException(ImportException.Reason.NOT_A_DISC)
                val xex = disc.files.firstOrNull { f -> f.path.equals("default.xex", true) }
                    ?: throw ImportException(ImportException.Reason.NO_XEX)
                if (xex.size != BaseRom.XEX_SIZE) {
                    // Outro jogo ou outra versao: o cabecalho diz qual.
                    val hb = ByteBuffer.allocate(minOf(xex.size, 65536L).toInt())
                    if (xex.offset < ch.size()) ch.read(hb, xex.offset)
                    rejectXex(hb.array().copyOf(hb.position()), "size ${xex.size}")
                }
                // Imagem cortada (download incompleto): os ficheiros passam do fim.
                val discSize = ch.size()
                disc.files.firstOrNull { f -> f.offset + f.size > discSize }?.let { f ->
                    throw ImportException(
                        ImportException.Reason.TRUNCATED,
                        "%s, %.2f / %.2f GB".format(f.path, discSize / 1e9, (f.offset + f.size) / 1e9),
                    )
                }
                val md = MessageDigest.getInstance("SHA-1")
                val header = java.io.ByteArrayOutputStream()
                disc.read(ch, xex) { b, n ->
                    md.update(b, 0, n)
                    if (header.size() < 64 * 1024) header.write(b, 0, minOf(n, 64 * 1024 - header.size()))
                }
                val hash = md.digest().joinToString("") { b -> "%02x".format(b) }
                if (hash != BaseRom.XEX_SHA1_US) rejectXex(header.toByteArray(), hash)

                // So a media/ (por ordem do disco: leitura sequencial) e o default.xex por ultimo.
                val files = disc.files.filter { f ->
                    f.path.startsWith("media/", ignoreCase = true) && f !== xex
                }.sortedBy { f -> f.offset } + xex
                requireFiles(files.map { f -> f.path })
                val total = files.sumOf { f -> f.size }
                ensureSpace(xex.size + files.filter { f -> f !== xex && !sameSize(f.path, f.size) }.sumOf { f -> f.size })
                AppPaths.xexFile.delete()
                val meter = Meter(total, onProgress)
                for (f in files) {
                    val target = File(AppPaths.gameDir, f.path)
                    if (f !== xex && target.isFile && target.length() == f.size) {
                        meter.add(f.path, f.size)
                        continue
                    }
                    target.parentFile?.mkdirs()
                    val tmp = File(target.parentFile, target.name + ".part")
                    try {
                        java.io.FileOutputStream(tmp).use { out ->
                            copyRange(ch, f.offset, f.size, out.channel) { n -> meter.add(f.path, n) }
                        }
                        if (target.exists()) target.delete()
                        if (!tmp.renameTo(target)) throw ImportException(ImportException.Reason.IO, f.path)
                    } catch (e: ImportException) {
                        tmp.delete()
                        throw e
                    } catch (e: Exception) {
                        tmp.delete()
                        throw ImportException(ImportException.Reason.IO, "${f.path}: ${e.message}")
                    }
                }
                // Os .cpk copiados tem de comecar pela assinatura (disco danificado = "CPK " em falta).
                files.filter { f -> f.path.endsWith(".cpk", ignoreCase = true) }.forEach { f ->
                    if (!cpkLooksOk(File(AppPaths.gameDir, f.path))) {
                        throw ImportException(ImportException.Reason.BAD_DATA, f.path)
                    }
                }
                writeManifest(files.map { f -> f.path to f.size })
                onProgress(ImportProgress.Copying("", total, total))
            }
        }
        GameLib.install(context, onProgress)
    }

    private fun requireFiles(paths: List<String>) {
        val lower = paths.map { it.lowercase() }
        val missing = BaseRom.FILES.filter { e ->
            e.required && lower.none { p ->
                if (e.directory) p.startsWith(e.path.lowercase() + "/") else p == e.path.lowercase()
            }
        }.map { it.path }
        if (missing.isNotEmpty()) {
            throw ImportException(ImportException.Reason.MISSING_REQUIRED, missing.joinToString())
        }
    }

    private fun ensureSpace(total: Long) {
        AppPaths.gameDir.mkdirs()
        val free = AppPaths.gameDir.usableSpace
        if (free < total + (64L shl 20)) {
            throw ImportException(
                ImportException.Reason.NO_SPACE,
                "%.1f GB / %.1f GB".format(total / 1e9, free / 1e9),
            )
        }
    }

    fun installedSizeBytes(): Long =
        AppPaths.gameDir.walkTopDown().filter { it.isFile }.sumOf { it.length() }
}

/**
 * Leitor minimo do sistema de ficheiros dos discos do Xbox 360 (XDVDFS / GDFX), igual
 * ao DiscImageDevice do SDK: o setor 32 da particao do jogo tem "MICROSOFT*XBOX*MEDIA",
 * a raiz e uma arvore binaria de entradas (esquerda/direita em unidades de 4 bytes).
 * Tudo little-endian, setores de 2 KB.
 */
class Xdvdfs private constructor(val files: List<FileEntry>) {

    data class FileEntry(val path: String, val offset: Long, val size: Long)

    fun read(ch: FileChannel, f: FileEntry, sink: (ByteArray, Int) -> Unit) {
        val bytes = ByteArray(1 shl 20)
        val buf = ByteBuffer.wrap(bytes)
        var pos = f.offset
        var left = f.size
        while (left > 0) {
            buf.clear()
            buf.limit(minOf(left, bytes.size.toLong()).toInt())
            val n = ch.read(buf, pos)
            if (n <= 0) throw ImportException(ImportException.Reason.IO, "fim do disco em ${f.path}")
            sink(bytes, n)
            pos += n
            left -= n
        }
    }

    companion object {
        private const val SECTOR = 2048L
        // Particao do jogo em bytes: xiso, XGD2 (Redump da maioria dos jogos), XGD3, XGD1.
        private val OFFSETS = longArrayOf(0x00000000, 0x0FD90000, 0x02080000, 0x18300000)
        private const val MAGIC = "MICROSOFT*XBOX*MEDIA"
        private const val ATTR_DIRECTORY = 0x10

        private fun readAt(ch: FileChannel, pos: Long, size: Int): ByteBuffer? {
            if (pos < 0 || pos + size > ch.size()) return null
            val b = ByteBuffer.allocate(size).order(ByteOrder.LITTLE_ENDIAN)
            while (b.hasRemaining()) {
                if (ch.read(b, pos + b.position()) <= 0) return null
            }
            b.flip()
            return b
        }

        fun open(ch: FileChannel): Xdvdfs? {
            var gameOffset = -1L
            for (o in OFFSETS) {
                val m = readAt(ch, o + 32 * SECTOR, 20) ?: continue
                val bytes = ByteArray(20).also { m.get(it) }
                if (String(bytes, Charsets.US_ASCII) == MAGIC) {
                    gameOffset = o
                    break
                }
            }
            if (gameOffset < 0) return null
            val fs = readAt(ch, gameOffset + 32 * SECTOR, 28) ?: return null
            val rootSector = fs.getInt(20).toLong() and 0xFFFFFFFFL
            val rootSize = fs.getInt(24).toLong() and 0xFFFFFFFFL
            if (rootSize < 13 || rootSize > (32L shl 20)) return null
            val out = ArrayList<FileEntry>()
            val ok = readDir(ch, gameOffset, gameOffset + rootSector * SECTOR, rootSize, "", out, 0)
            return if (ok) Xdvdfs(out) else null
        }

        private fun readDir(
            ch: FileChannel, gameOffset: Long, dirOffset: Long, dirSize: Long,
            prefix: String, out: MutableList<FileEntry>, depth: Int,
        ): Boolean {
            if (depth > 32) return false
            val dir = readAt(ch, dirOffset, dirSize.toInt()) ?: return false
            val visited = HashSet<Int>()
            fun entry(ordinal: Int): Boolean {
                val p = ordinal * 4
                if (!visited.add(p) || p + 14 > dir.limit()) return false
                val left = dir.getShort(p).toInt() and 0xFFFF
                val right = dir.getShort(p + 2).toInt() and 0xFFFF
                val sector = dir.getInt(p + 4).toLong() and 0xFFFFFFFFL
                val length = dir.getInt(p + 8).toLong() and 0xFFFFFFFFL
                val attributes = dir.get(p + 12).toInt() and 0xFF
                val nameLength = dir.get(p + 13).toInt() and 0xFF
                if (p + 14 + nameLength > dir.limit()) return false
                if (left != 0 && !entry(left)) return false
                val nameBytes = ByteArray(nameLength)
                for (i in 0 until nameLength) nameBytes[i] = dir.get(p + 14 + i)
                val name = String(nameBytes, Charsets.ISO_8859_1)
                val path = if (prefix.isEmpty()) name else "$prefix/$name"
                if (attributes and ATTR_DIRECTORY != 0) {
                    if (length != 0L &&
                        !readDir(ch, gameOffset, gameOffset + sector * SECTOR, length, path, out, depth + 1)
                    ) return false
                } else {
                    out += FileEntry(path, gameOffset + sector * SECTOR, length)
                }
                return right == 0 || entry(right)
            }
            return entry(0)
        }
    }
}
