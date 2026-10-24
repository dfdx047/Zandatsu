package com.dfdx047.zandatsu.data

import android.content.Context
import org.json.JSONObject
import java.io.DataInputStream
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.MessageDigest
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

/**
 * O codigo do jogo (libgame.so: recompilado, hooks, Renut e shaders traduzidos) vem no APK so encriptado
 * (assets/libgame.zdat, tools/pack-libgame.py). A chave e o SHA-256 da imagem PE do default.xex do jogador, lida pelo
 * libmain.so (src/xex_image.cpp); o libgame.so desencriptado fica em getFilesDir() (privado da app) e o GameActivity
 * carrega-o de la. Feito no import e de novo quando o APK traz outro libgame (libgame.json "id").
 */
object GameLib {
    private const val ASSET = "libgame.zdat"
    private const val META = "libgame.json"

    fun file(context: Context) = File(context.filesDir, "libgame.so")
    private fun stamp(context: Context) = File(context.filesDir, "libgame.id")

    private class Meta(val titleId: String, val xexSha1: String, val keySha256: String, val id: String, val size: Long)

    private fun meta(context: Context): Meta = context.assets.open(META).use { s ->
        val j = JSONObject(s.readBytes().decodeToString())
        Meta(j.getString("title_id"), j.getString("xex_sha1"), j.getString("key_sha256"), j.getString("id"), j.getLong("size"))
    }

    /** O libgame.so desencriptado existe e e o deste APK. */
    fun ready(context: Context): Boolean = try {
        file(context).length() == meta(context).size && stamp(context).readText() == meta(context).id
    } catch (_: Exception) {
        false
    }

    @JvmStatic
    private external fun nativeXexImage(path: String, titleOut: IntArray, errorOut: Array<String?>): ByteArray?

    /**
     * Le o default.xex instalado, deriva a chave e desencripta o libgame.so para getFilesDir(). XEX de outra versao:
     * ImportException UNSUPPORTED_VERSION com o title ID e o SHA-1 esperados.
     */
    @JvmStatic
    @JvmOverloads
    fun install(context: Context, onProgress: (ImportProgress) -> Unit = {}) {
        val m = meta(context)
        if (ready(context)) return
        val xex = AppPaths.xexFile
        if (!xex.isFile) throw ImportException(ImportException.Reason.NO_XEX)
        System.loadLibrary("main")
        onProgress(ImportProgress.Reading("default.xex"))
        val title = IntArray(1)
        val error = arrayOfNulls<String>(1)
        val pe = nativeXexImage(xex.absolutePath, title, error)
        val sha256 = { b: ByteArray -> MessageDigest.getInstance("SHA-256").digest(b) }
        val key = pe?.let(sha256)
        if (key == null || sha256(key).hex() != m.keySha256) {
            val sha1 = try { GameFiles.sha1File(xex) } catch (_: Exception) { "-" }
            throw ImportException(
                ImportException.Reason.UNSUPPORTED_VERSION,
                "title ID %08X, SHA-1 %s%s; esperado title ID %s, SHA-1 %s".format(
                    title[0], sha1.take(12), error[0]?.let { " ($it)" } ?: "", m.titleId, m.xexSha1.take(12),
                ),
            )
        }

        val out = file(context)
        val part = File(out.parentFile, out.name + ".part")
        stamp(context).delete()
        try {
            context.assets.open(ASSET).use { raw ->
                val input = DataInputStream(raw.buffered(1 shl 20))
                val header = ByteArray(24).also { input.readFully(it) }
                val hb = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN)
                if (hb.getInt(0) != 0x3154445A) throw ImportException(ImportException.Reason.IO, "libgame.zdat")  // "ZDT1"
                val chunk = hb.getInt(4)
                val size = hb.getLong(8)
                val nonce = ByteArray(12).also { System.arraycopy(header, 16, it, 0, 8) }
                val spec = SecretKeySpec(key, "AES")
                val cipher = Cipher.getInstance("AES/GCM/NoPadding")
                val buf = ByteArray(chunk + 16)
                part.outputStream().buffered(1 shl 20).use { o ->
                    var done = 0L
                    var i = 0
                    while (done < size) {
                        val n = minOf(chunk.toLong(), size - done).toInt()
                        input.readFully(buf, 0, n + 16)
                        ByteBuffer.wrap(nonce, 8, 4).order(ByteOrder.BIG_ENDIAN).putInt(i)
                        cipher.init(Cipher.DECRYPT_MODE, spec, GCMParameterSpec(128, nonce))
                        cipher.updateAAD(header)
                        o.write(cipher.doFinal(buf, 0, n + 16))
                        done += n
                        i++
                        onProgress(ImportProgress.Unpacking(done, size))
                    }
                }
            }
            if (!part.renameTo(out)) throw ImportException(ImportException.Reason.IO, out.name)
            stamp(context).writeText(m.id)
        } catch (e: javax.crypto.AEADBadTagException) {
            throw ImportException(ImportException.Reason.UNSUPPORTED_VERSION, "libgame.zdat: ${e.message}")
        } finally {
            part.delete()
        }
        AppLog.i("import", "libgame.so pronto (${out.length() / (1 shl 20)} MB, id ${m.id}, title ${m.titleId})")
    }

    private fun ByteArray.hex() = joinToString("") { "%02x".format(it) }
}
