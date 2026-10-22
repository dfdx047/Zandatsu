package com.dfdx047.swordman.game

import android.app.Activity
import android.graphics.Bitmap
import android.graphics.Color
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.view.Gravity
import android.view.PixelCopy
import android.view.SurfaceView
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.TextView
import com.dfdx047.swordman.data.AppPaths
import java.io.File
import java.io.FileOutputStream
import kotlin.math.abs
import kotlin.math.log10

/**
 * Autoteste do renderizador (menu do jogo > Depuracao). Com a imagem congelada (0105: a GPU
 * repete o ultimo frame com o jogo a correr) a imagem e sempre a mesma; o teste tira uma foto
 * com todas as otimizacoes ligadas (a referencia), depois desliga UMA de cada vez, compara a
 * foto com a referencia e mede o tempo de GPU por frame. Depois (fase de efeitos) salta UM
 * pixel shader de cada vez e mede quanto cada passe do MGR custa.
 *
 * Resultado em logs/autoteste.txt (+ uma foto por estado, logs/autoteste_NN.jpg), que entram
 * no "Exportar logs".
 */
class RenderSelfTest(
    private val activity: Activity,
    private val surface: SurfaceView,
    private val onDone: () -> Unit,
) {
    /** Otimizacao: rotulo + cvars que a ligam (valor "ligado"/"desligado"). */
    private class Opt(val label: String, val cvars: Map<String, Pair<String, String>>)

    // Sem os interruptores das transferencias entre render targets (keep_*, pares prof->cor,
    // drop_transfers, clear_siblings): eram do Burst Limit e, trocados ao vivo no MGR, deixavam o
    // Turnip sem GPU (VK_ERROR_DEVICE_LOST no RP5, 20/10).
    private val opts = listOf(
        Opt("resolves repetidos/sem leitor (passo 3)", mapOf("dbz3_resolve_dedupe" to ("true" to "false"))),
        Opt("draws nulos (passo 3)", mapOf("dbz3_skip_null_draws" to ("true" to "false"))),
        Opt("passe sem carregar a cor (passo 3)", mapOf("dbz3_discard_full_overwrite" to ("true" to "false"))),
        Opt("passes direto na textura (passo 4)", mapOf("dbz3_render_into_texture" to ("true" to "false"))),
        // 0095-0097 (17/10): desligados por padrao ate um teste no aparelho; com a config de
        // sempre o autoteste liga cada um ("com ...") e mostra a imagem e quanto vale.
        Opt("copia por desenho no atalho resolve -> textura", mapOf("dbz3_direct_resolve_draw" to ("true" to "false"))),
        Opt("resolves servidos pela textura do passo 4", mapOf("dbz3_rit_serve_resolves" to ("true" to "false"))),
        Opt("escritas sem leitor neutras (passo 3)", mapOf("dbz3_resolve_dedupe_unread_neutral" to ("true" to "false"))),
        // Partes dos render targets diretos (no Mali, sem eles o Goku volta a aparecer - 16/10 14:06).
        Opt("RT diretos: restauros ignorados", mapOf("dbz3_rt_elide_restores" to ("true" to "false"))),
        Opt("RT diretos: restauro da profundidade", mapOf("dbz3_skip_depth_restore" to ("true" to "false"))),
        Opt("RT diretos: Clear 4x direto em 1x", mapOf("dbz3_clear_to_1x" to ("true" to "false"))),
        Opt("uploads antes do passe", mapOf("dbz3_hoist_uploads" to ("true" to "false"))),
        Opt("resolve: leitura logo no fim", mapOf("dbz3_resolve_early_transition" to ("true" to "false"))),
        // 0110: atalho resolve -> textura para a profundidade float e a cor 32_FLOAT (MGR).
        Opt("atalho profundidade float / 32_FLOAT (0110)", mapOf("swordman_rtt_float" to ("true" to "false"))),
        Opt("render targets diretos (DF)", mapOf("dbz3_rt_direct" to ("true" to "false"))),
        Opt("resolve sem memoria (DF)", mapOf("dbz3_direct_resolve_skip_memory" to ("true" to "false"))),
        Opt("resolve sem memoria no Turnip", mapOf("swordman_skip_memory_turnip" to ("true" to "false"))),
        // Lote 2 do XenDroid Plus (22/10): desligados por padrao; o autoteste liga cada um ("com ...").
        Opt("MSAA 4x como 2x (XP)", mapOf("msaa_4x_as_2x" to ("true" to "false"))),
        Opt("4x de profundidade/simples em 1x (XP)", mapOf(
            "vulkan_depth_4x_as_1x" to ("true" to "false"),
            "vulkan_samples_as_pixels_simple_ps" to ("true" to "false"))),
        Opt("pular transfers sobrescritos (XP)", mapOf("skip_overwritten_transfers" to ("true" to "false"))),
    )

    /** Estado de referencia: o que o jogo tem agora (o da config, ex. sem restauros ignorados no Mali). */
    private var base = emptyMap<String, String>()
    /** So as otimizacoes que este build conhece (getCvar vazio = cvar que nao existe). */
    private var active = emptyList<Opt>()

    private val main = Handler(Looper.getMainLooper())
    private val worker = HandlerThread("SwordManSelfTest").apply { start() }
    private val workHandler = Handler(worker.looper)
    private val report = StringBuilder()
    private var original = emptyMap<String, String>()
    /** Lista de pixel shaders saltados da config (desfoque do brilho), reposta no fim. */
    private var originalSkip = ""
    private var reference: IntArray? = null
    private var noise = 0.0
    private var status: TextView? = null
    private var shot = 0

    fun start() {
        val all = opts.flatMap { it.cvars.keys }
        original = all.associateWith { NativeGameState.getCvar(it) }
        active = opts.filter { o -> o.cvars.keys.all { original[it].orEmpty().isNotEmpty() } }
        base = active.flatMap { o -> o.cvars.map { it.key to original[it.key].orEmpty() } }.toMap()
        report.append("Autoteste do render - ").append(java.util.Date()).append('\n')
        report.append("Referencia: as otimizacoes como estao na config. Cada linha troca UMA delas\n")
        report.append("('sem' = desligada, 'com' = ligada) e compara a imagem com a referencia.\n")
        report.append("Se a diferenca for maior que o ruido, essa otimizacao muda a imagem (provavel erro).\n")
        try {
            val bisect = File(AppPaths.logsDir, "turnip_bisect.txt")
            if (bisect.isFile && System.currentTimeMillis() - bisect.lastModified() < 3 * 3600_000L) {
                report.append("Ultimo arranque: ").append(bisect.readLines().lastOrNull().orEmpty()).append('\n')
            }
        } catch (_: Exception) {
        }
        // 0091: abaixo de 720p os passos do render nativo ligam por percentagem (a 75% todos,
        // a 67% so o atalho resolve -> textura e o passo 4): a linha diz a que resolucao foi.
        val percent = NativeGameState.getCvar("draw_resolution_percent")
        if (percent.isNotEmpty()) {
            report.append("Resolucao interna: draw_resolution_percent=").append(percent).append('\n')
        }
        // 0092: como os quadros chegam a tela (modo de apresentacao, Hz da tela, interpolacao). A
        // interpolacao nao muda a imagem do jogo (as fotos sao da saida do jogo), por isso nao e
        // um dos interruptores; esta linha diz em que modo o autoteste correu.
        val present = NativeGameState.presentInfo()
        report.append("Apresentacao: ").append(present.ifEmpty { "desconhecida" })
            .append(" | frame_rate=").append(NativeGameState.getCvar("frame_rate"))
            .append(" dbz3_frame_interpolation=").append(NativeGameState.getCvar("dbz3_frame_interpolation"))
            .append('\n')
        report.append("Config: " + base.entries.joinToString(" ") { "${it.key}=${it.value}" } + "\n\n")
        showStatus(activity.getString(com.dfdx047.swordman.R.string.selftest_start))
        originalSkip = NativeGameState.getCvar("swordman_skip_ps")
        NativeGameState.setSkipPs("")
        NativeGameState.setCvars(base + mapOf("swordman_ps_census" to "true"), persist = false)
        // Referencia: tudo ligado, duas fotos para medir o ruido.
        after(3500) {
            capture { a ->
                after(600) {
                    capture { b ->
                        reference = a
                        noise = if (a != null && b != null) diff(a, b).first else 0.0
                        NativeGameState.psCensus(reset = true)
                        measureGpu { ms ->
                            baseMs = ms
                            census = NativeGameState.psCensus(reset = true)
                            NativeGameState.setCvars(mapOf("swordman_ps_census" to "false"), persist = false)
                            if (ms.isNaN()) {
                                report.append("AVISO: a GPU nao desenhou nenhum frame com a imagem congelada (sem frame gravado?).\n")
                                report.append("As comparacoes abaixo nao valem: a tela esta parada, nao congelada.\n")
                            }
                            report.append(String.format(java.util.Locale.ROOT, "%-46s ruido %.3f%%, GPU %s\n", "referencia (config)", noise, fmtMs(ms)))
                            log("autoteste: referencia, ruido %.3f%%, GPU %s".format(java.util.Locale.ROOT, noise, fmtMs(ms)))
                            // Efeitos primeiro: alguns interruptores abaixo (resolve sem
                            // memoria) deixam a memoria do frame congelado diferente ate ao fim.
                            startEffects()
                        }
                    }
                }
            }
        }
    }

    private fun step(i: Int) {
        if (i > active.size) {
            NativeGameState.setCvars(base, persist = false)
            finish()
            return
        }
        val off: Map<String, String>
        val label: String
        if (i == active.size) {
            label = "TUDO desligado"
            off = active.flatMap { o -> o.cvars.map { it.key to it.value.second } }.toMap()
        } else {
            val o = active[i]
            // Liga a que estava desligada na config, desliga a que estava ligada.
            val isOn = o.cvars.all { base[it.key] == it.value.first }
            label = (if (isOn) "sem " else "com ") + o.label
            off = base + o.cvars.mapValues { if (isOn) it.value.second else it.value.first }
        }
        showStatus(activity.getString(com.dfdx047.swordman.R.string.selftest_step, i + 1, active.size + 1, label))
        log("autoteste: inicio '$label'")
        NativeGameState.setCvars(off, persist = false)
        measureGpu { ms ->
            capture { img ->
                val ref = reference
                val line = if (img == null || ref == null) {
                    String.format(java.util.Locale.ROOT, "%-46s sem foto\n", label)
                } else {
                    val (pct, psnr) = diff(ref, img)
                    val verdict = when {
                        pct <= noise * 2 + 0.05 -> "igual"
                        pct < 2.0 -> "PEQUENA diferenca"
                        else -> "DIFERENTE"
                    }
                    String.format(java.util.Locale.ROOT, "%-46s %.3f%% pixels, PSNR %.1f dB -> %s | GPU %s (%s)\n", label, pct, psnr, verdict, fmtMs(ms), fmtDelta(ms))
                }
                report.append(line)
                log("autoteste: " + line.trim())
                step(i + 1)
            }
        }
    }

    // -------------------------------------------------------------- efeitos

    /**
     * Fase 2 (0107): com a mesma imagem congelada, salta os draws de UM pixel shader de cada
     * vez (swordman_skip_ps) e mede quanto tempo de GPU o frame perde. Os candidatos sao os
     * passes de pos-processamento conhecidos do MGR + os pixel shaders com mais draws neste
     * frame (o censo da referencia). A foto de cada um mostra que efeito desaparece.
     */
    private class Fx(val hash: String, val draws: Long, val pitch: Int, val prim: Int)
    private class FxResult(val fx: Fx, val ms: Double, val pct: Double, val shot: Int)

    private var census = ""
    private var fxList = emptyList<Fx>()
    private val fxResults = ArrayList<FxResult>()

    private fun startEffects() {
        val seen = census.split(',').mapNotNull { e ->
            val f = e.split(':')
            if (f.size < 4) null else Fx(f[0], f[1].toLongOrNull() ?: 0, f[2].toIntOrNull() ?: 0, f[3].toIntOrNull() ?: 0)
        }
        if (seen.isEmpty()) {
            report.append("\nEfeitos: censo vazio (build sem 0107?)\n")
            step(0)
            return
        }
        val known = seen.filter { it.hash in KNOWN_POST }
        val rest = seen.filter { it.hash !in KNOWN_POST }.take((MAX_FX - known.size).coerceAtLeast(0))
        // Grupos: passes que so fazem sentido juntos (a cadeia pequena de 320x176 = brilho).
        val groups = GROUPS.filter { g -> g.split(',').all { h -> seen.any { it.hash == h } } }
            .map { g -> Fx(g, g.split(',').sumOf { h -> seen.first { it.hash == h }.draws }, 320, 6) }
        fxList = groups + known + rest
        report.append("\nEfeitos (salta UM pixel shader de cada vez; GPU = ms por frame, delta vs referencia)\n")
        report.append("Censo: ${seen.size} pixel shaders, ${seen.sumOf { it.draws }} draws durante a medicao\n")
        fxStep(0)
    }

    private fun fxStep(i: Int) {
        if (i >= fxList.size) {
            NativeGameState.setSkipPs("")
            measureGpu { end ->
                report.append(String.format(java.util.Locale.ROOT, "referencia no fim: GPU %s\n", fmtMs(end)))
                report.append("\nOrdenado pelo ganho (o que mais pesa primeiro):\n")
                fxResults.sortedBy { it.ms }.forEach { r ->
                    report.append(String.format(java.util.Locale.ROOT, "  PS %s  %-10s  imagem %.2f%%  foto %02d  draws %d pitch %d\n",
                        r.fx.hash, fmtDelta(r.ms), r.pct, r.shot, r.fx.draws, r.fx.pitch))
                }
                report.append("\nOtimizacoes (cada uma trocada sozinha):\n")
                step(0)
            }
            return
        }
        val fx = fxList[i]
        showStatus(activity.getString(com.dfdx047.swordman.R.string.selftest_step, i + 1, fxList.size, "PS ${fx.hash}"))
        log("autoteste: efeito sem PS ${fx.hash}")
        NativeGameState.setSkipPs(fx.hash)
        measureGpu { ms ->
            val myShot = shot
            capture { img ->
                val ref = reference
                val pct = if (img != null && ref != null) diff(ref, img).first else -1.0
                fxResults += FxResult(fx, ms, pct, myShot)
                val line = String.format(java.util.Locale.ROOT, "sem PS %s (draws %d, pitch %d, prim %d)  imagem %.2f%%  GPU %s (%s)  foto %02d\n",
                    fx.hash, fx.draws, fx.pitch, fx.prim, pct, fmtMs(ms), fmtDelta(ms), myShot)
                report.append(line)
                log("autoteste: " + line.trim())
                fxStep(i + 1)
            }
        }
    }

    // -------------------------------------------------------------- tempo de GPU

    private var baseMs = Double.NaN

    /** Espera a troca assentar e devolve os ms de GPU por frame numa janela (NaN sem dados). */
    private fun measureGpu(done: (Double) -> Unit) {
        after(SETTLE_MS) {
            val a = NativeGameState.gpuStats()
            after(MEASURE_MS) {
                val b = NativeGameState.gpuStats()
                val frames = b[0] - a[0]
                done(if (frames <= 0) Double.NaN else (b[1] - a[1]) / 1000.0 / frames)
            }
        }
    }

    private fun fmtMs(ms: Double) = if (ms.isNaN()) "n/d" else String.format(java.util.Locale.ROOT, "%.2f ms", ms)

    private fun fmtDelta(ms: Double): String =
        if (ms.isNaN() || baseMs.isNaN()) "n/d" else String.format(java.util.Locale.ROOT, "%+.2f ms", ms - baseMs)

    private companion object {
        const val SETTLE_MS = 1200L
        const val MEASURE_MS = 2000L
        const val MAX_FX = 60

        /** Grupos medidos juntos (hashes separados por virgula, como o swordman_skip_ps). */
        val GROUPS = listOf(
            "C45580526034B1F2,4086D1DF9FA6220E,38C32FC0BC7F322D",
            "C45580526034B1F2,4086D1DF9FA6220E",
        )

        /** Passes de ecra inteiro do MGR vistos na captura de 19/10 (RP5). */
        val KNOWN_POST = setOf(
            "11C7657530951A90", "E104A745F271970A", "7BE61437FF049E59", "28AEC10B4A0E14FF",
            "DAFD35EBE60D9860", "7ABDFEFBCADB4BF9", "E060C0FFE9EE118C", "17EE3DCA7468F67B",
            "A755AD0AC2470588", "C45580526034B1F2", "4086D1DF9FA6220E", "38C32FC0BC7F322D",
            "F971EBDCFE761317", "2CE21E05F9680D34", "F854BF3B7BF7AA9D", "B402E1761727D55B",
        )
    }

    private fun finish() {
        // Volta tudo ao que estava.
        NativeGameState.setCvars(original.filterValues { it.isNotEmpty() }, persist = false)
        NativeGameState.setSkipPs(originalSkip)
        NativeGameState.setCvars(mapOf("swordman_ps_census" to "false"), persist = false)
        try {
            File(AppPaths.logsDir, "autoteste.txt").writeText(report.toString())
        } catch (_: Exception) {
        }
        showStatus(activity.getString(com.dfdx047.swordman.R.string.selftest_exporting))
        // O zip vai logo para Download/ (o mesmo do "Exportar logs"): e so mandar.
        workHandler.post {
            val name = try {
                com.dfdx047.swordman.data.LogExporter.export(activity)
            } catch (e: Exception) {
                null
            }
            showStatus(if (name != null) activity.getString(com.dfdx047.swordman.R.string.selftest_done, name) else activity.getString(com.dfdx047.swordman.R.string.selftest_done_fail))
            main.postDelayed({
                status?.let { (it.parent as? ViewGroup)?.removeView(it) }
                status = null
                worker.quitSafely()
                onDone()
            }, 6000)
        }
    }

    // ------------------------------------------------------------------ fotos

    private fun capture(done: (IntArray?) -> Unit) {
        val w = surface.width
        val h = surface.height
        if (w <= 0 || h <= 0) {
            done(null)
            return
        }
        val bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        try {
            PixelCopy.request(surface, bmp, { result ->
                if (result != PixelCopy.SUCCESS) {
                    main.post { done(null) }
                    return@request
                }
                workHandler.post {
                    // 320x180 chega para ver um efeito a faltar e ignora o serrilhado.
                    val small = Bitmap.createScaledBitmap(bmp, 320, 180, true)
                    val px = IntArray(320 * 180)
                    small.getPixels(px, 0, 320, 0, 0, 320, 180)
                    try {
                        val f = File(AppPaths.logsDir, "autoteste_%02d.jpg".format(shot++))
                        FileOutputStream(f).use { Bitmap.createScaledBitmap(bmp, 640, 360, true).compress(Bitmap.CompressFormat.JPEG, 85, it) }
                    } catch (_: Exception) {
                    }
                    main.post { done(px) }
                }
            }, workHandler)
        } catch (e: Exception) {
            done(null)
        }
    }

    /** % de pixels com diferenca visivel (>24 em algum canal) e PSNR. */
    private fun diff(a: IntArray, b: IntArray): Pair<Double, Double> {
        var changed = 0
        var se = 0.0
        for (k in a.indices) {
            val p = a[k]
            val q = b[k]
            val dr = abs(Color.red(p) - Color.red(q))
            val dg = abs(Color.green(p) - Color.green(q))
            val db = abs(Color.blue(p) - Color.blue(q))
            if (dr > 24 || dg > 24 || db > 24) changed++
            se += (dr * dr + dg * dg + db * db).toDouble()
        }
        val mse = se / (a.size * 3.0)
        val psnr = if (mse <= 1e-9) 99.0 else 10 * log10(255.0 * 255.0 / mse)
        return changed * 100.0 / a.size to psnr
    }

    // ------------------------------------------------------------------- util

    private fun after(ms: Long, r: () -> Unit) {
        main.postDelayed(r, ms)
    }

    private fun log(line: String) {
        android.util.Log.i("SwordMan", line)
        NativeGameState.logLine(line)
    }

    private fun showStatus(text: String) {
        main.post {
            val tv = status ?: TextView(activity).also { t ->
                t.setTextColor(Color.parseColor("#FFD63C"))
                t.setBackgroundColor(Color.argb(170, 22, 10, 38))
                t.textSize = 13f
                val pad = (8 * activity.resources.displayMetrics.density).toInt()
                t.setPadding(pad, pad / 2, pad, pad / 2)
                activity.addContentView(
                    t,
                    FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL,
                    ).apply { bottomMargin = pad },
                )
                status = t
            }
            tv.text = text
        }
    }
}
