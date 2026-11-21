package com.dfdx047.zandatsu.pc

import android.content.Context
import android.net.Uri
import android.provider.DocumentsContract
import android.util.Log
import androidx.documentfile.provider.DocumentFile
import java.io.File
import java.io.FileOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Opcoes graficas do proprio jogo: Documents/MGR/SaveData/GraphicOption na pasta do jogo, 12 x uint32 LE, lido no
 * arranque. Formato descodificado em 19/11 (PCPorts gpu/README.md): 0 resolucao (indice na lista do jogo),
 * 1 filtro (0 off, 1 aniso 2x, 2 aniso 4x), 2 blur (0 on, 1 off), 3 sombra, 4 brilho, 6 janela (1 janela),
 * 7 MSAA (0 off, 1 x2), 8 zangeki, 9 zantime (0 low, 1 medium); 5, 10, 11 sem efeito visivel.
 *
 * 21/11: no Odin o ficheiro no cartao SD nao mudava desde 19/11 apesar das mudancas na launcher antiga (a escrita
 * falhava em silencio). Aqui as opcoes vivem nas preferencias da app e o ficheiro e gravado com verificacao (escreve,
 * le de volta, compara) ao mudar uma opcao e outra vez no JOGAR. Se a escrita por File falhar, tenta pelo SAF (a
 * pasta do jogo escolhida em "Dar acesso pelo sistema"). O resultado (caminho, hora, erro) fica gravado para o
 * separador Graficos.
 */
object GraphicOptions {
    private const val TAG = "MGRPC"
    private const val PREFS = "pc_graphic_option"
    val DEFAULT = intArrayOf(2, 2, 0, 1, 5, 0, 1, 1, 1, 1, 0, 0)   // o que o jogo grava no padrao (MEDIUM)

    private fun prefs(c: Context) = c.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    fun file(exe: String) = File(File(exe).parentFile, "Documents/MGR/SaveData/GraphicOption")

    /** Valores pedidos (preferencias); na primeira vez, os do ficheiro do jogo se existir, senao os do jogo. */
    fun values(c: Context): IntArray {
        prefs(c).getString("values", null)?.split(',')?.mapNotNull { it.trim().toIntOrNull() }?.takeIf { it.size == 12 }
            ?.let { return it.toIntArray() }
        val exe = PcGame.exePath(c)
        val fromFile = if (exe.isNotEmpty()) runCatching { readFile(file(exe)) }.getOrNull() else null
        return (fromFile ?: DEFAULT).copyOf()
    }

    private fun remember(c: Context, v: IntArray) = prefs(c).edit().putString("values", v.joinToString(",")).apply()

    fun readFile(f: File): IntArray? {
        if (f.length() != 48L) return null
        return decode(f.readBytes())
    }

    private fun decode(b: ByteArray): IntArray? {
        if (b.size != 48) return null
        val bb = ByteBuffer.wrap(b).order(ByteOrder.LITTLE_ENDIAN)
        return IntArray(12) { bb.getInt(it * 4) }
    }

    private fun encode(v: IntArray): ByteArray {
        val b = ByteBuffer.allocate(48).order(ByteOrder.LITTLE_ENDIAN); v.forEach { b.putInt(it) }
        return b.array()
    }

    /** Pasta do jogo dada pelo SAF (OPEN_DOCUMENT_TREE), ou null. */
    fun treeUri(c: Context): Uri? = prefs(c).getString("tree", null)?.let(Uri::parse)
    fun setTreeUri(c: Context, uri: Uri?) = prefs(c).edit().putString("tree", uri?.toString()).apply()

    /** Caminho aproximado da pasta do SAF ("primary:X" -> /storage/emulated/0/X, "6A55-7C3B:X" -> /storage/6A55-7C3B/X). */
    fun treePath(uri: Uri): String? = runCatching {
        val id = DocumentsContract.getTreeDocumentId(uri)
        val vol = id.substringBefore(':'); val rel = id.substringAfter(':', "")
        val root = if (vol == "primary") "/storage/emulated/0" else "/storage/$vol"
        if (rel.isEmpty()) root else "$root/$rel"
    }.getOrNull()

    class WriteResult(val ok: Boolean, val path: String, val via: String, val error: String?, val at: Long)

    /** Ultima gravacao (para o separador Graficos). */
    fun lastResult(c: Context): WriteResult? {
        val p = prefs(c)
        if (!p.contains("last_at")) return null
        return WriteResult(p.getBoolean("last_ok", false), p.getString("last_path", "").orEmpty(),
            p.getString("last_via", "").orEmpty(), p.getString("last_error", null), p.getLong("last_at", 0))
    }

    /** Guarda [v] nas preferencias e grava o GraphicOption (com verificacao). */
    fun save(c: Context, v: IntArray): WriteResult {
        remember(c, v)
        return write(c)
    }

    /** Grava os valores pedidos no GraphicOption do jogo, le de volta e compara. Chamado tambem no JOGAR. */
    fun write(c: Context): WriteResult {
        val exe = PcGame.exePath(c)
        val target = if (exe.isNotEmpty()) file(exe) else null
        val bytes = encode(values(c))
        val r = if (target == null) {
            WriteResult(false, "", "", c.getString(R.string.go_no_exe), System.currentTimeMillis())
        } else {
            val byFile = runCatching { writeFile(target, bytes) }
            if (byFile.isSuccess) {
                WriteResult(true, target.path, "file", null, System.currentTimeMillis())
            } else {
                val fileError = byFile.exceptionOrNull()?.let { it.message ?: it.toString() } ?: "?"
                Log.w(TAG, "GraphicOption por File falhou em ${target.path}: $fileError")
                val tree = treeUri(c)
                val bySaf = if (tree != null) runCatching { writeSaf(c, tree, target, bytes) } else null
                when {
                    bySaf?.isSuccess == true -> WriteResult(true, target.path, "saf", null, System.currentTimeMillis())
                    bySaf != null -> WriteResult(false, target.path, "saf", "$fileError; SAF: " +
                        (bySaf.exceptionOrNull()?.message ?: bySaf.exceptionOrNull().toString()), System.currentTimeMillis())
                    else -> WriteResult(false, target.path, "file", fileError, System.currentTimeMillis())
                }
            }
        }
        if (r.ok) Log.i(TAG, "GraphicOption gravado e verificado (${r.via}): ${r.path} = ${values(c).joinToString(",")}")
        else Log.w(TAG, "GraphicOption NAO gravado em ${r.path}: ${r.error}")
        prefs(c).edit().putBoolean("last_ok", r.ok).putString("last_path", r.path).putString("last_via", r.via)
            .putString("last_error", r.error).putLong("last_at", r.at).apply()
        return r
    }

    private fun writeFile(f: File, bytes: ByteArray) {
        f.parentFile?.let { if (!it.isDirectory && !it.mkdirs()) throw java.io.IOException("mkdirs falhou: ${it.path}") }
        FileOutputStream(f).use { o -> o.write(bytes); o.flush(); o.fd.sync() }
        val back = f.readBytes()
        if (!back.contentEquals(bytes)) throw java.io.IOException("verificacao: lidos ${back.size} bytes diferentes do gravado")
    }

    /** Pelo SAF: a arvore tem de ser a pasta do jogo (a do .exe). */
    private fun writeSaf(c: Context, tree: Uri, target: File, bytes: ByteArray) {
        val root = DocumentFile.fromTreeUri(c, tree) ?: throw java.io.IOException("SAF: pasta invalida")
        val treeDir = treePath(tree)
        val gameDir = File(PcGame.exePath(c)).parentFile?.path
        if (treeDir != null && gameDir != null && treeDir.trimEnd('/') != gameDir.trimEnd('/') &&
            treeDir.replace("/storage/emulated/0", "/sdcard") != gameDir.trimEnd('/')) {
            throw java.io.IOException("SAF: a pasta dada ($treeDir) nao e a do jogo ($gameDir)")
        }
        var dir = root
        for (name in listOf("Documents", "MGR", "SaveData")) {
            dir = dir.findFile(name)?.takeIf { it.isDirectory } ?: dir.createDirectory(name)
                ?: throw java.io.IOException("SAF: nao criei $name")
        }
        val doc = dir.findFile("GraphicOption") ?: dir.createFile("application/octet-stream", "GraphicOption")
            ?: throw java.io.IOException("SAF: nao criei GraphicOption")
        if (doc.name != "GraphicOption") throw java.io.IOException("SAF: o ficheiro ficou com o nome ${doc.name}")
        c.contentResolver.openOutputStream(doc.uri, "wt")?.use { it.write(bytes); it.flush() }
            ?: throw java.io.IOException("SAF: openOutputStream")
        val back = c.contentResolver.openInputStream(doc.uri)?.use { it.readBytes() } ?: ByteArray(0)
        if (!back.contentEquals(bytes)) throw java.io.IOException("SAF: verificacao falhou (${back.size} bytes)")
        // O que o jogo vai ler e o caminho por File: confirma tambem por ai, se der para ler.
        runCatching { target.readBytes() }.getOrNull()?.let {
            if (!it.contentEquals(bytes)) throw java.io.IOException("SAF gravou mas ${target.path} continua diferente")
        }
    }
}
