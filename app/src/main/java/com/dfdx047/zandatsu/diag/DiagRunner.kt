package com.dfdx047.zandatsu.diag

import android.app.ActivityManager
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.Build
import android.os.PerformanceHintManager
import android.os.PowerManager
import android.os.Process
import android.os.SystemClock
import com.dfdx047.zandatsu.BuildConfig
import com.dfdx047.zandatsu.data.AppLog
import com.dfdx047.zandatsu.data.AppPaths
import com.dfdx047.zandatsu.data.GameConfig
import com.dfdx047.zandatsu.data.GameLib
import com.dfdx047.zandatsu.data.LogExporter
import com.dfdx047.zandatsu.drivers.DriverKind
import com.dfdx047.zandatsu.drivers.DriverRepository
import com.dfdx047.zandatsu.game.GameActivity
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.TimeZone
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

/**
 * Teste de GPU (Configuracoes > Diagnostico, 04/11): recolhe o aparelho, o Vulkan e o driver, corre os micro-testes
 * (cada um num processo: [DiagService] para os de Vulkan, o proprio jogo para os shaders do jogo e o desempenho) e
 * junta tudo em zandatsu-diag_<gpu>_<data>.zip, so com ficheiros da pasta do app.
 *
 * Um teste que derruba o processo fica "crash" com o passo onde estava, e o teste seguinte corre na mesma.
 */
object DiagRunner {
    enum class Status { PENDING, RUNNING, PASS, FAIL, CRASH, TIMEOUT, SKIP }

    data class Item(val id: String, val status: Status = Status.PENDING, val message: String = "", val crashedAt: String? = null)

    data class State(
        val running: Boolean = false,
        val items: List<Item> = emptyList(),
        val zip: File? = null,
        val zipEntries: List<Pair<String, Long>> = emptyList(),
        val gpu: GpuFamily? = null,
        val summary: JSONObject? = null,
    )

    /** Os testes, pela ordem; os do jogo correm no processo :game. */
    val NATIVE = listOf("info", "pipelines_serial", "pipelines_parallel", "viewport_scissor", "clear_loadop", "bc_decode", "timestamps")

    /** Velocidade de cada nucleo: no processo do app (o :diag fica no cpuset de fundo, que no RP5 so tem os nucleos 0-3). */
    const val CPU = "cpu_cores"
    val GAME = listOf("game_shaders_serial", "game_shaders_parallel")
    const val PERF = "perf"
    val ORDER = listOf("info", "pipelines_serial", "pipelines_parallel", "game_shaders_serial", "game_shaders_parallel",
        "viewport_scissor", "clear_loadop", "bc_decode", "timestamps", "cpu_cores", PERF)

    private val _state = MutableStateFlow(State())
    val state: StateFlow<State> = _state

    /** "force_crash=<teste>/<passo>" para provar que o teste continua (so pelo adb: --es diag_options). */
    var options: String = ""

    fun gpu(): GpuFamily = GpuFamily.of(DriverRepository.gpuName(probe = false), DriverRepository.activeDir() != null)

    private fun update(id: String, status: Status, message: String = "", crashedAt: String? = null) {
        _state.value = _state.value.copy(items = _state.value.items.map {
            if (it.id == id) Item(id, status, message, crashedAt) else it
        })
    }

    private val scope = kotlinx.coroutines.CoroutineScope(kotlinx.coroutines.SupervisorJob() + Dispatchers.IO)

    /** Corre fora do ViewModel: o launcher pode ficar por tras do jogo durante minutos. */
    fun start(context: Context, onDone: () -> Unit = {}) {
        if (_state.value.running) return
        scope.launch {
            try {
                run(context)
                onDone()
            } catch (e: Exception) {
                AppLog.w("diag", "teste de GPU parou", e)
                _state.value = _state.value.copy(running = false)
            }
        }
    }

    suspend fun run(context: Context) = withContext(Dispatchers.IO) {
        if (_state.value.running) return@withContext
        val gpu = gpu()
        runStartedAt = System.currentTimeMillis() - 1000
        _state.value = State(running = true, items = ORDER.map { Item(it) }, gpu = gpu)
        val stamp = SimpleDateFormat("yyyyMMdd-HHmm", Locale.ROOT).format(Date())
        val root = File(AppPaths.appDir, "diag")
        val dir = File(root, "run-$stamp").apply { deleteRecursively(); mkdirs() }
        File(dir, "logs").mkdirs()
        AppLog.i("diag", "teste de GPU: ${gpu.name} (${gpu.family}), pasta ${dir.name}")
        val tests = JSONArray()
        // Opcao de desenvolvimento "only=a,b" (diag_options): so esses testes.
        val only = options.split(';').firstOrNull { it.startsWith("only=") }?.substringAfter('=')?.split(',')?.toSet()
        for (id in ORDER) {
            update(id, Status.RUNNING)
            val result = try {
                when {
                    only != null && id !in only -> JSONObject().put("id", id).put("status", "skip").put("message", "only")
                    else -> when (id) {
                    CPU -> runCpu(dir)
                    in NATIVE -> runNative(context, dir, id)
                    in GAME -> runGameShaders(context, dir, id)
                    else -> runPerf(context, dir)
                    }
                }
            } catch (e: Exception) {
                JSONObject().put("id", id).put("status", "fail").put("message", "erro no app: ${e.message}")
            }
            if (id != PERF) tests.put(result)
            File(dir, "tests").mkdirs()
            File(dir, "tests/$id.json").writeText(result.toString(1))
            val st = when (result.optString("status")) {
                "pass" -> Status.PASS
                "crash" -> Status.CRASH
                "timeout" -> Status.TIMEOUT
                "skip" -> Status.SKIP
                else -> Status.FAIL
            }
            update(id, st, result.optString("message"), result.optString("crashedAt").ifEmpty { null })
            AppLog.i("diag", "$id: ${result.optString("status")} ${result.optString("message")}")
        }
        val summary = summary(context, gpu, dir, tests)
        File(dir, "summary.json").writeText(summary.toString(1))
        val zip = File(root, "zandatsu-diag_${gpu.family.replace(Regex("[^a-z0-9-]"), "")}_$stamp.zip")
        val entries = zip(dir, zip)
        root.listFiles()?.filter { it.name.startsWith("zandatsu-diag_") && it != zip }?.sortedBy { it.lastModified() }
            ?.dropLast(2)?.forEach { it.delete() }
        _state.value = _state.value.copy(running = false, zip = zip, zipEntries = entries, summary = summary)
        AppLog.i("diag", "teste de GPU: ${zip.name} (${zip.length() / 1024} KB)")
    }

    // ------------------------------------------------------------------ Vulkan, processo :diag

    private fun pidOf(context: Context, suffix: String): Int? =
        context.getSystemService(ActivityManager::class.java).runningAppProcesses
            ?.firstOrNull { it.processName == context.packageName + suffix }?.pid

    private suspend fun waitGone(context: Context, suffix: String, ms: Long) {
        val t0 = SystemClock.elapsedRealtime()
        while (pidOf(context, suffix) != null && SystemClock.elapsedRealtime() - t0 < ms) delay(100)
    }

    /** Numa thread propria: se o nativo prender, a corrida segue ao fim de 90 s (a thread fica perdida ate o app sair). */
    private suspend fun runCpu(dir: File): JSONObject {
        val result = kotlinx.coroutines.CompletableDeferred<JSONObject>()
        Thread({
            result.complete(
                try {
                    JSONObject(DiagNative.runInProcess(CPU, dir.absolutePath, options))
                } catch (e: Throwable) {
                    JSONObject().put("id", CPU).put("status", "fail").put("message", e.toString())
                },
            )
        }, "zdiag-cpu").start()
        return kotlinx.coroutines.withTimeoutOrNull(90_000) { result.await() }
            ?: JSONObject().put("id", CPU).put("status", "timeout").put("crashedAt", "$CPU/${progressStep(File(dir, "$CPU.progress")) ?: "?"}")
                .put("message", "mais de 90 s")
    }

    private suspend fun runNative(context: Context, dir: File, id: String): JSONObject {
        val json = File(dir, "$id.json").apply { delete() }
        val progress = File(dir, "$id.progress").apply { delete() }
        waitGone(context, DiagService.PROCESS_SUFFIX, 5000)
        context.startService(
            Intent(context, DiagService::class.java)
                .putExtra(DiagService.EXTRA_TEST, id)
                .putExtra(DiagService.EXTRA_OUT, dir.absolutePath)
                .putExtra(DiagService.EXTRA_OPTIONS, options),
        )
        val t0 = SystemClock.elapsedRealtime()
        var seen = false
        while (true) {
            delay(150)
            val pid = pidOf(context, DiagService.PROCESS_SUFFIX)
            if (pid != null) seen = true
            val elapsed = SystemClock.elapsedRealtime() - t0
            if (json.isFile && pid == null) return JSONObject(json.readText())
            if (json.isFile && elapsed > 120_000) return JSONObject(json.readText())
            if (pid == null && (seen || elapsed > 15_000)) {
                delay(300)
                if (json.isFile) return JSONObject(json.readText())
                return crashed(id, progressStep(progress), elapsed)
            }
            if (elapsed > 120_000 && pid != null) {
                Process.killProcess(pid)
                return JSONObject().put("id", id).put("status", "timeout").put("durationMs", elapsed)
                    .put("crashedAt", "$id/${progressStep(progress) ?: "?"}").put("message", "mais de 2 min")
            }
        }
    }

    /** A linha 1 do .progress e o passo; a 2, se houver, "signal=N" (libzdiag). */
    private fun progressStep(progress: File): String? = try {
        progress.readLines().firstOrNull()?.trim()?.ifEmpty { null }
    } catch (_: Exception) {
        null
    }

    private fun crashed(id: String, step: String?, elapsed: Long): JSONObject {
        val at = "$id/${step ?: "?"}"
        return JSONObject().put("id", id).put("status", "crash").put("durationMs", elapsed).put("crashedAt", at)
            .put("message", "o processo fechou em $at")
    }

    // ------------------------------------------------------------------ jogo, processo :game

    private fun gameReady(context: Context): Boolean = try {
        GameLib.ready(context) || run { GameLib.install(context) {}; GameLib.ready(context) }
    } catch (e: Exception) {
        false
    }

    private fun startGame(context: Context, id: String, cvars: List<String>, log: File, profile: File? = null) {
        context.startActivity(
            Intent(context, GameActivity::class.java).setAction("com.dfdx047.zandatsu.PLAY")
                .putExtra("diag", id)
                .putExtra("diag_cvars", (listOf("vibration = false") + cvars).joinToString("\n"))
                .putExtra("diag_log", log.absolutePath)
                .apply { if (profile != null) putExtra("diag_profile", profile.absolutePath) }
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
        )
    }

    private suspend fun runGameShaders(context: Context, dir: File, id: String): JSONObject {
        if (!gameReady(context)) return JSONObject().put("id", id).put("status", "skip").put("message", "jogo nao importado")
        val out = File(dir, "$id.json").apply { delete() }
        val log = File(dir, "logs/$id.log")
        val guard = File(AppPaths.appDir, "cache/renut_compiling.bin")
        waitGone(context, ":game", 5000)
        val serial = id.endsWith("serial")
        startGame(context, id, listOf(
            "swordman_renderer = renut",
            "swordman_diag_compile = ${if (serial) "serial" else "parallel"}",
            "swordman_diag_out = ${out.absolutePath}",
            "swordman_diag_compile_seconds = 40",
            "banjo_renut_pipeline_cache = false",
            "swordman_renut_serial_pipelines = ${if (serial) "true" else "false"}",
        ), log)
        val t0 = SystemClock.elapsedRealtime()
        var seen = false
        while (true) {
            delay(250)
            val pid = pidOf(context, ":game")
            if (pid != null) seen = true
            val elapsed = SystemClock.elapsedRealtime() - t0
            if (out.isFile) {
                waitGone(context, ":game", 5000)
                return JSONObject(out.readText())
            }
            if (pid == null && (seen || elapsed > 20_000)) {
                delay(500)
                if (out.isFile) return JSONObject(out.readText())
                return crashed(id, guardSlot(guard) ?: lastLogLine(log), elapsed)
            }
            if (elapsed > 150_000) {
                pid?.let(Process::killProcess)
                return JSONObject().put("id", id).put("status", "timeout").put("durationMs", elapsed)
                    .put("message", "sem resultado em 2,5 min")
            }
        }
    }

    /** O pipeline que o compile_guard (renut/0034) deixou a meio: 16 posicoes de 96 bytes. */
    private fun guardSlot(file: File): String? = try {
        val b = file.readBytes()
        (0 until b.size / 96).map { i -> b.copyOfRange(i * 96, i * 96 + 96) }
            .firstOrNull { it[0] == 'v'.code.toByte() }?.let { s -> String(s, Charsets.US_ASCII).substringBefore('\u0000') }
    } catch (_: Exception) {
        null
    }

    private fun lastLogLine(log: File): String? = try {
        log.readLines().lastOrNull { it.isNotBlank() }?.substringAfter("] ")?.take(120)
    } catch (_: Exception) {
        null
    }

    /**
     * 60 s no menu do titulo (uma cena 3D fixa, sem save nem toques). Primeiro no Nativo; se ele cair ou nao chegar
     * ao titulo, no Emulado. Le as linhas de desempenho que o jogo ja escreve no log, com o perfil de CPU ligado
     * (profiler.cpp: tempo de cada thread a correr, na fila e a dormir, nucleos, frequencias, pilhas).
     *
     * 04/11 (POCO X7 Pro, Mali-G720): a thread principal a 30 ms por frame com 14% de CPU. Quando o aparelho nao segura
     * os 30 fps do titulo, o teste repete o desempenho mudando uma coisa de cada vez: a thread principal solta
     * (swordman_renut_cores = 0), as duas nos nucleos grandes sem nucleo fixo (= 1) e sem ADPF.
     */
    private suspend fun runPerf(context: Context, dir: File): JSONObject {
        if (!gameReady(context)) return JSONObject().put("id", PERF).put("status", "skip").put("message", "jogo nao importado")
        val renut = perfOnce(context, dir, "renut", "base", emptyList(), 75_000)
        if (renut.optString("status") != "pass") {
            val xenos = perfOnce(context, dir, "xenos", "xenos", emptyList(), 75_000)
            xenos.put("fallbackFrom", "renut: ${renut.optString("message")}")
            return xenos
        }
        if (renut.optDouble("fpsAvg", 0.0) >= 29.5 && !options.contains("perf_variants=1")) return renut
        val variants = JSONArray()
        for ((label, cvars) in listOf(
            "nopin" to listOf("swordman_renut_cores = 0"),
            "bigcores" to listOf("swordman_renut_cores = 1"),
            "noadpf" to listOf("env:ZDIAG_NO_ADPF = 1"),
        )) {
            update(PERF, Status.RUNNING, "variante $label")
            val v = perfOnce(context, dir, "renut", label, cvars, 55_000)
            variants.put(JSONObject().put("variant", label).put("cvars", JSONArray(cvars)).put("status", v.optString("status"))
                .put("fpsAvg", v.opt("fpsAvg")).put("gpuMsAvg", v.opt("gpuMsAvg")).put("mainThreadMsAvg", v.opt("mainThreadMsAvg"))
                .put("renderThreadMsAvg", v.opt("renderThreadMsAvg")).put("pauses", v.opt("pauses")).put("message", v.optString("message")))
        }
        renut.put("variants", variants)
        val text = (0 until variants.length()).joinToString("; ") {
            val v = variants.getJSONObject(it)
            "${v.optString("variant")} ${v.opt("fpsAvg") ?: v.optString("status")}"
        }
        return renut.put("message", renut.optString("message") + " | " + text)
    }

    private suspend fun perfOnce(
        context: Context, dir: File, renderer: String, label: String, cvars: List<String>, measureMs: Long,
    ): JSONObject {
        val log = File(dir, "logs/perf_$label.log").apply { delete() }
        val prof = File(dir, "prof_$label").apply { deleteRecursively(); mkdirs() }
        waitGone(context, ":game", 5000)
        startGame(context, "perf_$label", listOf("swordman_renderer = $renderer") + cvars, log, prof)
        val t0 = SystemClock.elapsedRealtime()
        var titleAt = 0L
        var seen = false
        var goWritten = false
        val thermal = JSONArray()
        var nextThermal = 0L
        while (true) {
            delay(500)
            val pid = pidOf(context, ":game")
            if (pid != null) seen = true
            val now = SystemClock.elapsedRealtime()
            val elapsed = now - t0
            if (pid == null && (seen || elapsed > 20_000)) {
                return JSONObject().put("id", PERF).put("renderer", renderer).put("variant", label).put("status", "crash")
                    .put("crashedAt", "perf/$label/${if (titleAt == 0L) "arranque" else "titulo"}")
                    .put("message", "o jogo fechou (${elapsed / 1000} s)")
            }
            if (titleAt == 0L && logHas(log, "Title_Alpha")) titleAt = now
            if (titleAt == 0L && elapsed > 120_000) {
                pid?.let(Process::killProcess)
                return JSONObject().put("id", PERF).put("renderer", renderer).put("variant", label).put("status", "timeout")
                    .put("message", "nao chegou ao titulo em 2 min")
            }
            // O perfil agrega as pilhas a partir daqui (as mesmas janelas que o parsePerf conta).
            if (titleAt != 0L && !goWritten && now - titleAt > 10_000) {
                goWritten = true
                try { File(prof, "go").writeText(System.currentTimeMillis().toString()) } catch (_: Exception) {}
            }
            if (titleAt != 0L && now >= nextThermal) {
                nextThermal = now + 5000
                thermal.put(thermalNow(context))
            }
            if (titleAt != 0L && now - titleAt > measureMs) {
                pid?.let(Process::killProcess)
                waitGone(context, ":game", 5000)
                return parsePerf(log, renderer).put("variant", label).put("thermal", thermal)
            }
        }
    }

    private fun thermalNow(context: Context): JSONObject {
        val pm = context.getSystemService(PowerManager::class.java)
        val o = JSONObject().put("status", pm.currentThermalStatus)
        if (Build.VERSION.SDK_INT >= 30) o.put("headroom", pm.getThermalHeadroom(0).toDouble().takeIf { !it.isNaN() } ?: -1.0)
        return o
    }

    private fun logHas(log: File, text: String): Boolean = try {
        log.isFile && log.useLines { lines -> lines.any { text in it } }
    } catch (_: Exception) {
        false
    }

    /**
     * As janelas de desempenho que comecam 10 s depois do titulo (o titulo leva alguns segundos a carregar). Ficam de
     * fora as janelas que tocam numa pausa (o jogador abriu o menu do app: 04/11, POCO X7 Pro, 3 pausas no meio do
     * teste, uma janela de 19 s com um frame de 12,9 s) e as que nao tem o tamanho normal (10 s no Nativo, 5 s no Emulado).
     */
    internal fun parsePerf(log: File, renderer: String): JSONObject {
        val lines = log.readLines()
        val title = lines.indexOfFirst { "Title_Alpha" in it }
        val o = JSONObject().put("id", PERF).put("renderer", renderer).put("scene", "menu do titulo")
        val titleAt = lines.drop(title + 1).firstNotNullOfOrNull { timeOf(it) }
            ?: return o.put("status", "fail").put("message", "nao chegou ao titulo")
        val start = titleAt + 10_000
        // Pausas do menu do app: [inicio, fim].
        val pauses = ArrayList<LongArray>()
        for (l in lines) {
            val t = timeOf(l) ?: continue
            if ("swordman: pausa" in l) pauses += longArrayOf(t, Long.MAX_VALUE)
            else if ("swordman: retoma" in l) pauses.lastOrNull()?.takeIf { it[1] == Long.MAX_VALUE }?.set(1, t)
        }
        val pausesInTest = pauses.count { it[1] >= start }
        fun usable(end: Long, seconds: Double, nominal: Double): Boolean {
            if (seconds < nominal * 0.9 || seconds > nominal * 1.1) return false
            val begin = end - (seconds * 1000).toLong()
            return begin >= start && pauses.none { it[0] <= end && it[1] >= begin }
        }
        val fps = ArrayList<Double>()
        val gpu = ArrayList<Double>()
        val render = ArrayList<Double>()
        val main = ArrayList<Double>()
        var maxFrame = 0.0
        var dropped = 0
        val reRenut = Regex("""\[renut] ([\d.]+) s: \d+ game frames \(([\d.]+) fps\), render thread busy ([\d.]+) ms.*GPU ([\d.]+) ms a frame""")
        val reGame = Regex("""game thread, ([\d.]+) s: \d+ frames, ([\d.]+) ms a frame from Swap to Swap \(most ([\d.]+) ms\)""")
        val reXenia = Regex("""dbz3: perf fps=([\d.]+) .*window=([\d.]+)s max_frame_ms=([\d.]+) .*gpu_ms avg=([\d.]+)""")
        for (l in lines.drop(title + 1)) {
            val t = timeOf(l) ?: continue
            reRenut.find(l)?.let {
                val g = it.groupValues
                if (usable(t, g[1].toDouble(), 10.0)) { fps += g[2].toDouble(); render += g[3].toDouble(); gpu += g[4].toDouble() }
                else if (t >= start) dropped++
            }
            reGame.find(l)?.let {
                val g = it.groupValues
                if (usable(t, g[1].toDouble(), 10.0)) { main += g[2].toDouble(); maxFrame = maxOf(maxFrame, g[3].toDouble()) }
            }
            reXenia.find(l)?.let {
                val g = it.groupValues
                if (usable(t, g[2].toDouble(), 5.0)) {
                    fps += g[1].toDouble(); maxFrame = maxOf(maxFrame, g[3].toDouble()); gpu += g[4].toDouble()
                } else if (t >= start) dropped++
            }
        }
        o.put("pauses", pausesInTest).put("windowsDropped", dropped)
        val note = if (pausesInTest > 0) " (menu do app aberto ${pausesInTest}x: janelas descartadas)" else ""
        if (fps.isEmpty()) {
            return o.put("status", "fail")
                .put("message", if (pausesInTest > 0) "interrompido: o menu do app foi aberto ${pausesInTest}x" else "sem linhas de desempenho no log")
        }
        val avg = fps.average()
        o.put("status", "pass").put("seconds", if (renderer == "renut") fps.size * 10 else fps.size * 5)
            .put("fpsAvg", round(avg)).put("fpsMinWindow", round(fps.min())).put("frameMsAvg", round(1000.0 / avg))
            .put("frameMsMax", round(maxFrame)).put("gpuMsAvg", round(gpu.average()))
        if (main.isNotEmpty()) o.put("mainThreadMsAvg", round(main.average()))
        if (render.isNotEmpty()) o.put("renderThreadMsAvg", round(render.average()))
        return o.put("message", "${round(avg)} fps, GPU ${round(gpu.average())} ms ($renderer, ${fps.size} janelas)$note")
    }

    private fun round(v: Double) = Math.round(v * 10) / 10.0

    private var runStartedAt = 0L
    private val appLogTime = SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.ROOT)
    private val timeFormat = SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.ROOT)
    private fun timeOf(line: String): Long? =
        if (line.length > 25 && line[0] == '[') try { timeFormat.parse(line.substring(1, 24))?.time } catch (_: Exception) { null } else null

    // ------------------------------------------------------------------ resumo e zip

    private fun summary(context: Context, gpu: GpuFamily, dir: File, tests: JSONArray): JSONObject {
        val o = JSONObject().put("schema", 1)
        val iso = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ssXXX", Locale.ROOT).apply { timeZone = TimeZone.getDefault() }
        o.put("createdAt", iso.format(Date()))
        o.put("app", JSONObject().put("versionName", BuildConfig.VERSION_NAME).put("versionCode", BuildConfig.VERSION_CODE))
        o.put("device", JSONObject().put("manufacturer", Build.MANUFACTURER).put("model", Build.MODEL).put("device", Build.DEVICE)
            .put("soc", if (Build.VERSION.SDK_INT >= 31) Build.SOC_MODEL else DriverRepository.socName())
            .put("socManufacturer", if (Build.VERSION.SDK_INT >= 31) Build.SOC_MANUFACTURER else "")
            .put("android", Build.VERSION.RELEASE).put("api", Build.VERSION.SDK_INT))
        o.put("gpu", JSONObject().put("name", gpu.name).put("family", gpu.family).put("knownProblem", gpu.knownProblem))
        val cfg = try { GameConfig.load() } catch (_: Exception) { null }
        o.put("settings", JSONObject().put("renderer", if (cfg?.useRenut == true) "renut" else "xenos")
            .put("resolution", cfg?.resolution?.label ?: "").put("aspect", cfg?.aspect ?: "")
            .put("preset", cfg?.renutPreset ?: "").put("frameRate", cfg?.frameRate?.toString() ?: "")
            .put("config", cfg?.toString() ?: ""))
        val lib = DriverRepository.libraryIn(AppPaths.activeDriverDir)
        val kind = lib?.let { DriverRepository.kindOf(it.name) }
        o.put("driver", JSONObject().put("active", when (kind) {
            null -> "system"; DriverKind.TURNIP -> "turnip"; DriverKind.RADV -> "radv"; DriverKind.PANVK -> "panvk"; else -> "other"
        }).put("name", DriverRepository.activeDir() ?: ""))
        o.put("cpu", cpu(context))
        val am = context.getSystemService(ActivityManager::class.java)
        val mi = ActivityManager.MemoryInfo().also { am.getMemoryInfo(it) }
        o.put("memory", JSONObject().put("totalMB", mi.totalMem / 1048576).put("availMB", mi.availMem / 1048576).put("lowMemory", mi.lowMemory))
        o.put("thermal", thermal(context))
        // O resumo do Vulkan vem do teste info.
        val info = (0 until tests.length()).map { tests.getJSONObject(it) }.firstOrNull { it.optString("id") == "info" }
        val d = info?.optJSONObject("details")
        val dev = d?.optJSONArray("devices")?.optJSONObject(0)
        if (d != null) o.put("vulkan", JSONObject()
            .put("apiVersion", dev?.optString("apiVersion")).put("deviceName", d.optString("deviceName"))
            .put("vendorID", dev?.optString("vendorID")).put("deviceID", dev?.optString("deviceID"))
            .put("driverID", d.opt("driverID")).put("driverName", d.optString("driverName")).put("driverInfo", d.optString("driverInfo"))
            .put("driverVersion", dev?.optJSONObject("driverVersion")?.optString("hex"))
            .put("driverVersionDecoded", dev?.optJSONObject("driverVersion")?.optString("decoded"))
            .put("textureCompressionBC", d.opt("textureCompressionBC")).put("textureCompressionASTC_LDR", d.opt("textureCompressionASTC_LDR"))
            .put("textureCompressionETC2", d.opt("textureCompressionETC2"))
            .put("extensionsCount", dev?.optJSONObject("extensions")?.length() ?: 0))
        // Os testes sem os details (estao em tests/<id>.json).
        val slim = JSONArray()
        for (i in 0 until tests.length()) {
            val t = tests.getJSONObject(i)
            slim.put(JSONObject().put("id", t.optString("id")).put("status", t.optString("status"))
                .put("durationMs", t.opt("durationMs")).put("message", t.optString("message"))
                .apply { t.optString("crashedAt").takeIf { it.isNotEmpty() }?.let { put("crashedAt", it) } })
        }
        o.put("tests", slim)
        File(dir, "tests/perf.json").takeIf { it.isFile }?.let { o.put("perf", JSONObject(it.readText())) }
        try {
            File(dir, "aparelho.txt").writeText(LogExporter.deviceReport())
        } catch (_: Exception) {
        }
        return o
    }

    private fun cpu(context: Context): JSONObject {
        val cores = JSONArray()
        val byFreq = sortedMapOf<Long, MutableList<Int>>()
        for (c in 0 until Runtime.getRuntime().availableProcessors().coerceAtLeast(1).coerceAtMost(32)) {
            val base = "/sys/devices/system/cpu/cpu$c"
            val max = File("$base/cpufreq/cpuinfo_max_freq").takeIf { it.canRead() }?.readText()?.trim()?.toLongOrNull() ?: 0L
            val min = File("$base/cpufreq/cpuinfo_min_freq").takeIf { it.canRead() }?.readText()?.trim()?.toLongOrNull() ?: 0L
            val online = File("$base/online").takeIf { it.canRead() }?.readText()?.trim() != "0"
            cores.put(JSONObject().put("id", c).put("maxKHz", max).put("minKHz", min).put("online", online))
            byFreq.getOrPut(max) { mutableListOf() } += c
        }
        val clusters = JSONArray()
        byFreq.forEach { (f, list) -> clusters.put(JSONObject().put("cores", JSONArray(list)).put("maxKHz", f)) }
        val adpf = JSONObject()
        if (Build.VERSION.SDK_INT >= 31) {
            val rate = try { context.getSystemService(PerformanceHintManager::class.java)?.preferredUpdateRateNanos ?: -1 } catch (_: Throwable) { -1L }
            adpf.put("supported", rate >= 0).put("preferredRateNs", rate)
        } else adpf.put("supported", false)
        return JSONObject().put("cores", cores.length()).put("coreList", cores).put("clusters", clusters).put("adpf", adpf)
            .put("abi", Build.SUPPORTED_ABIS.joinToString(","))
    }

    private fun thermal(context: Context): JSONObject {
        val o = JSONObject()
        val pm = context.getSystemService(PowerManager::class.java)
        o.put("status", pm.currentThermalStatus)
        if (Build.VERSION.SDK_INT >= 30) o.put("headroom", pm.getThermalHeadroom(10).toDouble().takeIf { !it.isNaN() } ?: -1.0)
        val battery = context.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        battery?.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, Int.MIN_VALUE)?.takeIf { it != Int.MIN_VALUE }
            ?.let { o.put("batteryTempC", it / 10.0) }
        return o
    }

    /** summary.json, tests/, logs/ (os do teste, ate 2 MB cada, o fim), images/ (as leituras de volta), aparelho.txt. */
    private fun zip(dir: File, zip: File): List<Pair<String, Long>> {
        val entries = ArrayList<Pair<String, Long>>()
        ZipOutputStream(zip.outputStream().buffered()).use { z ->
            fun add(name: String, bytes: ByteArray) {
                z.putNextEntry(ZipEntry(name))
                z.write(bytes)
                z.closeEntry()
                entries += name to bytes.size.toLong()
            }
            File(dir, "summary.json").takeIf { it.isFile }?.let { add("summary.json", it.readBytes()) }
            File(dir, "aparelho.txt").takeIf { it.isFile }?.let { add("aparelho.txt", it.readBytes()) }
            File(dir, "tests").listFiles()?.sortedBy { it.name }?.forEach { add("tests/${it.name}", it.readBytes()) }
            File(dir, "logs").listFiles()?.sortedBy { it.name }?.forEach { f ->
                val b = f.readBytes()
                add("logs/${f.name}", if (b.size > 2_000_000) b.copyOfRange(b.size - 2_000_000, b.size) else b)
            }
            dir.listFiles()?.filter { it.name.endsWith(".png") }?.sortedBy { it.name }?.forEach { add("images/${it.name}", it.readBytes()) }
            // O perfil de CPU de cada corrida de desempenho (profiler.cpp).
            dir.listFiles()?.filter { it.isDirectory && it.name.startsWith("prof_") }?.sortedBy { it.name }?.forEach { d ->
                d.listFiles()?.filter { it.isFile && !it.name.endsWith(".tmp") && it.name != "go" }?.sortedBy { it.name }?.forEach { f ->
                    val b = f.readBytes()
                    add("logs/${d.name}/${f.name}", if (b.size > 4_000_000) b.copyOfRange(b.size - 4_000_000, b.size) else b)
                }
            }
            // O registo do app desde o inicio do teste (ADPF, launcher, diag).
            try {
                val since = runStartedAt
                // So as etiquetas do sistema (sem [game]/[import]: trazem o caminho da ISO).
                val tags = listOf("[adpf]", "[launcher]", "[driver]", "[diag]", "[crash]", "[app]")
                val lines = AppPaths.appLog.readLines().filter { l ->
                    l.length > 23 && tags.any { it in l } && (appLogTime.parse(l.substring(0, 23))?.time ?: 0L) >= since
                }
                if (lines.isNotEmpty()) add("logs/app.log", lines.joinToString("\n").toByteArray())
            } catch (_: Exception) {
            }
            // O que o compile_guard (renut/0034) sabe dos pipelines que derrubaram o driver.
            File(AppPaths.appDir, "cache/renut_bad_pipelines.txt").takeIf { it.isFile }?.let { add("logs/renut_bad_pipelines.txt", it.readBytes()) }
        }
        return entries
    }
}
