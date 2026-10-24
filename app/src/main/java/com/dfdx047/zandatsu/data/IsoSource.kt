package com.dfdx047.zandatsu.data

import android.content.Context
import android.content.Intent
import android.net.Uri
import java.io.File
import java.io.FileInputStream

/**
 * Jogo lido direto da ISO, como o Xenia, sem extrair nada: a importacao so confere o default.xex (copiado, 27 MB) e
 * descriptografa o libgame. A ISO fica onde o jogador a tem, com a permissao persistente do SAF; o processo :game
 * abre-a a cada arranque e passa o descritor ao nativo (ZANDATSU_ISO_FD, src/mgr_app.h), que a monta como disco
 * (DiscImageDevice, patch 0179 do SDK).
 *
 * A marca (game/.zandatsu-iso) guarda o uri e o tamanho. A importacao de uma pasta apaga-a: o jogo volta a ser a
 * pasta game/.
 */
object IsoSource {
    private lateinit var context: Context

    private val marker get() = File(AppPaths.gameDir, ".zandatsu-iso")

    fun init(ctx: Context) {
        context = ctx.applicationContext
    }

    /** A ISO importada (uri e tamanho), ou null: o jogo esta na pasta game/. */
    fun current(): Pair<Uri, Long>? = try {
        val lines = marker.readLines()
        Uri.parse(lines[0]) to lines[1].toLong()
    } catch (_: Exception) {
        null
    }

    /** Guarda a ISO (com permissao persistente). Falso se o fornecedor nao a da: entao extrai-se. */
    fun save(uri: Uri, size: Long): Boolean {
        try {
            context.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        } catch (e: SecurityException) {
            AppLog.w("import", "ISO sem permissao persistente: $uri", e)
            return false
        }
        marker.parentFile?.mkdirs()
        marker.writeText("$uri\n$size\n")
        return true
    }

    fun clear() {
        marker.delete()
    }

    /**
     * Com a ISO ativa: o que sobrou de importacoes antigas extraidas em game/ (tudo menos o default.xex e as marcas
     * com ponto). O jogo ja nao le nada disso.
     */
    private fun staleFiles(): List<File> {
        if (current() == null) return emptyList()
        return AppPaths.gameDir.listFiles()?.filter { it.name != "default.xex" && !it.name.startsWith(".") }.orEmpty()
    }

    /** Bytes dos ficheiros extraidos antigos (0 sem ISO ativa ou sem sobras). */
    fun staleBytes(): Long = staleFiles().sumOf { f -> f.walkTopDown().filter { it.isFile }.sumOf { it.length() } }

    /** Apaga os ficheiros extraidos antigos; devolve os bytes libertados. */
    fun deleteStale(): Long {
        val bytes = staleBytes()
        staleFiles().forEach { it.deleteRecursively() }
        AppLog.i("import", "ficheiros extraidos antigos apagados: ${bytes / 1_000_000} MB")
        return bytes
    }

    /** Os caminhos da ISO (em minusculas), ou null se ela ja nao abre (apagada, movida, sem permissao). */
    fun files(): List<String>? {
        val (uri, size) = current() ?: return null
        return try {
            context.contentResolver.openFileDescriptor(uri, "r")?.use { pfd ->
                if (pfd.statSize != size) return null
                FileInputStream(pfd.fileDescriptor).channel.use { ch ->
                    Xdvdfs.open(ch)?.files?.map { it.path.lowercase() }
                }
            }
        } catch (_: Exception) {
            null
        }
    }

    /**
     * Processo :game, antes do nativo: o descritor da ISO, aberto ate o processo acabar (o nativo faz mmap dele), ou
     * -1 sem ISO importada ou se ela ja nao abre (fica a pasta game/).
     */
    @JvmStatic
    fun openFd(ctx: Context): Int {
        val (uri, size) = current() ?: return -1
        return try {
            val pfd = ctx.contentResolver.openFileDescriptor(uri, "r") ?: return -1
            if (pfd.statSize != size) {
                AppLog.w("game", "ISO com outro tamanho (${pfd.statSize}, esperado $size): $uri")
                pfd.close()
                return -1
            }
            val fd = pfd.detachFd()
            AppLog.i("game", "jogo lido da ISO (fd $fd): $uri")
            fd
        } catch (e: Exception) {
            AppLog.w("game", "ISO nao abre: $uri", e)
            -1
        }
    }
}
