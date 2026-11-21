package com.dfdx047.zandatsu.pc.game

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.view.KeyEvent
import android.view.MotionEvent
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.MutableTransitionState
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.slideInHorizontally
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowLeft
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.SportsEsports
import androidx.compose.material.icons.filled.TouchApp
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.runtime.remember
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.dfdx047.zandatsu.pc.R
import com.dfdx047.zandatsu.pc.data.ButtonBadge
import com.dfdx047.zandatsu.pc.data.ControlStyle
import com.dfdx047.zandatsu.pc.data.Directional
import com.dfdx047.zandatsu.pc.data.LauncherPrefs
import com.dfdx047.zandatsu.pc.data.PcTouchMode
import com.dfdx047.zandatsu.pc.ui.theme.LogoStyle
import kotlin.math.abs
import kotlin.math.roundToInt

/*
 * Menu do jogo de PC: o mesmo painel lateral da app do 360 (AndroidMenu.kt), sem as opcoes do 360 (cvars, video,
 * Renut). Abre-se com o Back, com Back + Start no comando ou com o botao por cima do jogo; usa-se igual por toque e por
 * comando: LB/RB mudam de aba, cima/baixo escolhem a linha, esquerda/direita mudam o valor, A escolhe, B fecha.
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
    /** Comando fisico ligado: o painel mostra as teclas (LB/RB, A, B). */
    var padConnected by mutableStateOf(false)

    private val main = Handler(Looper.getMainLooper())
    private var heldDir = 0
    private val repeat = object : Runnable {
        override fun run() {
            if (heldDir == 0) return
            direction(heldDir)
            main.postDelayed(this, REPEAT_MS)
        }
    }

    /** O jogo de PC nao tem a ponte dos sons do menu do 360. */
    fun sound(@Suppress("UNUSED_PARAMETER") s: Int) {}

    fun open(tabIndex: Int) {
        tab = tabIndex.coerceIn(0, (tabCount - 1).coerceAtLeast(0))
        row = -1
        sound(SOUND_OPEN)
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
        sound(SOUND_MOVE)
    }

    private fun changeTab(d: Int) = selectTab((tab + d + tabCount) % tabCount)

    private fun move(d: Int) {
        var i = row
        while (true) {
            i += d
            if (i < 0 || i >= items.size) return
            if (items[i].selectable) {
                row = i
                sound(SOUND_MOVE)
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
        sound(SOUND_CONFIRM)
    }

    fun activate() {
        when (val it = items.getOrNull(row)) {
            is MAction -> {
                sound(SOUND_CONFIRM)
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
        if (KeyEvent.isGamepadButton(code)) padConnected = true
        when (code) {
            KeyEvent.KEYCODE_DPAD_UP -> direction(DIR_UP)
            KeyEvent.KEYCODE_DPAD_DOWN -> direction(DIR_DOWN)
            KeyEvent.KEYCODE_DPAD_LEFT -> direction(DIR_LEFT)
            KeyEvent.KEYCODE_DPAD_RIGHT -> direction(DIR_RIGHT)
            KeyEvent.KEYCODE_BUTTON_L1, KeyEvent.KEYCODE_BUTTON_L2 -> changeTab(-1)
            KeyEvent.KEYCODE_BUTTON_R1, KeyEvent.KEYCODE_BUTTON_R2 -> changeTab(1)
            KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.KEYCODE_ENTER -> activate()
            KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_BUTTON_SELECT,
            KeyEvent.KEYCODE_ESCAPE, KeyEvent.KEYCODE_BUTTON_MODE -> close()
            else -> return KeyEvent.isGamepadButton(code)
        }
        return true
    }

    fun close() {
        stopRepeat()
        sound(SOUND_CLOSE)
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
        const val SOUND_OPEN = 0
        const val SOUND_MOVE = 1
        const val SOUND_CONFIRM = 2
        const val SOUND_CLOSE = 3
        const val DIR_UP = 1
        const val DIR_DOWN = 2
        const val DIR_LEFT = 3
        const val DIR_RIGHT = 4
        const val FIRST_REPEAT_MS = 380L
        const val REPEAT_MS = 120L
    }
}

// ------------------------------------------------------------------ opcoes

/** Acoes do menu, implementadas pelo PcOverlay. */
interface PcMenuActions {
    fun resume()
    fun openEditor()
    fun exitToLauncher()
    /** Estilo, opacidade e afins mudaram: o PcOverlay rele as preferencias. */
    fun applyAppearance()
    val touchMode: PcTouchMode
}

/**
 * O que o menu mostra e muda no jogo de PC: Pausa (continuar, editar os controles, sair), Controles (o que o
 * PcOverlay aplica ao vivo) e Ajuda (os gestos do modo de toque em uso). O video fica no menu OPTIONS do proprio jogo
 * e na launcher (o DXVK e as opcoes do jogo so se leem no arranque).
 */
class PcMenuModel(private val context: Context, private val prefs: LauncherPrefs, private val actions: PcMenuActions) {
    private var directional by mutableStateOf(prefs.directional)
    private var style by mutableStateOf(prefs.controlStyle)
    private var badge by mutableStateOf(prefs.buttonBadge)
    private var opacity by mutableFloatStateOf(prefs.touchOpacity)
    private var scale by mutableFloatStateOf(prefs.touchScale)
    private var haptics by mutableStateOf(prefs.touchHaptics)
    private var blade by mutableStateOf(prefs.bladeToggle)
    private var pauseMenus by mutableStateOf(prefs.pauseOnMenus)
    private var confirmExit by mutableStateOf(false)

    private fun s(id: Int) = context.getString(id)

    private val buttons get() = actions.touchMode == PcTouchMode.BUTTONS

    /** Ao abrir: le as preferencias outra vez. */
    fun refresh() {
        directional = prefs.directional
        style = prefs.controlStyle
        badge = prefs.buttonBadge
        opacity = prefs.touchOpacity
        scale = prefs.touchScale
        haptics = prefs.touchHaptics
        blade = prefs.bladeToggle
        pauseMenus = prefs.pauseOnMenus
        confirmExit = false
    }

    val tabs: List<String>
        get() = listOf(s(R.string.menu_tab_pause), s(R.string.tab_controls), s(R.string.menu_tab_help))

    fun items(tab: Int): List<MenuItem> = when (tab) {
        0 -> pauseItems()
        1 -> controlItems()
        else -> helpItems()
    }

    private fun <T> choice(label: String, help: String?, options: List<Pair<T, String>>, current: T, onSet: (T) -> Unit): MChoice {
        val i = options.indexOfFirst { it.first == current }.coerceAtLeast(0)
        return MChoice(label, help, options.map { it.second }, i) { k -> onSet(options[k].first) }
    }

    private fun pauseItems(): List<MenuItem> = listOfNotNull(
        MAction(s(R.string.ingame_resume), null, s(R.string.menu_go)) { actions.resume() },
        if (buttons) MAction(s(R.string.ingame_edit_controls), s(R.string.menu_edit_help), s(R.string.menu_open)) { actions.openEditor() } else null,
        MHeader(s(R.string.menu_session)),
        MAction(
            s(R.string.ingame_exit),
            s(R.string.ingame_exit_hint),
            if (confirmExit) s(R.string.menu_exit_confirm_action) else s(R.string.menu_exit_action),
            danger = true,
        ) { if (confirmExit) actions.exitToLauncher() else confirmExit = true },
    )

    private fun controlItems(): List<MenuItem> {
        val list = ArrayList<MenuItem>()
        list += MInfo(context.getString(R.string.menu_touch_mode_info, modeName()))
        if (buttons) {
            list += MAction(s(R.string.ingame_edit_controls), s(R.string.menu_edit_help), s(R.string.menu_open)) { actions.openEditor() }
            list += choice(s(R.string.ctl_directional), null,
                listOf(Directional.STICK to s(R.string.ctl_stick), Directional.DPAD to s(R.string.ctl_dpad)), directional) { v ->
                directional = v; prefs.directional = v; actions.applyAppearance()
            }
            list += choice(s(R.string.ctl_style), null,
                listOf(
                    ControlStyle.GAME to s(R.string.ctl_style_classic),
                    ControlStyle.SCIFI to "Sci-fi",
                    ControlStyle.XBOX to "Xbox",
                    ControlStyle.PLAYSTATION to "PlayStation",
                    ControlStyle.NINTENDO to "Nintendo",
                ), style) { v -> style = v; prefs.controlStyle = v; actions.applyAppearance() }
            if (style.iconic) {
                list += choice(s(R.string.ctl_badge), null,
                    listOf(
                        ButtonBadge.XBOX to "Xbox",
                        ButtonBadge.PLAYSTATION to "PlayStation",
                        ButtonBadge.NINTENDO to "Nintendo",
                        ButtonBadge.NONE to s(R.string.ctl_badge_none),
                    ), badge) { v -> badge = v; prefs.buttonBadge = v; actions.applyAppearance() }
            }
            list += MStep(s(R.string.ctl_opacity_title), null, "${(opacity * 100).roundToInt()}%") { d ->
                val next = ((opacity * 10).roundToInt() + d).coerceIn(1, 10) / 10f
                opacity = next; prefs.touchOpacity = next; actions.applyAppearance()
            }
            list += MStep(s(R.string.ctl_scale_title), null, "${(scale * 100).roundToInt()}%") { d ->
                val next = ((scale * 10).roundToInt() + d).coerceIn(5, 16) / 10f
                scale = next; prefs.touchScale = next; actions.applyAppearance()
            }
            list += choice(s(R.string.ctl_blade_mode), s(R.string.ctl_blade_toggle_sub),
                listOf(true to s(R.string.ctl_blade_toggle_opt), false to s(R.string.ctl_blade_hold_opt)), blade) { v ->
                blade = v; prefs.bladeToggle = v; actions.applyAppearance()
            }
            list += MToggle(s(R.string.ctl_haptics), null, haptics) { v ->
                haptics = v; prefs.touchHaptics = v; actions.applyAppearance()
            }
        }
        list += MToggle(s(R.string.ctl_pause_menu), s(R.string.ctl_pause_menu_sub), pauseMenus) { v ->
            pauseMenus = v; prefs.pauseOnMenus = v; actions.applyAppearance()
        }
        return list
    }

    private fun modeName() = s(when (actions.touchMode) {
        PcTouchMode.BUTTONS -> R.string.ctl_mode_buttons
        PcTouchMode.GESTURES -> R.string.ctl_mode_gestures
        PcTouchMode.MOUSE -> R.string.ctl_mode_mouse
    })

    /** Os gestos do modo em uso e os dos menus do jogo (os mesmos textos das explicacoes da launcher). */
    private fun helpItems(): List<MenuItem> {
        val list = ArrayList<MenuItem>()
        list += MHeader(modeName())
        val steps = when (actions.touchMode) {
            PcTouchMode.BUTTONS -> listOf(R.string.demo_btn_stick, R.string.demo_btn_x, R.string.demo_btn_y, R.string.demo_btn_a,
                R.string.demo_btn_lt, R.string.demo_btn_camera, R.string.demo_btn_click)
            PcTouchMode.GESTURES -> listOf(R.string.demo_gest_move, R.string.demo_gest_light, R.string.demo_gest_strong,
                R.string.demo_gest_camera, R.string.demo_gest_blade, R.string.demo_gest_back, R.string.demo_gest_start, R.string.demo_gest_jump)
            PcTouchMode.MOUSE -> listOf(R.string.demo_mouse_drag, R.string.demo_mouse_tap, R.string.demo_mouse_two)
        }
        steps.forEach { list += MInfo("• " + s(it)) }
        list += MHeader(s(R.string.ctl_menus_title))
        listOf(R.string.demo_menu_swipe, R.string.demo_menu_tap, R.string.demo_menu_back).forEach { list += MInfo("• " + s(it)) }
        list += MHeader(s(R.string.settings_physical_pad))
        list += MInfo(s(R.string.menu_pad_help))
        return list
    }
}

// --------------------------------------------------------------------- ecra

/*
 * 12/11: painel lateral (antes era um quadro no meio da tela). Abre-se da direita, no tema da launcher (claro,
 * escuro ou Material You), abas com icone; a ajuda da linha escolhida aparece por baixo dela (o rodape fixo
 * tapava a ultima linha da lista). Tocar fora do painel fecha o menu.
 */
@Composable
fun PcGameMenu(model: PcMenuModel, controller: MenuController) {
    val tabs = model.tabs
    controller.tabCount = tabs.size
    val items = model.items(controller.tab)
    SideEffect {
        controller.items = items
        controller.ensureRow()
    }
    val scheme = MaterialTheme.colorScheme
    val listState = rememberLazyListState()
    LaunchedEffect(controller.tab, controller.row) {
        if (controller.row >= 0) listState.animateScrollToItem((controller.row - 2).coerceAtLeast(0))
    }
    val shown = remember { MutableTransitionState(false) }.apply { targetState = true }

    BoxWithConstraints(Modifier.fillMaxSize()) {
        val panelWidth = minOf(380.dp, maxWidth * 0.92f)
        Box(
            Modifier
                .fillMaxSize()
                .background(Color.Black.copy(alpha = 0.35f))
                .clickable(
                    interactionSource = remember { MutableInteractionSource() },
                    indication = null,
                ) { controller.close() },
        )
        AnimatedVisibility(
            visibleState = shown,
            modifier = Modifier.align(Alignment.CenterEnd),
            enter = slideInHorizontally(tween(180)) { it / 2 } + fadeIn(tween(180)),
        ) {
            Column(
                Modifier
                    .fillMaxHeight()
                    .width(panelWidth)
                    .clip(RoundedCornerShape(topStart = 20.dp, bottomStart = 20.dp))
                    .background(scheme.surface)
                    .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) {}
                    .windowInsetsPadding(WindowInsets.displayCutout.only(WindowInsetsSides.End + WindowInsetsSides.Vertical)),
            ) {
                MenuHeader(controller)
                MenuTabs(tabs, controller)
                HorizontalDivider(color = scheme.outlineVariant.copy(alpha = 0.5f))
                LazyColumn(
                    Modifier
                        .weight(1f)
                        .fillMaxWidth(),
                    state = listState,
                    contentPadding = PaddingValues(horizontal = 10.dp, vertical = 6.dp),
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
                if (controller.padConnected) {
                    Text(
                        androidx.compose.ui.res.stringResource(R.string.menu_hints),
                        style = MaterialTheme.typography.labelSmall,
                        color = scheme.onSurfaceVariant,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier
                            .fillMaxWidth()
                            .background(scheme.surfaceContainer)
                            .padding(horizontal = 16.dp, vertical = 8.dp),
                    )
                }
            }
        }
    }
}

@Composable
private fun MenuHeader(controller: MenuController) {
    val scheme = MaterialTheme.colorScheme
    Row(
        Modifier
            .fillMaxWidth()
            .padding(start = 18.dp, end = 8.dp, top = 10.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.size(width = 3.dp, height = 16.dp).background(scheme.primary))
        Spacer(Modifier.width(8.dp))
        Column(Modifier.weight(1f)) {
            Text("ZANDATSU PC", style = LogoStyle.copy(fontSize = 14.sp, letterSpacing = 3.sp), color = scheme.onSurface)
            Text(
                androidx.compose.ui.res.stringResource(R.string.ingame_paused),
                style = MaterialTheme.typography.labelSmall,
                color = scheme.onSurfaceVariant,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
        IconButton(onClick = { controller.close() }) {
            Icon(
                Icons.Filled.Close,
                contentDescription = androidx.compose.ui.res.stringResource(R.string.ingame_resume),
                tint = scheme.onSurfaceVariant,
            )
        }
    }
}

private val TAB_ICONS = listOf(Icons.Filled.Pause, Icons.Filled.SportsEsports, Icons.Filled.TouchApp)

@Composable
private fun MenuTabs(tabs: List<String>, controller: MenuController) {
    val scheme = MaterialTheme.colorScheme
    Row(
        Modifier
            .fillMaxWidth()
            .padding(horizontal = 10.dp, vertical = 6.dp),
        horizontalArrangement = Arrangement.spacedBy(4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (controller.padConnected) PadHint("LB")
        tabs.forEachIndexed { i, t ->
            val on = i == controller.tab
            Column(
                Modifier
                    .weight(1f)
                    .clip(RoundedCornerShape(12.dp))
                    .background(if (on) scheme.secondaryContainer else Color.Transparent)
                    .clickable { controller.selectTab(i) }
                    .padding(vertical = 6.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Icon(
                    TAB_ICONS.getOrElse(i) { Icons.Filled.Settings },
                    contentDescription = null,
                    tint = if (on) scheme.onSecondaryContainer else scheme.onSurfaceVariant,
                    modifier = Modifier.size(20.dp),
                )
                Text(
                    t,
                    style = MaterialTheme.typography.labelSmall,
                    color = if (on) scheme.onSecondaryContainer else scheme.onSurfaceVariant,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
        }
        if (controller.padConnected) PadHint("RB")
    }
}

@Composable
private fun PadHint(text: String) {
    Text(
        text,
        style = MaterialTheme.typography.labelSmall,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        modifier = Modifier
            .border(1.dp, MaterialTheme.colorScheme.outlineVariant, RoundedCornerShape(6.dp))
            .padding(horizontal = 5.dp, vertical = 2.dp),
    )
}

@Composable
private fun MenuRow(item: MenuItem, selected: Boolean, onClick: () -> Unit, onAdjust: (Int) -> Unit) {
    val scheme = MaterialTheme.colorScheme
    when (item) {
        is MHeader -> Text(
            item.label.uppercase(),
            style = MaterialTheme.typography.labelMedium,
            color = scheme.primary,
            modifier = Modifier.padding(start = 10.dp, top = 14.dp, bottom = 4.dp),
        )
        is MInfo -> Text(
            item.label,
            style = MaterialTheme.typography.bodySmall,
            color = scheme.onSurfaceVariant,
            modifier = Modifier.padding(horizontal = 10.dp, vertical = 4.dp),
        )
        else -> {
            Column(
                Modifier
                    .fillMaxWidth()
                    .padding(vertical = 1.dp)
                    .clip(RoundedCornerShape(12.dp))
                    .background(if (selected) scheme.secondaryContainer else Color.Transparent)
                    .clickable(onClick = onClick)
                    .padding(horizontal = 12.dp, vertical = 7.dp),
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        item.label,
                        style = MaterialTheme.typography.bodyMedium,
                        color = if (selected) scheme.onSecondaryContainer else scheme.onSurface,
                        modifier = Modifier.weight(1f).padding(end = 8.dp),
                    )
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
                val help = if (selected) {
                    listOfNotNull(
                        item.help,
                        if (item.restart) androidx.compose.ui.res.stringResource(R.string.menu_restart_needed) else null,
                    ).joinToString(" ")
                } else if (item.restart) {
                    androidx.compose.ui.res.stringResource(R.string.menu_restart_badge)
                } else {
                    ""
                }
                if (help.isNotEmpty()) {
                    Text(
                        help,
                        style = MaterialTheme.typography.bodySmall,
                        color = if (selected) scheme.onSecondaryContainer.copy(alpha = 0.8f) else scheme.tertiary,
                        modifier = Modifier.padding(top = 3.dp),
                    )
                }
            }
        }
    }
}

@Composable
private fun TogglePill(on: Boolean) {
    val scheme = MaterialTheme.colorScheme
    Box(
        Modifier
            .size(width = 40.dp, height = 22.dp)
            .clip(RoundedCornerShape(50))
            .background(if (on) scheme.primary else scheme.surfaceVariant)
            .border(1.dp, if (on) scheme.primary else scheme.outline, RoundedCornerShape(50))
            .padding(3.dp),
        contentAlignment = if (on) Alignment.CenterEnd else Alignment.CenterStart,
    ) {
        Box(
            Modifier
                .size(16.dp)
                .clip(CircleShape)
                .background(if (on) scheme.onPrimary else scheme.outline),
        )
    }
}

@Composable
private fun Stepper(value: String, onAdjust: (Int) -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Arrow(Icons.AutoMirrored.Filled.KeyboardArrowLeft) { onAdjust(-1) }
        Text(
            value,
            style = MaterialTheme.typography.labelLarge,
            color = MaterialTheme.colorScheme.primary,
            textAlign = TextAlign.Center,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.widthIn(min = 64.dp, max = 140.dp).padding(horizontal = 2.dp),
        )
        Arrow(Icons.AutoMirrored.Filled.KeyboardArrowRight) { onAdjust(1) }
    }
}

@Composable
private fun Arrow(icon: androidx.compose.ui.graphics.vector.ImageVector, onClick: () -> Unit) {
    Box(
        Modifier
            .size(28.dp)
            .clip(CircleShape)
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        Icon(icon, contentDescription = null, tint = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.size(20.dp))
    }
}
