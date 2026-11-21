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
import kotlin.math.roundToInt

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

    /**
     * Resolucao interna em % da nativa do ecra (25..100, passos de 5): os buffers da janela ficam desse tamanho e o
     * compositor (ou o FSR/CAS do DXVK) escala. 100 = nativa (MGR_RENDER_SIZE vazio).
     */
    fun renderScale(c: Context): Int = prefs(c).getInt("render_scale", 100)
    fun setRenderScale(c: Context, v: Int) = prefs(c).edit().putInt("render_scale", v.coerceIn(SCALE_MIN, 100)).apply()
    const val SCALE_MIN = 25
    const val SCALE_STEP = 5

    /** Tamanho nativo do ecra em paisagem (largura >= altura), pelo modo atual da tela. */
    fun nativeSize(c: Context): Pair<Int, Int> {
        val wm = c.getSystemService(android.view.WindowManager::class.java)
        val b = wm.maximumWindowMetrics.bounds
        // Context.getDisplay() so existe em contextos visuais (activity); fora disso fica o tamanho da janela maxima.
        val mode = runCatching { c.display?.mode }.getOrNull()
        val w = maxOf(mode?.physicalWidth ?: b.width(), mode?.physicalHeight ?: b.height())
        val h = minOf(mode?.physicalWidth ?: b.width(), mode?.physicalHeight ?: b.height())
        return w to h
    }

    /** "WxH" para [scale]% de [w]x[h] (mesma proporcao, largura e altura pares); "" a 100%. */
    fun renderSizeFor(w: Int, h: Int, scale: Int): String {
        if (scale >= 100 || w <= 0 || h <= 0) return ""
        val rw = (w * scale / 100.0 / 2).roundToInt() * 2
        val rh = (h * scale / 100.0 / 2).roundToInt() * 2
        return "${rw.coerceAtLeast(2)}x${rh.coerceAtLeast(2)}"
    }

    /** A % (passo de 5) mais perto de uma altura alvo (perfis: 480, 540, 720; 0 = nativa). */
    fun scaleForHeight(nativeH: Int, targetH: Int): Int {
        if (targetH <= 0 || nativeH <= 0 || targetH >= nativeH) return 100
        val pct = targetH * 100.0 / nativeH
        return ((pct / SCALE_STEP).roundToInt() * SCALE_STEP).coerceIn(SCALE_MIN, 100)
    }

    /** Limite de fps (MGR_FPS_CAP, sempre o numero). */
    val FPS_CAPS = listOf(30, 45, 60, 90, 120)
    fun fpsCap(c: Context): Int = prefs(c).getInt("fps_cap", 60)
    fun setFpsCap(c: Context, v: Int) = prefs(c).edit().putInt("fps_cap", v).apply()

    /**
     * Upscaling e interpolacao no present do DXVK (pc/ports/dxvk_android/fx/present_fx.h): DXVK_FSR (0 bilinear,
     * 1 FSR 1 EASU+RCAS, 2 CAS), DXVK_FSR_SHARPNESS (0..1), DXVK_FSR_OUT (0 = ecra, 720 = buffers de 720p que o
     * compositor escala) e DXVK_FI (0/1: um frame gerado entre cada par de frames do jogo, 30 -> 60; precisa do
     * limite de 30 fps).
     */
    fun fsrMode(c: Context): Int = prefs(c).getInt("fsr", 0)
    fun setFsrMode(c: Context, v: Int) = prefs(c).edit().putInt("fsr", v).apply()
    fun fsrSharpness(c: Context): Float = prefs(c).getFloat("fsr_sharpness", 0.8f)
    fun setFsrSharpness(c: Context, v: Float) = prefs(c).edit().putFloat("fsr_sharpness", v).apply()
    fun fsrOut(c: Context): Int = prefs(c).getInt("fsr_out", 0)
    fun setFsrOut(c: Context, v: Int) = prefs(c).edit().putInt("fsr_out", v).apply()
    fun frameInterp(c: Context): Boolean = prefs(c).getBoolean("frame_interp", false)
    fun setFrameInterp(c: Context, v: Boolean) = prefs(c).edit().putBoolean("frame_interp", v).apply()

    /**
     * Multithread (MGR_MT=1): as threads do jogo correm o codigo do jogo em paralelo e so os imports com estado
     * tomam a "maquina". Odin 2 Mini, combate do CONTINUE: 26,9-27,7 fps contra 17,4-17,9 com a maquina unica.
     */
    fun multithread(c: Context): Boolean = prefs(c).getBoolean("multithread", true)
    fun setMultithread(c: Context, v: Boolean) = prefs(c).edit().putBoolean("multithread", v).apply()

    /** Perfil escolhido ("" = ainda nenhum: o primeiro arranque usa o do SoC; "custom" = Personalizado). */
    fun profile(c: Context): String = prefs(c).getString("profile", "").orEmpty()
    fun setProfile(c: Context, v: String) = prefs(c).edit().putString("profile", v).apply()

    /**
     * Perfis: GraphicOption do jogo + resolucao interna (altura alvo, convertida para % do ecra) + limite de fps.
     * Baixo = o antigo "Fraco" (SD665/480+), Medio = o antigo "Baixo", Alto = o antigo "Medio", Ultra = o antigo "Alto".
     */
    class Profile(val id: String, val label: Int, val height: Int, val fps: Int, val opts: Map<Int, Int>)
    val PROFILES = listOf(
        Profile("baixo", R.string.profile_low, 480, 30, mapOf(1 to 0, 2 to 1, 3 to 0, 7 to 0, 8 to 0, 9 to 0)),
        Profile("medio", R.string.profile_medium, 540, 30, mapOf(1 to 1, 2 to 1, 3 to 0, 7 to 0, 8 to 0, 9 to 0)),
        Profile("alto", R.string.profile_high, 720, 60, mapOf(1 to 1, 2 to 0, 3 to 1, 7 to 0, 8 to 1, 9 to 0)),
        Profile("ultra", R.string.profile_ultra, 0, 60, mapOf(1 to 2, 2 to 0, 3 to 1, 7 to 1, 8 to 1, 9 to 1)),
    )
    /** SoCs fracos (SD665, SD680, SD480+, Helio G-series...): o perfil de omissao e o Baixo; o resto, Alto. */
    fun defaultProfile(): String {
        val soc = (android.os.Build.SOC_MODEL ?: "").uppercase()
        val weak = listOf("SM6115", "SM6125", "SM6225", "SM4350", "SM4375", "SM4450", "MT67", "MT68", "UMS", "T6", "T7")
        return if (weak.any { soc.startsWith(it) } || Runtime.getRuntime().availableProcessors() <= 6) "baixo" else "alto"
    }
    /** Aplica um perfil: opcoes graficas do jogo (gravadas no GraphicOption), resolucao interna e fps. */
    fun applyProfile(c: Context, id: String): GraphicOptions.WriteResult? {
        val p = PROFILES.firstOrNull { it.id == id } ?: return null
        setProfile(c, id)
        setRenderScale(c, scaleForHeight(nativeSize(c).second, p.height))
        setFpsCap(c, p.fps)
        if (p.fps != 30) setFrameInterp(c, false)
        val v = GraphicOptions.values(c); p.opts.forEach { (i, x) -> v[i] = x }
        return GraphicOptions.save(c, v)
    }

    /** Todas as variaveis de ambiente das opcoes da launcher (sempre todas: o processo do jogo pode ser reutilizado). */
    fun launchEnv(c: Context, displayHz: Int): List<Pair<String, String>> {
        val (w, h) = nativeSize(c)
        return listOf(
            "MGR_RENDER_SIZE" to renderSizeFor(w, h, renderScale(c)),
            "MGR_FPS_CAP" to fpsCap(c).toString(),
            "MGR_MT" to (if (multithread(c)) "1" else "0"),
            "MGR_TOUCH_MODE" to com.dfdx047.zandatsu.pc.data.LauncherPrefs(c).pcTouchMode.key,
            "DXVK_FSR" to fsrMode(c).toString(),
            "DXVK_FSR_SHARPNESS" to String.format(java.util.Locale.ROOT, "%.2f", fsrSharpness(c)),
            "DXVK_FSR_OUT" to fsrOut(c).toString(),
            "DXVK_FI" to (if (frameInterp(c)) "1" else "0"),
            "DXVK_FI_HZ" to displayHz.toString(),
        )
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
        val magic = c.assets.open("libmgrpc.zdat").use { s -> ByteArray(4).also { DataInputStream(s).readFully(it) } }
        if (String(magic) == "ZDT3") { unpackXz(c, key, fd, onProgress); return keep }
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

    /**
     * ZDT3: cabecalho igual ao ZDT2; cada bloco (8 MiB) e xz encriptado com AES-256-GCM. Os blocos sao independentes:
     * descodificados em paralelo (um por nucleo) e escritos no memfd na posicao certa (pwrite).
     */
    private fun unpackXz(c: Context, key: ByteArray, fd: java.io.FileDescriptor, onProgress: (Long, Long) -> Unit) {
        val threads = Runtime.getRuntime().availableProcessors().coerceIn(2, 8)
        val pool = java.util.concurrent.Executors.newFixedThreadPool(threads)
        val slots = java.util.concurrent.Semaphore(threads * 2)
        val failure = java.util.concurrent.atomic.AtomicReference<Throwable?>(null)
        val written = java.util.concurrent.atomic.AtomicLong()
        FileOutputStream(fd).use { fo ->
            val ch = fo.channel
            c.assets.open("libmgrpc.zdat").use { raw ->
                val input = DataInputStream(raw.buffered(1 shl 20))
                val header = ByteArray(24).also { input.readFully(it) }
                val hb = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN)
                val chunk = hb.getInt(4); val size = hb.getLong(8)
                val spec = SecretKeySpec(key, "AES")
                var done = 0L; var i = 0
                while (done < size && failure.get() == null) {
                    val n = minOf(chunk.toLong(), size - done).toInt()
                    val blockLen = Integer.reverseBytes(input.readInt())
                    if (blockLen < 16 || blockLen > chunk + (chunk shr 3) + 65536) throw PcException("libmgrpc.zdat: bloco $i")
                    val block = ByteArray(blockLen).also { input.readFully(it) }
                    val index = i; val offset = done
                    slots.acquire()
                    pool.execute {
                        try {
                            val nonce = ByteArray(12)
                            System.arraycopy(header, 16, nonce, 0, 8)
                            ByteBuffer.wrap(nonce, 8, 4).order(ByteOrder.BIG_ENDIAN).putInt(index)
                            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
                            cipher.init(Cipher.DECRYPT_MODE, spec, GCMParameterSpec(128, nonce)); cipher.updateAAD(header)
                            val dec = cipher.doFinal(block)
                            val plain = ByteArray(n)
                            org.tukaani.xz.XZInputStream(java.io.ByteArrayInputStream(dec)).use { DataInputStream(it).readFully(plain) }
                            val bb = ByteBuffer.wrap(plain)
                            var at = offset
                            while (bb.hasRemaining()) at += ch.write(bb, at)
                            onProgress(written.addAndGet(n.toLong()), size)
                        } catch (t: Throwable) {
                            failure.compareAndSet(null, t)
                        } finally {
                            slots.release()
                        }
                    }
                    done += n; i++
                }
                pool.shutdown()
                pool.awaitTermination(10, java.util.concurrent.TimeUnit.MINUTES)
                failure.get()?.let { throw PcException("libmgrpc.zdat: ${it.message}") }
                if (written.get() != size) throw PcException("libmgrpc.zdat: ${written.get()} de $size bytes")
            }
        }
    }
}
