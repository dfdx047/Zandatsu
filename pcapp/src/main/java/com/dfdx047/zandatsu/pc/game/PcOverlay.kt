package com.dfdx047.zandatsu.pc.game

import android.app.Activity
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.InputDevice
import android.view.KeyEvent
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.ImageView
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.savedstate.SavedStateRegistry
import androidx.savedstate.SavedStateRegistryController
import androidx.savedstate.SavedStateRegistryOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import com.dfdx047.zandatsu.pc.R
import com.dfdx047.zandatsu.pc.controls.ControlAction
import com.dfdx047.zandatsu.pc.controls.ControlElement
import com.dfdx047.zandatsu.pc.controls.ControlLayoutStore
import com.dfdx047.zandatsu.pc.controls.ControlsEditor
import com.dfdx047.zandatsu.pc.controls.LayoutKind
import com.dfdx047.zandatsu.pc.controls.PadInput
import com.dfdx047.zandatsu.pc.data.LauncherPrefs
import com.dfdx047.zandatsu.pc.data.PcTouchMode
import com.dfdx047.zandatsu.pc.ui.theme.SwordManTheme
import com.dfdx047.zandatsu.pc.data.DarkMode

/** Dono de lifecycle/saved state sempre ativo para o ComposeView na janela-painel por cima do jogo. */
class OverlayOwner : LifecycleOwner, SavedStateRegistryOwner {
    private val registry = LifecycleRegistry(this)
    private val savedState = SavedStateRegistryController.create(this)

    init {
        savedState.performAttach()
        savedState.performRestore(null)
        registry.currentState = Lifecycle.State.RESUMED
    }

    override val lifecycle: Lifecycle get() = registry
    override val savedStateRegistry: SavedStateRegistry get() = savedState.savedStateRegistry

    fun destroy() {
        registry.currentState = Lifecycle.State.DESTROYED
    }
}

enum class InGameScreen { CLOSED, MENU, EDITOR }

/**
 * Camada por cima do jogo de PC (versao simplificada do GameOverlay da app do 360):
 *  - modo Botoes: os controles desenhados (TouchControlsView + HelperEngine -> SwordManPad -> nativeTouchPad),
 *    o botao de menu e o de troca de layout (combate / menus / comando completo);
 *  - modos Gestos e Mouse: so o botao de menu; o resto do toque vai direto para o nativo (PcGameActivity).
 * Menu (Compose): continuar, editar os controles (modo Botoes), sair do jogo. Com o menu aberto o jogo fica em pausa.
 */
class PcOverlay(
    private val activity: Activity,
    private val host: ViewGroup,
    private val mode: PcTouchMode,
) {
    private val prefs = LauncherPrefs(activity)
    private val engine = HelperEngine()
    private val main = Handler(Looper.getMainLooper())
    private val owner = OverlayOwner()
    private val screen = mutableStateOf(InGameScreen.CLOSED)
    private var view: TouchControlsView? = null
    private var menuButton: ImageView? = null
    private var layoutButton: ImageView? = null
    private var compose: ComposeView? = null
    private var inBattle = true
    private var onMap = false

    /** Menu ou editor aberto/fechado: o PcGameActivity pausa o jogo (nativePause). */
    var onMenuChanged: ((Boolean) -> Unit)? = null
    /** "Sair do jogo" no menu. */
    var onExit: (() -> Unit)? = null

    val buttonsMode get() = mode == PcTouchMode.BUTTONS

    fun onCreate() {
        host.setViewTreeLifecycleOwner(owner)
        host.setViewTreeSavedStateRegistryOwner(owner)
        val density = activity.resources.displayMetrics.density
        if (buttonsMode) {
            engine.pauseQuickMenu = prefs.pauseOnMenus
            engine.autoSwitch = prefs.touchAutoSwitch
            val v = TouchControlsView(activity).apply {
                listener = object : TouchControlsView.Listener {
                    override fun onPress(element: ControlElement) {
                        val a = element.action
                        // O botao "menu" (Back + Start) do layout abre o menu da app.
                        if (a is ControlAction.Press && a.inputs == setOf(PadInput.BACK, PadInput.START)) {
                            openMenu(); return
                        }
                        ensurePad(); engine.press(element.id, element.action)
                    }
                    override fun onRelease(element: ControlElement) = engine.releaseElement(element.id)
                    override fun onStick(right: Boolean, x: Float, y: Float) {
                        ensurePad(); if (right) engine.setLook(x, y) else engine.setStick(x, y)
                    }
                    override fun onLookTap(element: ControlElement) { ensurePad(); engine.tapR3() }
                    override fun onDpad(element: ControlElement, inputs: Set<PadInput>) { ensurePad(); engine.setDpad(element.id, inputs) }
                }
            }
            view = v
            host.addView(v, ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
            val lb = roundButton(R.drawable.ic_btn_layout, R.string.ingame_switch_layout) { cycleLayout() }
            layoutButton = lb
            host.addView(lb, FrameLayout.LayoutParams((40 * density).toInt(), (40 * density).toInt(), Gravity.TOP or Gravity.START).apply {
                leftMargin = (64 * density).toInt(); topMargin = (10 * density).toInt()
            })
        }
        val mb = roundButton(R.drawable.ic_btn_menu, R.string.ingame_title) { openMenu() }
        menuButton = mb
        host.addView(mb, FrameLayout.LayoutParams((40 * density).toInt(), (40 * density).toInt(), Gravity.TOP or Gravity.START).apply {
            leftMargin = (14 * density).toInt(); topMargin = (10 * density).toInt()
        })
        val c = ComposeView(activity).apply {
            setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnDetachedFromWindowOrReleasedFromPool)
            setContent {
                SwordManTheme(themeMode = prefs.themeMode, darkMode = DarkMode.DARK) {
                    when (screen.value) {
                        InGameScreen.CLOSED -> Unit
                        InGameScreen.MENU -> PcMenu(
                            buttonsMode,
                            onResume = { setScreen(InGameScreen.CLOSED) },
                            onEdit = { setScreen(InGameScreen.EDITOR) },
                            onExit = { onExit?.invoke() },
                        )
                        InGameScreen.EDITOR -> ControlsEditor(prefs, overGame = true, initialKind = currentLayoutKind()) { closeEditor() }
                    }
                }
            }
            visibility = View.GONE
        }
        compose = c
        host.addView(c, ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        applyAppearance()
        if (buttonsMode) main.postDelayed({ ensurePad() }, 1500)
    }

    private fun roundButton(icon: Int, description: Int, onClick: () -> Unit): ImageView {
        val density = activity.resources.displayMetrics.density
        return ImageView(activity).apply {
            setImageResource(icon)
            setColorFilter(Color.parseColor("#FFD27A"))
            val pad = (9 * density).toInt()
            setPadding(pad, pad, pad, pad)
            background = GradientDrawable().apply {
                shape = GradientDrawable.OVAL
                setColor(Color.argb(60, 255, 255, 255))
                setStroke((1.5f * density).toInt(), Color.argb(140, 255, 243, 222))
            }
            alpha = prefs.touchOpacity.coerceAtLeast(0.4f)
            contentDescription = activity.getString(description)
            setOnClickListener { onClick() }
        }
    }

    private fun ensurePad() {
        if (!SwordManPad.isAttached()) { SwordManPad.attach(); engine.start() }
    }

    private fun currentLayoutKind(): LayoutKind = when {
        onMap -> LayoutKind.FULL
        inBattle -> LayoutKind.GAMEPLAY
        else -> LayoutKind.MENU
    }

    private fun cycleLayout() {
        when (currentLayoutKind()) {
            LayoutKind.GAMEPLAY -> { inBattle = false; onMap = false }
            LayoutKind.MENU -> { inBattle = false; onMap = true }
            LayoutKind.FULL -> { inBattle = true; onMap = false }
        }
        engine.noteManualLayout(inBattle)
        view?.let { it.releaseAll(); it.layout = ControlLayoutStore.load(currentLayoutKind()) }
    }

    private fun applyAppearance() {
        view?.let { v ->
            v.layout = ControlLayoutStore.load(currentLayoutKind())
            v.style = prefs.controlStyle
            v.directional = prefs.directional
            v.badge = prefs.buttonBadge
            v.globalOpacity = prefs.touchOpacity
            v.haptics = prefs.touchHaptics
            v.bladeToggle = prefs.bladeToggle
            v.globalScale = prefs.touchScale
        }
        val a = prefs.touchOpacity.coerceAtLeast(0.4f)
        menuButton?.alpha = a
        layoutButton?.alpha = a
        updateVisibility()
    }

    private fun updateVisibility() {
        val open = isMenuOpen()
        view?.let { if (open) it.releaseAll(); it.visibility = if (open) View.GONE else View.VISIBLE }
        menuButton?.visibility = if (open) View.GONE else View.VISIBLE
        layoutButton?.visibility = if (open) View.GONE else View.VISIBLE
    }

    fun isMenuOpen(): Boolean = screen.value != InGameScreen.CLOSED

    fun openMenu() = setScreen(InGameScreen.MENU)

    private fun closeEditor() {
        view?.layout = ControlLayoutStore.load(currentLayoutKind())
        setScreen(InGameScreen.MENU)
    }

    private fun setScreen(s: InGameScreen) {
        screen.value = s
        onMenuChanged?.invoke(s != InGameScreen.CLOSED && prefs.pauseOnMenus)
        compose?.visibility = if (s == InGameScreen.CLOSED) View.GONE else View.VISIBLE
        updateVisibility()
        if (buttonsMode) {
            engine.menuOpen = s != InGameScreen.CLOSED && prefs.pauseOnMenus
            engine.start()
        }
    }

    /** Ponto (coordenadas da camada) em cima do botao de menu? (modos Gestos/Mouse: o resto vai para o nativo). */
    fun menuButtonContains(x: Float, y: Float): Boolean {
        val b = menuButton ?: return false
        if (b.visibility != View.VISIBLE) return false
        val slop = 8 * activity.resources.displayMetrics.density
        return x >= b.left - slop && x <= b.right + slop && y >= b.top - slop && y <= b.bottom + slop
    }

    /** Modo Botoes: ha um controle da tela neste ponto (ou o Blade Mode por deslize esta ativo). */
    fun touchOwnsPoint(x: Float, y: Float): Boolean =
        view?.let { it.visibility == View.VISIBLE && it.ownsPoint(x, y) } ?: false

    /** Back: fecha um nivel; devolve true se tratou. */
    fun onBackPressed(): Boolean = when (screen.value) {
        InGameScreen.EDITOR -> { closeEditor(); true }
        InGameScreen.MENU -> { setScreen(InGameScreen.CLOSED); true }
        InGameScreen.CLOSED -> false
    }

    /** Back + Start ao mesmo tempo num comando fisico abre o menu. */
    private val comboDown = HashSet<Int>()
    fun onMenuKeyEvent(e: KeyEvent): Boolean {
        if (isMenuOpen()) return false
        val code = e.keyCode
        if (code != KeyEvent.KEYCODE_BUTTON_SELECT && code != KeyEvent.KEYCODE_BUTTON_START) return false
        when (e.action) {
            KeyEvent.ACTION_DOWN -> { comboDown += code; if (comboDown.size == 2) { comboDown.clear(); openMenu(); return true } }
            KeyEvent.ACTION_UP -> comboDown -= code
        }
        return false
    }

    fun isGamepadEvent(source: Int): Boolean =
        source and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
            source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK

    fun onResume() {
        applyAppearance()
        if (buttonsMode && SwordManPad.isAttached()) engine.start()
    }

    fun onPause() {
        view?.releaseAll()
        engine.stop()
    }

    fun onDestroy() {
        main.removeCallbacksAndMessages(null)
        engine.release()
        SwordManPad.detach()
        owner.destroy()
        view = null
    }
}

@Composable
private fun PcMenu(buttonsMode: Boolean, onResume: () -> Unit, onEdit: () -> Unit, onExit: () -> Unit) {
    Box(Modifier.fillMaxSize().background(androidx.compose.ui.graphics.Color.Black.copy(alpha = 0.55f)), contentAlignment = Alignment.Center) {
        Surface(shape = MaterialTheme.shapes.large, color = MaterialTheme.colorScheme.surfaceContainerHigh, modifier = Modifier.width(340.dp)) {
            Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text(stringResource(R.string.ingame_title), style = MaterialTheme.typography.titleLarge)
                Text(stringResource(R.string.ingame_paused), style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
                Button(onClick = onResume, modifier = Modifier.fillMaxWidth()) { Text(stringResource(R.string.ingame_resume)) }
                if (buttonsMode) OutlinedButton(onClick = onEdit, modifier = Modifier.fillMaxWidth()) { Text(stringResource(R.string.ingame_edit_controls)) }
                OutlinedButton(onClick = onExit, modifier = Modifier.fillMaxWidth()) { Text(stringResource(R.string.ingame_exit)) }
                Text(stringResource(R.string.ingame_exit_hint), style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}
