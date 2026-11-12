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
 *
 * O APK traz dois: libgame (ARMv8.2, atomicos LSE em linha) e libgame-v80 (ARMv8.0, para SoCs como o Snapdragon 665,
 * onde as instrucoes do 8.1 dao SIGILL). Escolhe-se pela CPU (HWCAP do kernel); o id de cada um e diferente, entao trocar
 * de variante refaz a copia. Para testar: ficheiro "libgame-variant" com v80 ou v82 na pasta do app (Android/data).
 */
object GameLib {
    private const val V82 = "libgame"
    private const val V80 = "libgame-v80"

    fun file(context: Context) = File(context.filesDir, "libgame.so")
    private fun stamp(context: Context) = File(context.filesDir, "libgame.id")

    private class Meta(val titleId: String, val xexSha1: String, val keySha256: String, val id: String, val size: Long)

    // HWCAP do aarch64 (asm/hwcap.h): atomicos LSE e SQRDMLAH (ARMv8.1), CRC32.
    private const val AT_HWCAP = 16L
    private const val HWCAP_CRC32 = 1L shl 7
    private const val HWCAP_ATOMICS = 1L shl 8
    private const val HWCAP_ASIMDRDM = 1L shl 12
    private const val HWCAP_V81 = HWCAP_CRC32 or HWCAP_ATOMICS or HWCAP_ASIMDRDM

    /** AT_HWCAP de /proc/self/auxv (pares u64 tipo/valor); se falhar, as "Features" do /proc/cpuinfo. */
    private fun hwcap(): Long {
        try {
            val b = ByteBuffer.wrap(File("/proc/self/auxv").readBytes()).order(ByteOrder.nativeOrder())
            while (b.remaining() >= 16) {
                val type = b.long
                val value = b.long
                if (type == AT_HWCAP) return value
                if (type == 0L) break
            }
        } catch (_: Exception) {
        }
        return try {
            val f = File("/proc/cpuinfo").readLines().firstOrNull { it.startsWith("Features") }
                ?.substringAfter(':')?.trim()?.split(Regex("\\s+"))?.toSet() ?: emptySet()
            (if ("crc32" in f) HWCAP_CRC32 else 0L) or (if ("atomics" in f) HWCAP_ATOMICS else 0L) or
                (if ("asimdrdm" in f) HWCAP_ASIMDRDM else 0L)
        } catch (_: Exception) {
            0L
        }
    }

    private var cachedVariant: String? = null

    /** "libgame" (CPU ARMv8.1+) ou "libgame-v80"; so ha o segundo se o APK o trouxer. */
    @JvmStatic
    fun variant(context: Context): String = cachedVariant ?: run {
        val forced = try {
            context.getExternalFilesDir(null)?.let { File(it, "libgame-variant") }?.takeIf { it.isFile }?.readText()?.trim()
        } catch (_: Exception) {
            null
        }
        val cap = hwcap()
        val v81 = cap and HWCAP_V81 == HWCAP_V81
        val hasV80 = try { context.assets.open("$V80.json").close(); true } catch (_: Exception) { false }
        val v = when {
            !hasV80 -> V82
            forced == "v80" -> V80
            forced == "v82" -> V82
            v81 -> V82
            else -> V80
        }
        AppLog.i("import", "CPU hwcap 0x%x (ARMv8.1 %s)%s: %s".format(
            cap, if (v81) "sim" else "nao", forced?.let { " forcado $it" } ?: "", v))
        cachedVariant = v
        v
    }

    private fun meta(context: Context): Meta = context.assets.open("${variant(context)}.json").use { s ->
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
            context.assets.open("${variant(context)}.zdat").use { raw ->
                val input = DataInputStream(raw.buffered(1 shl 20))
                val header = ByteArray(24).also { input.readFully(it) }
                val hb = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN)
                // "ZDT1": pedacos so encriptados; "ZDT2" (12/11): cada pedaco com o tamanho a frente, comprimido (zlib) e
                // depois encriptado (tools/pack-libgame.py).
                val magic = hb.getInt(0)
                val packed = when (magic) {
                    0x3154445A -> false  // "ZDT1"
                    0x3254445A -> true   // "ZDT2"
                    else -> throw ImportException(ImportException.Reason.IO, "libgame.zdat")
                }
                val chunk = hb.getInt(4)
                val size = hb.getLong(8)
                val nonce = ByteArray(12).also { System.arraycopy(header, 16, it, 0, 8) }
                val spec = SecretKeySpec(key, "AES")
                val cipher = Cipher.getInstance("AES/GCM/NoPadding")
                // Um bloco comprimido pode passar um pouco do pedaco (dados que nao comprimem): folga de 1/8 + 1 KiB.
                val buf = ByteArray(chunk + chunk / 8 + 1024 + 16)
                val plain = ByteArray(chunk)
                val inflater = java.util.zip.Inflater()
                part.outputStream().buffered(1 shl 20).use { o ->
                    var done = 0L
                    var i = 0
                    while (done < size) {
                        val n = minOf(chunk.toLong(), size - done).toInt()
                        val blockLen = if (packed) Integer.reverseBytes(input.readInt()) else n + 16
                        if (blockLen < 16 || blockLen > buf.size) throw ImportException(ImportException.Reason.IO, "libgame.zdat")
                        input.readFully(buf, 0, blockLen)
                        ByteBuffer.wrap(nonce, 8, 4).order(ByteOrder.BIG_ENDIAN).putInt(i)
                        cipher.init(Cipher.DECRYPT_MODE, spec, GCMParameterSpec(128, nonce))
                        cipher.updateAAD(header)
                        val dec = cipher.doFinal(buf, 0, blockLen)
                        if (packed) {
                            inflater.reset()
                            inflater.setInput(dec)
                            var got = 0
                            while (got < n && !inflater.finished()) {
                                val k = inflater.inflate(plain, got, n - got)
                                if (k == 0 && (inflater.needsInput() || inflater.needsDictionary())) break
                                got += k
                            }
                            if (got != n) throw ImportException(ImportException.Reason.IO, "libgame.zdat: pedaco $i")
                            o.write(plain, 0, n)
                        } else {
                            o.write(dec)
                        }
                        done += n
                        i++
                        onProgress(ImportProgress.Unpacking(done, size))
                    }
                }
                inflater.end()
            }
            if (!part.renameTo(out)) throw ImportException(ImportException.Reason.IO, out.name)
            stamp(context).writeText(m.id)
        } catch (e: javax.crypto.AEADBadTagException) {
            throw ImportException(ImportException.Reason.UNSUPPORTED_VERSION, "libgame.zdat: ${e.message}")
        } finally {
            part.delete()
        }
        AppLog.i("import", "libgame.so pronto (${variant(context)}, ${out.length() / (1 shl 20)} MB, id ${m.id}, title ${m.titleId})")
    }

    private fun ByteArray.hex() = joinToString("") { "%02x".format(it) }
}
