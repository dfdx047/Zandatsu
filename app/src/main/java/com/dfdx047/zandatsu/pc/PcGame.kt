package com.dfdx047.zandatsu.pc

import android.content.Context
import android.os.ParcelFileDescriptor
import android.system.Os
import android.system.OsConstants
import org.json.JSONObject
import java.io.DataInputStream
import java.io.File
import java.io.FileOutputStream
import java.io.RandomAccessFile
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.MessageDigest
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

/**
 * Port de PC do Metal Gear Rising: Revengeance (branch pc/main, codigo em pc/).
 *
 * O codigo do jogo (runtime + codigo liftado) vai so encriptado no APK (assets/libmgrpc.zdat, pc/tools/pack-libmgr.py),
 * no mesmo formato ZDT2 do libgame do 360. A chave e o SHA-256 da seccao .text do executavel do jogador: so quem tem o
 * jogo a obtem. A lib desencriptada vai para um memfd e o carregador nativo (libmgrlab.so) abre-a por fd: o codigo em
 * claro nunca toca o disco (o Odin 2 Mini de teste nem tem espaco interno para ele).
 */
object PcGame {
    private const val PREFS = "pc_game"
    const val EXE_NAME = "METAL GEAR RISING REVENGEANCE.exe"

    class PcException(msg: String) : Exception(msg)

    private fun prefs(c: Context) = c.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    /** Caminho do exe do jogador (pasta do jogo de PC com GameData ao lado). */
    fun exePath(c: Context): String = prefs(c).getString("exe", "").orEmpty()
    fun setExePath(c: Context, path: String) = prefs(c).edit().putString("exe", path).apply()

    /** "" = driver do sistema; senao a pasta de um driver instalado (aba Drivers). Nunca muda sozinho. */
    fun driverDir(c: Context): String = prefs(c).getString("driver", "").orEmpty()
    fun setDriverDir(c: Context, dir: String) = prefs(c).edit().putString("driver", dir).apply()

    /** Tamanho dos buffers da janela ("" = nativa), ex. "1280x720": resolucao livre, o compositor escala. */
    fun renderSize(c: Context): String = prefs(c).getString("render", "").orEmpty()
    fun setRenderSize(c: Context, size: String) = prefs(c).edit().putString("render", size).apply()

    /**
     * Opcoes graficas do proprio jogo: Documents/MGR/SaveData/GraphicOption na pasta do jogo, 12 x uint32 LE, lido no
     * arranque. Formato descodificado em 19/11 (PCPorts gpu/README.md): 0 resolucao (indice na lista do jogo),
     * 1 filtro (0 off, 1 aniso 2x, 2 aniso 4x), 2 blur (0 on, 1 off), 3 sombra, 4 brilho, 6 janela (1 janela),
     * 7 MSAA (0 off, 1 x2), 8 zangeki, 9 zantime (0 low, 1 medium); 5, 10, 11 sem efeito visivel.
     */
    fun graphicOptionFile(exe: String) = File(File(exe).parentFile, "Documents/MGR/SaveData/GraphicOption")
    val GRAPHIC_DEFAULT = intArrayOf(2, 2, 0, 1, 5, 0, 1, 1, 1, 1, 0, 0)   // o que o jogo grava no padrao (MEDIUM)
    fun readGraphicOptions(exe: String): IntArray {
        val f = graphicOptionFile(exe)
        if (f.length() != 48L) return GRAPHIC_DEFAULT.copyOf()
        val b = ByteBuffer.wrap(f.readBytes()).order(ByteOrder.LITTLE_ENDIAN)
        return IntArray(12) { b.getInt(it * 4) }
    }
    fun writeGraphicOptions(exe: String, v: IntArray) {
        val f = graphicOptionFile(exe); f.parentFile?.mkdirs()
        val b = ByteBuffer.allocate(48).order(ByteOrder.LITTLE_ENDIAN); v.forEach { b.putInt(it) }
        f.writeBytes(b.array())
    }

    /** Limite de fps (MGR_FPS_CAP): 0 = o do jogo (60). */
    fun fpsCap(c: Context): Int = prefs(c).getInt("fps_cap", 0)
    fun setFpsCap(c: Context, v: Int) = prefs(c).edit().putInt("fps_cap", v).apply()
    /** Perfil escolhido ("" = ainda nenhum: o primeiro arranque usa o do SoC). */
    fun profile(c: Context): String = prefs(c).getString("profile", "").orEmpty()
    fun setProfile(c: Context, v: String) = prefs(c).edit().putString("profile", v).apply()

    /**
     * Perfis da launcher: GraphicOption do jogo + resolucao interna + limite de fps. Medidos no Odin 2 Mini com o
     * perfil sd480p (combate do prologo): FRACO 18,1 fps, MEDIO 16,9 (o combate esta preso na CPU).
     */
    class Profile(val id: String, val label: String, val render: String, val fps: Int, val opts: Map<Int, Int>)
    val PROFILES = listOf(
        Profile("fraco", "Fraco (SD665/480+)", "854x480", 30, mapOf(1 to 0, 2 to 1, 3 to 0, 7 to 0, 8 to 0, 9 to 0)),
        Profile("baixo", "Baixo", "960x540", 30, mapOf(1 to 1, 2 to 1, 3 to 0, 7 to 0, 8 to 0, 9 to 0)),
        Profile("medio", "Médio", "1280x720", 0, mapOf(1 to 1, 2 to 0, 3 to 1, 7 to 0, 8 to 1, 9 to 0)),
        Profile("alto", "Alto", "", 0, mapOf(1 to 2, 2 to 0, 3 to 1, 7 to 1, 8 to 1, 9 to 1)),
    )
    /** SoCs fracos (SD665, SD680, SD480+, Helio G-series...): o perfil de omissao e o FRACO. */
    fun defaultProfile(): String {
        val soc = (android.os.Build.SOC_MODEL ?: "").uppercase()
        val weak = listOf("SM6115", "SM6125", "SM6225", "SM4350", "SM4375", "SM4450", "MT67", "MT68", "UMS", "T6", "T7")
        return if (weak.any { soc.startsWith(it) } || Runtime.getRuntime().availableProcessors() <= 6) "fraco" else "medio"
    }
    /** Aplica um perfil: grava o GraphicOption do jogo e as opcoes da launcher. */
    fun applyProfile(c: Context, exe: String, id: String) {
        val p = PROFILES.firstOrNull { it.id == id } ?: return
        setProfile(c, id); setRenderSize(c, p.render); setFpsCap(c, p.fps)
        if (exe.isNotEmpty()) runCatching {
            val v = readGraphicOptions(exe); p.opts.forEach { (i, x) -> v[i] = x }; writeGraphicOptions(exe, v)
        }
    }

    fun available(c: Context): Boolean = try { c.assets.open("libmgrpc.json").close(); true } catch (_: Exception) { false }

    /** Procura a pasta do jogo nos cartoes/armazenamento (lab: /storage/<sd>/MGRPC). */
    fun findExe(): String? = File("/storage").listFiles().orEmpty().flatMap {
        listOf(File(it, "MGRPC/$EXE_NAME"), File(it, "MGR/$EXE_NAME"), File(it, "Games/MGR/$EXE_NAME"))
    }.plus(File("/sdcard/MGRPC/$EXE_NAME")).firstOrNull { it.isFile }?.absolutePath

    /** SHA-256 da seccao .text do PE (a mesma conta do pack-libmgr.py). */
    fun textKey(exe: File): ByteArray {
        RandomAccessFile(exe, "r").use { f ->
            fun u16(at: Long): Int { f.seek(at); return f.read() or (f.read() shl 8) }
            fun u32(at: Long): Long { f.seek(at); val b = ByteArray(4); f.readFully(b); return ByteBuffer.wrap(b).order(ByteOrder.LITTLE_ENDIAN).int.toLong() and 0xffffffffL }
            val pe = u32(0x3c)
            val sections = u16(pe + 6)
            val table = pe + 24 + u16(pe + 20)
            for (i in 0 until sections) {
                val at = table + 40L * i
                f.seek(at); val name = ByteArray(8); f.readFully(name)
                if (String(name).trimEnd('\u0000') != ".text") continue
                val size = u32(at + 16); val raw = u32(at + 20)
                val md = MessageDigest.getInstance("SHA-256")
                f.seek(raw); val buf = ByteArray(1 shl 20); var left = size
                while (left > 0) { val n = f.read(buf, 0, minOf(left, buf.size.toLong()).toInt()); if (n <= 0) break; md.update(buf, 0, n); left -= n }
                return md.digest()
            }
        }
        throw PcException("executavel sem seccao .text: ${exe.name}")
    }

    /**
     * Desencripta o libmgrpc.zdat para um memfd e devolve o descritor (int, fica aberto neste processo para o
     * android_dlopen_ext do carregador nativo).
     */
    fun decryptToMemfd(c: Context, exe: File, onProgress: (Long, Long) -> Unit = { _, _ -> }): Int {
        val meta = JSONObject(c.assets.open("libmgrpc.json").bufferedReader().readText())
        val key = textKey(exe)
        val sha = MessageDigest.getInstance("SHA-256").digest(key).joinToString("") { "%02x".format(it) }
        if (sha != meta.getString("key_sha256")) throw PcException("executavel nao suportado (esperado SHA-1 ${meta.getString("exe_sha1").take(12)})")
        val fd = Os.memfd_create("libmgrpc.so", 0)
        val keep = ParcelFileDescriptor.dup(fd).detachFd()   // FileOutputStream.close() closes fd; the dup stays open
        FileOutputStream(fd).buffered(1 shl 20).use { o ->
            c.assets.open("libmgrpc.zdat").use { raw ->
                val input = DataInputStream(raw.buffered(1 shl 20))
                val header = ByteArray(24).also { input.readFully(it) }
                val hb = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN)
                if (hb.getInt(0) != 0x3254445A) throw PcException("libmgrpc.zdat: formato")
                val chunk = hb.getInt(4); val size = hb.getLong(8)
                val nonce = ByteArray(12).also { System.arraycopy(header, 16, it, 0, 8) }
                val spec = SecretKeySpec(key, "AES"); val cipher = Cipher.getInstance("AES/GCM/NoPadding")
                val buf = ByteArray(chunk + chunk / 8 + 1024 + 16); val plain = ByteArray(chunk)
                val inflater = java.util.zip.Inflater()
                var done = 0L; var i = 0
                while (done < size) {
                    val n = minOf(chunk.toLong(), size - done).toInt()
                    val blockLen = Integer.reverseBytes(input.readInt())
                    if (blockLen < 16 || blockLen > buf.size) throw PcException("libmgrpc.zdat: bloco $i")
                    input.readFully(buf, 0, blockLen)
                    ByteBuffer.wrap(nonce, 8, 4).order(ByteOrder.BIG_ENDIAN).putInt(i)
                    cipher.init(Cipher.DECRYPT_MODE, spec, GCMParameterSpec(128, nonce)); cipher.updateAAD(header)
                    val dec = cipher.doFinal(buf, 0, blockLen)
                    inflater.reset(); inflater.setInput(dec)
                    var got = 0
                    while (got < n && !inflater.finished()) { val k = inflater.inflate(plain, got, n - got); if (k == 0 && inflater.needsInput()) break; got += k }
                    if (got != n) throw PcException("libmgrpc.zdat: pedaco $i")
                    o.write(plain, 0, n); done += n; i++; onProgress(done, size)
                }
                inflater.end()
            }
        }
        return keep
    }
}
