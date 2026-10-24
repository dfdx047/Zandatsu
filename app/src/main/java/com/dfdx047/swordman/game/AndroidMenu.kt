package com.dfdx047.swordman.game

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.view.KeyEvent
import android.view.MotionEvent
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.dfdx047.swordman.R
import com.dfdx047.swordman.data.AppLog
import com.dfdx047.swordman.data.ButtonBadge
import com.dfdx047.swordman.data.ControlStyle
import com.dfdx047.swordman.data.Directional
import com.dfdx047.swordman.data.DisplayRates
import com.dfdx047.swordman.data.FrameMode
import com.dfdx047.swordman.data.GameConfig
import com.dfdx047.swordman.data.InternalResolution
import com.dfdx047.swordman.data.LauncherPrefs
import com.dfdx047.swordman.data.RenutValidated
import com.dfdx047.swordman.data.TouchMode
import com.dfdx047.swordman.data.cvars
import com.dfdx047.swordman.data.frameMode
import com.dfdx047.swordman.data.withFrameMode
import com.dfdx047.swordman.ui.theme.LogoStyle
import kotlin.math.abs
import kotlin.math.roundToInt

/*
 * Menu do jogo do Project SwordMan (Android). Abre-se com o Back (o botao do Odin e de outros
 * portateis, o botao/gesto de voltar do telemovel), com Back + Start no comando, com o botao
 * de pausa ou com a engrenagem por cima do jogo, e usa-se igual por toque e por comando: LB/RB mudam de aba, cima/baixo escolhem a linha, esquerda/direita mudam o
 * valor, A escolhe, B fecha. Cada mudanca vai logo para o jogo (cvars ao vivo) e fica no
 * mgr.toml; as que so valem depois de reiniciar dizem-no.
 */

// ------------------------------------------------------------------ linhas

sealed class MenuItem {
    abstract val label: String
    open val help: String? = null
    open val restart: Boolean = false
    val selectable: Boolean get() = this !is MHeader && this !is MInfo
}

class MHeader(override val label: String) : MenuItem()
class MInfo(override val label: String) : MenuItem()

class MToggle(
    override val label: String,
    override val help: String?,
    val value: Boolean,
    override val restart: Boolean = false,
    val onSet: (Boolean) -> Unit,
) : MenuItem()

class MChoice(
    override val label: String,
    override val help: String?,
    val options: List<String>,
    val index: Int,
    override val restart: Boolean = false,
    val onSet: (Int) -> Unit,
) : MenuItem()

/** Valor numerico em passos (esquerda/direita), mostrado ja formatado. */
class MStep(
    override val label: String,
    override val help: String?,
    val text: String,
    val onStep: (Int) -> Unit,
) : MenuItem()

class MAction(
    override val label: String,
    override val help: String?,
    val actionText: String,
    val danger: Boolean = false,
    val run: () -> Unit,
) : MenuItem()

// --------------------------------------------------------------- navegacao

/** Estado da navegacao (aba, linha) e o que fazem as teclas do comando. */
class MenuController {
    var tab by mutableIntStateOf(0)
    var row by mutableIntStateOf(-1)
    var tabCount = 1
    var items: List<MenuItem> = emptyList()
    var onClose: () -> Unit = {}

    private val main = Handler(Looper.getMainLooper())
    private var heldDir = 0
    private val repeat = object : Runnable {
        override fun run() {
            if (heldDir == 0) return
            direction(heldDir)
            main.postDelayed(this, REPEAT_MS)
        }
    }

    fun sound(s: Int) = NativeGameState.menuSound(s)

    fun open(tabIndex: Int) {
        tab = tabIndex.coerceIn(0, (tabCount - 1).coerceAtLeast(0))
        row = -1
        sound(NativeGameState.SOUND_OPEN)
    }

    fun ensureRow() {
        if (items.getOrNull(row)?.selectable != true) {
            row = items.indexOfFirst { it.selectable }
        }
    }

    fun selectTab(i: Int) {
        if (i == tab) return
        tab = i
        row = -1
        sound(NativeGameState.SOUND_MOVE)
    }

    private fun changeTab(d: Int) = selectTab((tab + d + tabCount) % tabCount)

    private fun move(d: Int) {
        var i = row
        while (true) {
            i += d
            if (i < 0 || i >= items.size) return
            if (items[i].selectable) {
                row = i
                sound(NativeGameState.SOUND_MOVE)
                return
            }
        }
    }

    fun adjust(d: Int) {
        when (val it = items.getOrNull(row)) {
            is MToggle -> it.onSet(!it.value)
            is MChoice -> if (it.options.isNotEmpty()) it.onSet((it.index + d + it.options.size) % it.options.size)
            is MStep -> it.onStep(d)
            else -> return
        }
        sound(NativeGameState.SOUND_CONFIRM)
    }

    fun activate() {
        when (val it = items.getOrNull(row)) {
            is MAction -> {
                sound(NativeGameState.SOUND_CONFIRM)
                it.run()
            }
            is MToggle, is MChoice -> adjust(1)
            else -> Unit
        }
    }

    private fun direction(d: Int) {
        when (d) {
            DIR_UP -> move(-1)
            DIR_DOWN -> move(1)
            DIR_LEFT -> adjust(-1)
            DIR_RIGHT -> adjust(1)
        }
    }

    /** Tecla premida com o menu aberto. Devolve true se a consumiu. */
    fun onKeyDown(code: Int): Boolean {
        when (code) {
            KeyEvent.KEYCODE_DPAD_UP -> direction(DIR_UP)
            KeyEvent.KEYCODE_DPAD_DOWN -> direction(DIR_DOWN)
            KeyEvent.KEYCODE_DPAD_LEFT -> direction(DIR_LEFT)
            KeyEvent.KEYCODE_DPAD_RIGHT -> direction(DIR_RIGHT)
            KeyEvent.KEYCODE_BUTTON_L1, KeyEvent.KEYCODE_BUTTON_L2 -> changeTab(-1)
            KeyEvent.KEYCODE_BUTTON_R1, KeyEvent.KEYCODE_BUTTON_R2 -> changeTab(1)
            KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.KEYCODE_ENTER -> activate()
            KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BUTTON_START -> close()
            else -> return KeyEvent.isGamepadButton(code)
        }
        return true
    }

    fun close() {
        stopRepeat()
        sound(NativeGameState.SOUND_CLOSE)
        onClose()
    }

    /** Analogico esquerdo / cruz (eixos HAT): com repeticao enquanto se segura. */
    fun onMotion(ev: MotionEvent) {
        val hx = ev.getAxisValue(MotionEvent.AXIS_HAT_X)
        val hy = ev.getAxisValue(MotionEvent.AXIS_HAT_Y)
        val sx = ev.getAxisValue(MotionEvent.AXIS_X)
        val sy = ev.getAxisValue(MotionEvent.AXIS_Y)
        val x = if (abs(hx) > 0.5f) hx else sx
        val y = if (abs(hy) > 0.5f) hy else sy
        val d = when {
            abs(y) >= 0.6f && abs(y) >= abs(x) -> if (y < 0) DIR_UP else DIR_DOWN
            abs(x) >= 0.6f -> if (x < 0) DIR_LEFT else DIR_RIGHT
            else -> 0
        }
        if (d == heldDir) return
        stopRepeat()
        heldDir = d
        if (d != 0) {
            direction(d)
            main.postDelayed(repeat, FIRST_REPEAT_MS)
        }
    }

    fun stopRepeat() {
        heldDir = 0
        main.removeCallbacks(repeat)
    }

    private companion object {
        const val DIR_UP = 1
        const val DIR_DOWN = 2
        const val DIR_LEFT = 3
        const val DIR_RIGHT = 4
        const val FIRST_REPEAT_MS = 380L
        const val REPEAT_MS = 120L
    }
}

// ------------------------------------------------------------------ opcoes

/** O que o menu mostra e muda: as opcoes do jogo (cvars) e as dos controles por toque. */
class MenuModel(private val context: Context, private val prefs: LauncherPrefs, private val actions: InGameActions) {
    private var cfg by mutableStateOf(GameConfig.load())
    /** Cvars ao vivo que o launcher nao guarda (posicao do contador de FPS). */
    private val live = mutableStateMapOf<String, String>()
    private var touchMode by mutableStateOf(prefs.touchMode)
    private var directional by mutableStateOf(prefs.directional)
    private var style by mutableStateOf(prefs.controlStyle)
    private var badge by mutableStateOf(prefs.buttonBadge)
    private var opacity by mutableFloatStateOf(prefs.touchOpacity)
    private var haptics by mutableStateOf(prefs.touchHaptics)
    private var pauseMenus by mutableStateOf(prefs.pauseOnMenus)
    private var confirmExit by mutableStateOf(false)

    private fun s(id: Int) = context.getString(id)
    private fun s(id: Int, vararg args: Any) = context.getString(id, *args)

    /** Ao abrir: le o que esta no jogo agora. */
    fun refresh() {
        cfg = GameConfig.load()
        for (k in LIVE_CVARS) live[k] = NativeGameState.getCvar(k)
        touchMode = prefs.touchMode
        directional = prefs.directional
        style = prefs.controlStyle
        badge = prefs.buttonBadge
        opacity = prefs.touchOpacity
        haptics = prefs.touchHaptics
        pauseMenus = prefs.pauseOnMenus
        confirmExit = false
        selfTestRefused = false
        AppLog.i("menu", "aberto")
    }

    private fun set(next: GameConfig, cvars: Map<String, String>) {
        cfg = next
        if (cvars.isNotEmpty()) {
            val ok = NativeGameState.setCvars(cvars)
            AppLog.i("menu", "cvars ${cvars.entries.joinToString { "${it.key}=${it.value}" }}" + if (ok) "" else " (falhou)")
        }
    }

    /** Opcoes so do app (volume, modo seguro): ficam no mgr.toml para o launcher. */
    private fun saveAppOnly(next: GameConfig) {
        cfg = next
        Thread { GameConfig.save(next) }.start()
    }

    private fun setLive(name: String, value: String) {
        live[name] = value
        NativeGameState.setCvars(mapOf(name to value))
    }

    private fun appearance() = actions.applyAppearance()

    val tabs: List<String>
        get() = listOf(
            s(R.string.menu_tab_pause),
            s(R.string.menu_tab_video),
            s(R.string.perf_effects_title),
            s(R.string.tab_controls),
            s(R.string.tab_debug),
            s(R.string.menu_tab_system),
        )

    /** Indice da aba dos controles (a engrenagem abre nela). */
    val controlsTab = 3

    fun items(tab: Int): List<MenuItem> = when (tab) {
        0 -> pauseItems()
        1 -> videoItems()
        2 -> effectItems()
        3 -> controlItems()
        4 -> debugItems()
        else -> systemItems()
    }

    private fun <T> choice(
        label: String, help: String?, options: List<Pair<T, String>>, current: T,
        restart: Boolean = false, onSet: (T) -> Unit,
    ): MChoice {
        val i = options.indexOfFirst { it.first == current }.coerceAtLeast(0)
        return MChoice(label, help, options.map { it.second }, i, restart) { k -> onSet(options[k].first) }
    }

    private fun exitItem() = MAction(
        s(R.string.ingame_exit),
        s(R.string.menu_exit_help),
        if (confirmExit) s(R.string.menu_exit_confirm_action) else s(R.string.menu_exit_action),
        danger = true,
    ) {
        if (confirmExit) actions.exitToLauncher() else confirmExit = true
    }

    private fun pauseItems(): List<MenuItem> = listOf(
        MAction(s(R.string.ingame_resume), null, s(R.string.menu_go)) { actions.resume() },
        MAction(s(R.string.settings_touch_edit), s(R.string.menu_edit_help), s(R.string.menu_open)) { actions.openEditor() },
        MHeader(s(R.string.menu_session)),
        exitItem(),
    )

    private fun pct(x: Double) = "${(x * 100).roundToInt()}%"

    private fun videoItems(): List<MenuItem> {
        val c = cfg
        val list = ArrayList<MenuItem>()
        if (c.useRenut) return renutVideoItems(c)
        list += choice(
            s(R.string.settings_resolution), s(R.string.perf_resolution_sub),
            InternalResolution.entries.map { it to it.label }, c.resolution, restart = true,
        ) { v ->
            set(
                c.copy(resolution = v),
                mapOf(
                    "draw_resolution_scale_x" to "${v.scale}",
                    "draw_resolution_scale_y" to "${v.scale}",
                    "draw_resolution_percent" to "${v.percent}",
                ),
            )
        }
        // 30 = hack (jogo a 60, 1 frame desenhado em 2); 120 interpolado so com tela de 120 Hz.
        val has120 = DisplayRates.supports120(context)
        val modes = buildList {
            add(FrameMode.FPS30 to s(R.string.perf_fps_30_hack))
            add(FrameMode.FPS60 to "60 FPS")
            if (has120) add(FrameMode.FPS120_INTERP to s(R.string.perf_fps_120_interp))
        }
        list += choice(
            s(R.string.perf_frame_rate), s(R.string.perf_frame_rate_sub), modes, c.frameMode,
        ) { v ->
            set(c.withFrameMode(v), v.cvars())
            actions.setDisplayRate(v.outputHz)
        }
        when (c.frameMode) {
            FrameMode.FPS30 -> list += MInfo(s(R.string.perf_fps_30_note))
            FrameMode.FPS120_INTERP -> list += MInfo(s(R.string.perf_fps_120_interp_note))
            else -> Unit
        }
        list += MToggle(s(R.string.settings_show_fps), null, c.showFps) { v ->
            set(c.copy(showFps = v), mapOf("debug_overlay" to v.toString()))
        }
        if (c.showFps) {
            list += choice(
                s(R.string.menu_fps_position), null,
                listOf(
                    "top-left" to s(R.string.menu_pos_top_left),
                    "top-right" to s(R.string.menu_pos_top_right),
                    "bottom-left" to s(R.string.menu_pos_bottom_left),
                    "bottom-right" to s(R.string.menu_pos_bottom_right),
                ),
                live["debug_overlay_position"]?.ifEmpty { null } ?: "top-left",
            ) { v -> setLive("debug_overlay_position", v) }
        }
        list += choice(
            s(R.string.settings_aspect_mode), null,
            listOf(true to s(R.string.aspect_original), false to s(R.string.aspect_stretch)), c.letterbox,
        ) { v -> set(c.copy(letterbox = v), mapOf("present_letterbox" to v.toString())) }
        list += MHeader(s(R.string.settings_filter))
        list += choice(
            s(R.string.settings_filter), s(R.string.settings_filter_sub),
            listOf("bilinear" to "Bilinear", "cas" to "AMD CAS", "fsr" to "AMD FSR 1"), c.presentEffect,
        ) { v -> set(c.copy(presentEffect = v), mapOf("present_effect" to v)) }
        if (c.presentEffect == "fsr") {
            list += choice(
                s(R.string.settings_fsr_mode), s(R.string.settings_fsr_mode_sub),
                listOf(
                    "auto" to s(R.string.fsr_native),
                    "quality" to s(R.string.fsr_quality),
                    "balanced" to s(R.string.fsr_balanced),
                    "performance" to s(R.string.fsr_performance),
                    "ultra_performance" to s(R.string.fsr_ultra),
                ),
                c.fsrQuality,
            ) { v -> set(c.copy(fsrQuality = v), mapOf("present_fsr_quality_mode" to v)) }
            val sharp = ((2.0 - c.fsrSharpness) / 2.0).coerceIn(0.0, 1.0)
            list += MStep(s(R.string.settings_fsr_sharpness), null, pct(sharp)) { d ->
                val next = (sharp + d * 0.1).coerceIn(0.0, 1.0)
                val red = 2.0 - next * 2.0
                set(c.copy(fsrSharpness = red), mapOf("present_fsr_sharpness_reduction" to num(red)))
            }
        }
        if (c.presentEffect == "cas") {
            list += MStep(s(R.string.settings_cas_sharpness), null, pct(c.casSharpness)) { d ->
                val next = (c.casSharpness + d * 0.1).coerceIn(0.0, 1.0)
                set(c.copy(casSharpness = next), mapOf("present_cas_additional_sharpness" to num(next)))
            }
        }
        list += choice(
            s(R.string.settings_aa), null,
            listOf("none" to s(R.string.off), "fxaa" to "FXAA", "fxaa_extreme" to "FXAA Extreme"), c.fxaa,
        ) { v -> set(c.copy(fxaa = v), mapOf("swap_post_effect" to v)) }
        list += choice(
            s(R.string.settings_aniso), null,
            listOf(-1 to s(R.string.aniso_game), 1 to "1x", 2 to "2x", 3 to "4x", 4 to "8x", 5 to "16x"), c.anisotropic,
        ) { v -> set(c.copy(anisotropic = v), mapOf("anisotropic_override" to "$v")) }
        return list
    }

    /** Renut: resolucao (540p/720p), FPS na tela e anisotropico; o resto e do Xenia. */
    private fun renutVideoItems(c: GameConfig): List<MenuItem> {
        val list = ArrayList<MenuItem>()
        list += choice(
            s(R.string.settings_resolution), s(R.string.renut_resolution_sub),
            listOf(InternalResolution.P540, InternalResolution.P720).map { it to it.label },
            if (c.resolution == InternalResolution.P540) InternalResolution.P540 else InternalResolution.P720,
        ) { v ->
            val next = c.copy(resolution = v)
            set(next, mapOf("sa2_render_height_handheld" to "${next.renutHeight}"))
        }
        list += MInfo(s(R.string.renut_fps_note))
        list += MToggle(s(R.string.settings_show_fps), null, c.showFps) { v ->
            set(c.copy(showFps = v), mapOf("debug_overlay" to v.toString()))
        }
        list += choice(
            s(R.string.settings_aniso), null,
            listOf(-1 to s(R.string.aniso_game), 2 to "2x", 3 to "4x", 4 to "8x", 5 to "16x"),
            if (c.anisotropic <= 1) -1 else c.anisotropic,
        ) { v ->
            val next = c.copy(anisotropic = v)
            set(next, mapOf("renut_anisotropic_filtering" to next.toToml().getValue("renut_anisotropic_filtering").toString()))
        }
        return list
    }

    /** Som e efeitos do jogo: no Renut as sombras e o brilho, ao vivo. */
    private fun effectItems(): List<MenuItem> {
        val c = cfg
        if (c.useRenut) return listOf(
            MStep(s(R.string.settings_volume), null, "${c.volume}%") { d ->
                val next = (c.volume + d * 10).coerceIn(0, 100)
                NativeGameState.setVolume(next)
                saveAppOnly(c.copy(volume = next))
            },
            MToggle(s(R.string.settings_vibration), null, c.vibration) { v ->
                set(c.copy(vibration = v), mapOf("vibration" to v.toString()))
            },
        ) + listOfNotNull(
            // So as opcoes validadas no aparelho (RenutValidated).
            MHeader(s(R.string.perf_effects_title)).takeIf { RenutValidated.PRESETS },
            choice(
                s(R.string.q_shadows), s(R.string.renut_shadows_sub),
                listOf(true to s(R.string.q_high), false to s(R.string.q_off)), c.renutShadows,
            ) { v -> set(c.copy(renutShadows = v), mapOf("swordman_renut_shadows" to v.toString())) }
                .takeIf { RenutValidated.SHADOWS },
            choice(
                s(R.string.renut_bloom), s(R.string.renut_bloom_sub),
                listOf(true to s(R.string.on), false to s(R.string.off)), c.renutBloom,
            ) { v -> set(c.copy(renutBloom = v), mapOf("swordman_renut_bloom" to v.toString())) }
                .takeIf { RenutValidated.BLOOM },
        )
        return listOf(
            MStep(s(R.string.settings_volume), null, "${c.volume}%") { d ->
                val next = (c.volume + d * 10).coerceIn(0, 100)
                NativeGameState.setVolume(next)
                saveAppOnly(c.copy(volume = next))
            },
            MToggle(s(R.string.settings_vibration), null, c.vibration) { v ->
                set(c.copy(vibration = v), mapOf("vibration" to v.toString()))
            },
        )
    }

    private fun controlItems(): List<MenuItem> {
        val list = ArrayList<MenuItem>()
        list += MAction(s(R.string.settings_touch_edit), s(R.string.menu_edit_help), s(R.string.menu_open)) { actions.openEditor() }
        list += choice(
            s(R.string.settings_touch), s(R.string.settings_touch_mode_sub),
            listOf(
                TouchMode.AUTO to s(R.string.touch_mode_auto),
                TouchMode.ON to s(R.string.touch_mode_on),
                TouchMode.OFF to s(R.string.touch_mode_off),
            ),
            touchMode,
        ) { v ->
            touchMode = v
            prefs.touchMode = v
            AppLog.i("menu", "controles na tela: ${v.key}")
            appearance()
        }
        if (touchMode == TouchMode.OFF) list += MInfo(s(R.string.menu_touch_off_note))
        list += choice(
            s(R.string.ingame_directional), s(R.string.settings_directional_sub),
            listOf(Directional.STICK to s(R.string.directional_stick), Directional.DPAD to s(R.string.directional_dpad)),
            directional,
        ) { v ->
            directional = v
            prefs.directional = v
            appearance()
        }
        list += choice(
            s(R.string.settings_touch_style), null,
            listOf(
                ControlStyle.GAME to "MGR",
                ControlStyle.XBOX to "Xbox",
                ControlStyle.PLAYSTATION to "PlayStation",
                ControlStyle.NINTENDO to "Nintendo",
            ),
            style,
        ) { v ->
            style = v
            prefs.controlStyle = v
            appearance()
        }
        if (style == ControlStyle.GAME) {
            list += choice(
                s(R.string.ingame_badge), null,
                listOf(
                    ButtonBadge.XBOX to "Xbox",
                    ButtonBadge.PLAYSTATION to "PlayStation",
                    ButtonBadge.NINTENDO to "Nintendo",
                    ButtonBadge.NONE to s(R.string.badge_none),
                ),
                badge,
            ) { v ->
                badge = v
                prefs.buttonBadge = v
                appearance()
            }
        }
        list += MStep(s(R.string.settings_touch_opacity), null, "${(opacity * 100).roundToInt()}%") { d ->
            val next = ((opacity * 10).roundToInt() + d).coerceIn(1, 10) / 10f
            opacity = next
            prefs.touchOpacity = next
            appearance()
        }
        list += MToggle(s(R.string.settings_touch_haptics), null, haptics) { v ->
            haptics = v
            prefs.touchHaptics = v
            appearance()
        }
        list += MToggle(s(R.string.settings_pause_menus), s(R.string.settings_pause_menus_sub), pauseMenus) { v ->
            pauseMenus = v
            prefs.pauseOnMenus = v
            appearance()
        }
        val c = cfg
        list += MStep(s(R.string.settings_deadzone), null, pct(c.deadzone)) { d ->
            val next = (c.deadzone + d * 0.05).coerceIn(0.0, 0.5)
            set(
                c.copy(deadzone = next),
                mapOf("left_stick_deadzone_percentage" to num(next), "right_stick_deadzone_percentage" to num(next)),
            )
        }
        return list
    }

    private var selfTestRefused by mutableStateOf(false)
    private var exportStatus by mutableStateOf<String?>(null)

    private fun debugItems(): List<MenuItem> {
        val c = cfg
        return listOfNotNull(
            MAction(s(R.string.menu_selftest), s(R.string.menu_selftest_help), s(R.string.menu_go)) {
                selfTestRefused = !actions.startSelfTest()
            },
            if (selfTestRefused) MInfo(s(R.string.menu_selftest_refused)) else null,
            MAction(s(R.string.ingame_capture), s(R.string.menu_capture_help), s(R.string.menu_go)) {
                actions.captureFrames()
                actions.resume()
            },
            MToggle(s(R.string.settings_verbose_log), s(R.string.settings_verbose_log_sub), c.verboseLog) { v ->
                set(c.copy(verboseLog = v), mapOf("log_level" to if (v) "debug" else "info"))
            },
            MToggle(s(R.string.settings_safe_gpu), s(R.string.settings_safe_gpu_sub), c.safeGpu, restart = true) { v ->
                saveAppOnly(c.copy(safeGpu = v))
            },
            MInfo(s(R.string.port_menu_restart_note)),
        )
    }

    private fun systemItems(): List<MenuItem> = listOfNotNull(
        MAction(s(R.string.export_logs), s(R.string.menu_export_logs_help), s(R.string.menu_go)) {
            exportStatus = s(R.string.menu_exporting)
            Thread {
                val name = try {
                    com.dfdx047.swordman.data.LogExporter.export(context)
                } catch (e: Exception) {
                    AppLog.e("menu", "exportar logs falhou", e)
                    null
                }
                exportStatus = if (name != null) "Download/$name" else s(R.string.menu_save_failed)
            }.start()
        },
        exportStatus?.let { MInfo(it) },
        MInfo(s(R.string.err_dialog_discord_help)),
        MHeader(s(R.string.menu_session)),
        exitItem(),
    )

    private fun num(x: Double): String = String.format(java.util.Locale.ROOT, "%.4f", x)

    private companion object {
        val LIVE_CVARS = listOf("debug_overlay_position")
    }
}

// --------------------------------------------------------------------- ecra

@Composable
fun AndroidGameMenu(model: MenuModel, controller: MenuController) {
    val tabs = model.tabs
    controller.tabCount = tabs.size
    val items = model.items(controller.tab)
    SideEffect {
        controller.items = items
        controller.ensureRow()
    }
    val shape = RoundedCornerShape(22.dp)
    val listState = rememberLazyListState()
    LaunchedEffect(controller.tab, controller.row) {
        if (controller.row >= 0) listState.animateScrollToItem((controller.row - 2).coerceAtLeast(0))
    }

    Box(
        Modifier
            .fillMaxSize()
            .background(Color.Black.copy(alpha = 0.55f)),
        contentAlignment = Alignment.Center,
    ) {
        Column(
            Modifier
                .padding(12.dp)
                .widthIn(max = 780.dp)
                .fillMaxHeight(0.94f)
                .clip(shape)
                .background(MaterialTheme.colorScheme.surface.copy(alpha = 0.97f))
                .border(1.dp, MaterialTheme.colorScheme.outlineVariant, shape),
        ) {
            Row(
                Modifier
                    .fillMaxWidth()
                    .background(MaterialTheme.colorScheme.surfaceContainer)
                    .padding(horizontal = 18.dp, vertical = 10.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Box(Modifier.size(width = 3.dp, height = 18.dp).background(MaterialTheme.colorScheme.primary))
                Spacer(Modifier.width(10.dp))
                Text(
                    "PROJECT SWORDMAN",
                    style = LogoStyle.copy(fontSize = 16.sp),
                    color = MaterialTheme.colorScheme.onSurface,
                )
                Spacer(Modifier.weight(1f))
                Text(
                    androidx.compose.ui.res.stringResource(R.string.ingame_paused),
                    style = MaterialTheme.typography.labelMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Row(
                Modifier
                    .fillMaxWidth()
                    .horizontalScroll(rememberScrollState())
                    .padding(horizontal = 12.dp, vertical = 8.dp),
                horizontalArrangement = Arrangement.spacedBy(6.dp),
            ) {
                tabs.forEachIndexed { i, t ->
                    val on = i == controller.tab
                    Box(
                        Modifier
                            .clip(RoundedCornerShape(8.dp))
                            .background(if (on) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant)
                            .clickable { controller.selectTab(i) }
                            .padding(horizontal = 14.dp, vertical = 7.dp),
                    ) {
                        Text(
                            t,
                            style = MaterialTheme.typography.labelLarge,
                            color = if (on) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurfaceVariant,
                            maxLines = 1,
                        )
                    }
                }
            }
            LazyColumn(
                Modifier
                    .weight(1f)
                    .fillMaxWidth(),
                state = listState,
                contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            ) {
                itemsIndexed(items) { i, item ->
                    MenuRow(
                        item,
                        selected = i == controller.row,
                        onClick = {
                            controller.row = i
                            controller.activate()
                        },
                        onAdjust = { d ->
                            controller.row = i
                            controller.adjust(d)
                        },
                    )
                }
            }
            val selected = items.getOrNull(controller.row)
            Column(
                Modifier
                    .fillMaxWidth()
                    .background(MaterialTheme.colorScheme.surfaceContainer)
                    .padding(horizontal = 18.dp, vertical = 8.dp),
            ) {
                val help = listOfNotNull(
                    selected?.help,
                    if (selected?.restart == true) androidx.compose.ui.res.stringResource(R.string.menu_restart_needed) else null,
                ).joinToString(" ")
                Text(
                    help,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.height(34.dp),
                )
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        androidx.compose.ui.res.stringResource(R.string.menu_hints),
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.secondary,
                        modifier = Modifier.weight(1f),
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                    Box(
                        Modifier
                            .clip(RoundedCornerShape(8.dp))
                            .background(MaterialTheme.colorScheme.primary)
                            .clickable { controller.close() }
                            .padding(horizontal = 14.dp, vertical = 6.dp),
                    ) {
                        Text(
                            androidx.compose.ui.res.stringResource(R.string.ingame_resume),
                            style = MaterialTheme.typography.labelLarge,
                            color = MaterialTheme.colorScheme.onPrimary,
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun MenuRow(item: MenuItem, selected: Boolean, onClick: () -> Unit, onAdjust: (Int) -> Unit) {
    when (item) {
        is MHeader -> Text(
            item.label,
            style = MaterialTheme.typography.titleSmall,
            color = MaterialTheme.colorScheme.primary,
            modifier = Modifier.padding(start = 8.dp, top = 12.dp, bottom = 4.dp),
        )
        is MInfo -> Text(
            item.label,
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(horizontal = 8.dp, vertical = 6.dp),
        )
        else -> {
            val scheme = MaterialTheme.colorScheme
            val bg = if (selected) {
                Brush.horizontalGradient(listOf(scheme.primary.copy(alpha = 0.35f), scheme.primary.copy(alpha = 0.05f)))
            } else {
                Brush.horizontalGradient(listOf(scheme.surfaceVariant.copy(alpha = 0.5f), Color.Transparent))
            }
            Row(
                Modifier
                    .fillMaxWidth()
                    .padding(vertical = 2.dp)
                    .clip(RoundedCornerShape(10.dp))
                    .background(bg)
                    .clickable(onClick = onClick)
                    .padding(horizontal = 14.dp, vertical = 9.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Column(Modifier.weight(1f).padding(end = 10.dp)) {
                    Text(item.label, style = MaterialTheme.typography.bodyLarge, color = scheme.onSurface)
                    if (item.restart) {
                        Text(
                            androidx.compose.ui.res.stringResource(R.string.menu_restart_badge),
                            style = MaterialTheme.typography.labelSmall,
                            color = scheme.tertiary,
                        )
                    }
                }
                when (item) {
                    is MToggle -> TogglePill(item.value)
                    is MChoice -> Stepper(item.options.getOrElse(item.index) { "-" }, onAdjust)
                    is MStep -> Stepper(item.text, onAdjust)
                    is MAction -> Text(
                        item.actionText,
                        style = MaterialTheme.typography.labelLarge,
                        color = if (item.danger) scheme.error else scheme.primary,
                    )
                    else -> Unit
                }
            }
        }
    }
}

@Composable
private fun TogglePill(on: Boolean) {
    Box(
        Modifier
            .size(width = 48.dp, height = 26.dp)
            .clip(RoundedCornerShape(50))
            .background(if (on) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant)
            .border(1.dp, MaterialTheme.colorScheme.outline, RoundedCornerShape(50))
            .padding(3.dp),
        contentAlignment = if (on) Alignment.CenterEnd else Alignment.CenterStart,
    ) {
        Box(
            Modifier
                .size(20.dp)
                .clip(CircleShape)
                .background(if (on) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurfaceVariant),
        )
    }
}

@Composable
private fun Stepper(value: String, onAdjust: (Int) -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Arrow("◀") { onAdjust(-1) }
        Text(
            value,
            style = MaterialTheme.typography.labelLarge,
            color = MaterialTheme.colorScheme.primary,
            textAlign = TextAlign.Center,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.widthIn(min = 92.dp, max = 220.dp).padding(horizontal = 6.dp),
        )
        Arrow("▶") { onAdjust(1) }
    }
}

@Composable
private fun Arrow(glyph: String, onClick: () -> Unit) {
    Box(
        Modifier
            .size(32.dp)
            .clip(CircleShape)
            .background(MaterialTheme.colorScheme.surfaceVariant)
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        Text(glyph, color = MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.labelLarge)
    }
}
