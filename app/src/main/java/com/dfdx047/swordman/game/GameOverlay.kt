package com.dfdx047.swordman.game

import android.app.Activity
import android.content.Context
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.hardware.input.InputManager
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.Gravity
import android.view.InputDevice
import android.view.InputEvent
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.ImageView
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import com.dfdx047.swordman.R
import com.dfdx047.swordman.controls.ControlAction
import com.dfdx047.swordman.controls.ControlElement
import com.dfdx047.swordman.controls.ControlLayoutStore
import com.dfdx047.swordman.controls.LayoutKind
import com.dfdx047.swordman.controls.MacroStep
import com.dfdx047.swordman.controls.PadInput
import com.dfdx047.swordman.data.AppPaths
import com.dfdx047.swordman.data.DarkMode
import com.dfdx047.swordman.data.GameConfig
import com.dfdx047.swordman.data.LauncherPrefs
import com.dfdx047.swordman.data.ThemeMode
import com.dfdx047.swordman.data.TouchMode
import com.dfdx047.swordman.ui.theme.SwordManTheme
import org.libsdl.app.SwordManPad

/**
 * Controles por toque dentro do jogo:
 *  - TouchControlsView por cima da superficie do SDL, com tres layouts (combate, menus e
 *    comando completo), trocados pelo botao de troca (e sozinhos quando o runtime souber
 *    se o jogo esta em combate ou num menu);
 *  - modo Automatico / Ligado / Desligado (no automatico aparecem ao tocar na
 *    tela e somem quando se usa um comando fisico);
 *  - engrenagem (canto de cima a esquerda): menu dos controles e editor;
 *  - pause (em cima, ao centro): opcoes do jogo por toque (ex-menu rapido do PC);
 *  - com qualquer menu aberto o jogo fica pausado.
 *
 * Chamado pelo GameActivity (Java) nos eventos do ciclo de vida e de input.
 */
class GameOverlay(private val activity: Activity) : InputManager.InputDeviceListener, InGameActions {
    private val prefs = LauncherPrefs(activity)
    private val engine = HelperEngine()
    private val main = Handler(Looper.getMainLooper())
    private val startedAt = SystemClock.uptimeMillis()
    private var view: TouchControlsView? = null
    private var gear: ImageView? = null
    private var pauseButton: ImageView? = null
    private var layoutButton: ImageView? = null
    private var compose: ComposeView? = null
    private val owner = OverlayOwner()
    private val screen = mutableStateOf(InGameScreen.CLOSED)
    /** Menu do jogo do Android (AndroidMenu.kt): navegacao e opcoes. */
    private val menuController = MenuController()
    private val menuModel by lazy { MenuModel(activity, prefs, this) }
    private val inputManager = activity.getSystemService(Context.INPUT_SERVICE) as InputManager
    /** Teste automatico pelo PC (tools/autotest.sh). */
    private val autoPilot = AutoPilot(
        activity,
        press = { input ->
            engine.press("autopilot", com.dfdx047.swordman.controls.ControlAction.Sequence(
                listOf(com.dfdx047.swordman.controls.MacroStep(setOf(input), 6)), holdLast = false))
        },
        selfTest = { startSelfTest() },
        captureFrames = { captureFrames() },
    ).also { it.register() }

    /** Modo automatico: os controles estao a mostra (ultimo input foi toque). */
    private var autoShown = true
    /** Layout de combate (o de omissao: tambem serve nos menus, A confirma e B volta). */
    private var inBattle = true
    /** Comando completo (tem prioridade sobre o combate e os menus). */
    private var onMap = false

    private val attachRunnable = Runnable { attachPad() }
    private var gpuStats: GpuStatsSampler? = null

    fun onCreate() {
        AppPaths.init(activity)
        if (GameConfig.load().perfLogging) {
            val f = java.io.File(AppPaths.logsDir, "gpu_stats.log")
            f.delete()
            gpuStats = GpuStatsSampler(f)
            val ctl = java.io.File(AppPaths.logsDir, "controles.log")
            ctl.delete()
            val fmt = java.text.SimpleDateFormat("HH:mm:ss.SSS", java.util.Locale.ROOT)
            engine.diag = { line ->
                try {
                    ctl.appendText(fmt.format(java.util.Date()) + " " + line + "\n")
                } catch (_: Exception) {
                }
            }
        }
        engine.pauseQuickMenu = prefs.pauseOnMenus
        // Volume escolhido no launcher (o ganho e global: pode ir antes de o audio arrancar).
        NativeGameState.setVolume(GameConfig.load().volume)
        engine.onBattleChanged = { battle ->
            inBattle = battle
            view?.let {
                it.releaseAll()
                it.layout = currentLayout()
            }
        }
        engine.onMapChanged = { map ->
            onMap = map
            view?.let {
                it.releaseAll()
                it.layout = currentLayout()
            }
        }
        // O motor corre sempre (pausa, luta/menus), mesmo com os controles desligados.
        main.postDelayed(attachRunnable, 2500)
        val density = activity.resources.displayMetrics.density

        val v = TouchControlsView(activity).apply {
            listener = object : TouchControlsView.Listener {
                override fun onPress(element: ControlElement) {
                    // O botao "menu" (Back + Start) abre o menu do Android.
                    val a = element.action
                    if (a is ControlAction.Press && a.inputs == setOf(PadInput.BACK, PadInput.START)) {
                        openMenuAt(0)
                        return
                    }
                    ensurePad()
                    engine.press(element.id, element.action)
                }

                override fun onRelease(element: ControlElement) = engine.releaseElement(element.id)

                override fun onStick(right: Boolean, x: Float, y: Float) {
                    ensurePad()
                    if (right) engine.setLook(x, y) else engine.setStick(x, y)
                }

                override fun onLookTap(element: ControlElement) {
                    ensurePad()
                    engine.tapR3()
                }

                override fun onDpad(element: ControlElement, inputs: Set<PadInput>) {
                    ensurePad()
                    engine.setDpad(element.id, inputs)
                }
            }
        }
        view = v
        autoShown = !physicalPadConnected()
        applyAppearance()
        activity.addContentView(
            v,
            ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT),
        )

        // Engrenagem (canto de cima a esquerda): menu dos controles.
        val g = roundButton(R.drawable.ic_btn_menu, R.string.ingame_title) { openMenuAt(menuModel.controlsTab) }
        gear = g
        activity.addContentView(
            g,
            FrameLayout.LayoutParams((40 * density).toInt(), (40 * density).toInt(), Gravity.TOP or Gravity.START).apply {
                leftMargin = (14 * density).toInt()
                topMargin = (10 * density).toInt()
            },
        )
        // Pause (em cima, ao centro): opcoes do jogo.
        val pb = roundButton(R.drawable.ic_btn_pause, R.string.port_menu_title) { openMenuAt(0) }
        pauseButton = pb
        activity.addContentView(
            pb,
            FrameLayout.LayoutParams((40 * density).toInt(), (40 * density).toInt(), Gravity.TOP or Gravity.CENTER_HORIZONTAL).apply {
                topMargin = (10 * density).toInt()
            },
        )

        // Trocar a mao entre os layouts (ao lado da engrenagem): combate -> menus -> comando
        // completo -> combate. Se o runtime souber o estado do jogo, a troca automatica
        // seguinte volta a mandar.
        val lb = roundButton(R.drawable.ic_btn_layout, R.string.ingame_switch_layout) {
            when (currentLayoutKind()) {
                LayoutKind.GAMEPLAY -> {
                    inBattle = false
                    onMap = false
                }
                LayoutKind.MENU -> {
                    inBattle = false
                    onMap = true
                }
                LayoutKind.FULL -> {
                    inBattle = true
                    onMap = false
                }
            }
            com.dfdx047.swordman.data.AppLog.i("controls", "botao de troca -> " + currentLayoutKind().name)
            engine.diag?.invoke("botao de troca -> " + currentLayoutKind().name)
            view?.let {
                it.releaseAll()
                it.layout = currentLayout()
            }
        }
        layoutButton = lb
        activity.addContentView(
            lb,
            FrameLayout.LayoutParams((40 * density).toInt(), (40 * density).toInt(), Gravity.TOP or Gravity.START).apply {
                leftMargin = (64 * density).toInt()
                topMargin = (10 * density).toInt()
            },
        )

        // Menus (Compose), escondidos ate um dos botoes ser tocado.
        val c = ComposeView(activity).apply {
            setViewTreeLifecycleOwner(owner)
            setViewTreeSavedStateRegistryOwner(owner)
            setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnDetachedFromWindowOrReleasedFromPool)
            setContent {
                // O tema do app (MGR ou Material You), sempre escuro por cima do jogo.
                SwordManTheme(themeMode = prefs.themeMode, darkMode = DarkMode.DARK) {
                    InGameOverlay(screen.value, prefs, this@GameOverlay, menuModel, menuController)
                }
            }
            visibility = View.GONE
        }
        activity.addContentView(
            c,
            ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT),
        )
        compose = c

        inputManager.registerInputDeviceListener(this, main)
        updateVisibility()
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

    /** Layout em uso (combate, menus ou comando completo), como o editor o gravou. */
    private fun currentLayout(): com.dfdx047.swordman.controls.ControlLayout =
        ControlLayoutStore.load(currentLayoutKind())

    override fun currentLayoutKind(): LayoutKind = when {
        onMap -> LayoutKind.FULL
        inBattle -> LayoutKind.GAMEPLAY
        else -> LayoutKind.MENU
    }

    private fun ensurePad() {
        if (!SwordManPad.isAttached() && SystemClock.uptimeMillis() - startedAt > 1500) attachPad()
    }

    private fun attachPad() {
        // Sempre ligado (mesmo sem controles na tela): e por ele que a vibracao do
        // jogo chega ao motor do aparelho quando o comando fisico nao tem motor.
        SwordManPad.attach()
        engine.start()
    }

    // ---------------------------------------------------- input (GameActivity)

    /**
     * Toque na tela com os controles escondidos no modo automatico: mostra-os e
     * consome esse toque (devolve true).
     */
    fun onScreenTouch(ev: MotionEvent): Boolean {
        if (ev.actionMasked != MotionEvent.ACTION_DOWN) return false
        if (prefs.touchMode != TouchMode.AUTO || isMenuOpen() || autoShown) return false
        autoShown = true
        ensurePad()
        updateVisibility()
        return true
    }

    /** Botao ou eixo de um comando fisico: no modo automatico esconde os controles da tela. */
    fun onPhysicalInput(event: InputEvent) {
        if (prefs.touchMode != TouchMode.AUTO || !autoShown) return
        val dev = event.device ?: return
        if (dev.isVirtual) return
        val s = event.source
        val pad = s and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
            s and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK ||
            s and InputDevice.SOURCE_DPAD == InputDevice.SOURCE_DPAD
        if (!pad) return
        // Eixos: so um movimento a serio conta (sticks com drift e gatilhos com ruido
        // mandam eventos sem parar e escondiam os controles logo apos aparecerem).
        if (event is MotionEvent && STICK_AXES.none { kotlin.math.abs(event.getAxisValue(it)) > 0.5f }) return
        if (event is KeyEvent && (event.action != KeyEvent.ACTION_DOWN || event.keyCode == KeyEvent.KEYCODE_BACK)) return
        autoShown = false
        view?.releaseAll()
        updateVisibility()
    }

    // ---------------------------------------------------------------- menus

    fun isMenuOpen(): Boolean = screen.value != InGameScreen.CLOSED

    /** Botao Back com o jogo a correr: abre o menu. */
    fun openMenu() = openMenuAt(0)

    private fun openMenuAt(tab: Int) {
        menuModel.refresh()
        menuController.onClose = { resume() }
        menuController.open(tab)
        setScreen(InGameScreen.MENU)
    }

    /** Teclas Back + Start do comando ao mesmo tempo (com o menu fechado). */
    private val comboDown = HashSet<Int>()

    /**
     * Teclas do comando para o menu (GameActivity.dispatchKeyEvent): com o menu aberto as
     * pressoes sao do menu (as teclas soltas seguem para o jogo, que esta em pausa e assim
     * nao fica com nenhuma presa); com ele fechado, Back + Start abre-o. Devolve true se
     * consumiu o evento.
     */
    fun onMenuKeyEvent(e: KeyEvent): Boolean {
        val code = e.keyCode
        if (code == KeyEvent.KEYCODE_BACK) return false
        when (screen.value) {
            InGameScreen.MENU -> {
                if (e.action != KeyEvent.ACTION_DOWN) return false
                return menuController.onKeyDown(code)
            }
            InGameScreen.EDITOR -> return false
            InGameScreen.CLOSED -> Unit
        }
        if (code != KeyEvent.KEYCODE_BUTTON_SELECT && code != KeyEvent.KEYCODE_BUTTON_START) return false
        when (e.action) {
            KeyEvent.ACTION_DOWN -> {
                comboDown += code
                if (comboDown.size == 2) {
                    comboDown.clear()
                    openMenuAt(0)
                    return true
                }
            }
            KeyEvent.ACTION_UP -> comboDown -= code
        }
        return false
    }

    /** Analogico / cruz com o menu aberto. Devolve true se consumiu. */
    fun onMenuMotion(ev: MotionEvent): Boolean {
        if (screen.value != InGameScreen.MENU) return false
        val s = ev.source
        if (s and InputDevice.SOURCE_JOYSTICK != InputDevice.SOURCE_JOYSTICK) return false
        menuController.onMotion(ev)
        return true
    }

    private fun setScreen(s: InGameScreen) {
        screen.value = s
        compose?.visibility = if (s == InGameScreen.CLOSED) View.GONE else View.VISIBLE
        if (s != InGameScreen.CLOSED) {
            view?.releaseAll()
        }
        updateVisibility()
        // Com o menu aberto o jogo fica pausado (GPU + audio), se o usuario quiser.
        engine.menuOpen = s != InGameScreen.CLOSED && prefs.pauseOnMenus
        engine.start()
    }

    override fun resume() {
        menuController.stopRepeat()
        setScreen(InGameScreen.CLOSED)
    }

    /**
     * Volta ao launcher. O jogo corre no seu proprio processo (":game"): o runtime
     * recompilado nao se reinicia dentro do mesmo processo, por isso o processo do
     * jogo termina e o proximo "Jogar" comeca do zero.
     */
    override fun setDisplayRate(hz: Int) {
        (activity as? GameActivity)?.requestDisplayRate(hz)
    }

    override fun captureFrames() {
        try {
            java.io.File(AppPaths.appDir, "capture.req").writeText("2")
        } catch (e: Exception) {
            android.util.Log.w("swordman", "capture.req", e)
        }
    }

    private var selfTest: RenderSelfTest? = null

    override fun startSelfTest(): Boolean {
        val surface = GameActivity.gameSurface() ?: return false
        if (selfTest != null) return false
        setScreen(InGameScreen.CLOSED)
        // 22/10: a imagem congelada (replay do frame gravado) saia branca e o jogo caia (RP5 e
        // Cube). O autoteste corre ao vivo: cada estado medido A/B/A/B com o jogo a correr.
        // O modo congelado fica no codigo (RenderSelfTest(live = false)) para quando o replay
        // for corrigido.
        engine.freeze = false
        com.dfdx047.swordman.data.AppLog.i("selftest", "inicio (ao vivo)")
        selfTest = RenderSelfTest(activity, surface, live = true) {
            selfTest = null
            com.dfdx047.swordman.data.AppLog.i("selftest", "fim")
        }.also { it.start() }
        return true
    }

    override fun exitToLauncher() {
        setScreen(InGameScreen.CLOSED)
        NativeGameState.forceResume()
        NativeGameState.setCvars(mapOf("swordman_freeze_replay" to "false", "swordman_freeze_record" to "false"), persist = false)
        com.dfdx047.swordman.data.SessionState.markEnded(AppPaths.appDir)
        com.dfdx047.swordman.data.AppLog.i("game", "saida pelo menu")
        activity.startActivity(
            android.content.Intent(activity, com.dfdx047.swordman.LauncherActivity::class.java)
                .addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK or android.content.Intent.FLAG_ACTIVITY_CLEAR_TOP),
        )
        activity.finishAndRemoveTask()
        main.postDelayed({ android.os.Process.killProcess(android.os.Process.myPid()) }, 400)
    }

    override fun openEditor() = setScreen(InGameScreen.EDITOR)

    override fun closeEditor() {
        view?.layout = currentLayout()
        setScreen(InGameScreen.MENU)
    }

    override fun isInBattle(): Boolean = inBattle

    override fun applyAppearance() {
        val v = view ?: return
        v.layout = currentLayout()
        v.style = prefs.controlStyle
        v.directional = prefs.directional
        v.badge = prefs.buttonBadge
        v.globalOpacity = prefs.touchOpacity
        v.haptics = prefs.touchHaptics
        val a = prefs.touchOpacity.coerceAtLeast(0.4f)
        gear?.alpha = a
        pauseButton?.alpha = a
        layoutButton?.alpha = a
        updateVisibility()
    }

    // ------------------------------------------------------------ ciclo de vida

    fun onResume() {
        engine.pauseQuickMenu = prefs.pauseOnMenus
        applyAppearance()
        if (SwordManPad.isAttached()) engine.start()
        gpuStats?.start()
    }

    fun onPause() {
        gpuStats?.stop()
        view?.releaseAll()
        engine.stop()
    }

    fun onDestroy() {
        autoPilot.unregister()
        gpuStats?.stop()
        NativeGameState.forceResume()
        main.removeCallbacks(attachRunnable)
        inputManager.unregisterInputDeviceListener(this)
        engine.release()
        SwordManPad.detach()
        owner.destroy()
        view = null
    }

    /** Back do Android com um menu aberto: volta um nivel. Devolve true se tratou. */
    fun onBackPressed(): Boolean = when (screen.value) {
        InGameScreen.EDITOR -> {
            closeEditor()
            true
        }
        InGameScreen.MENU -> {
            menuController.close()
            true
        }
        InGameScreen.CLOSED -> false
    }

    private fun isGamepad(d: InputDevice): Boolean =
        !d.isVirtual && (d.sources and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
            d.sources and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK)

    private fun physicalPadConnected(): Boolean =
        InputDevice.getDeviceIds().any { id -> InputDevice.getDevice(id)?.let { isGamepad(it) } == true }

    private fun updateVisibility() {
        val v = view ?: return
        val menuOpen = isMenuOpen()
        val show = !menuOpen && when (prefs.touchMode) {
            TouchMode.ON -> true
            TouchMode.OFF -> false
            TouchMode.AUTO -> autoShown
        }
        if (!show) v.releaseAll()
        v.visibility = if (show) View.VISIBLE else View.GONE
        // Engrenagem e pause ficam sempre (menos com um menu aberto). Teste do
        // compositor: "env:DF_HIDE_BUTTONS = 1" no cvars.txt esconde-os (o Back abre o menu).
        val hideAll = android.system.Os.getenv("DF_HIDE_BUTTONS") == "1"
        val buttons = if (menuOpen || hideAll) View.GONE else View.VISIBLE
        gear?.visibility = buttons
        pauseButton?.visibility = buttons
        // A troca de layout so faz sentido com os controles da tela a mostra.
        layoutButton?.visibility = if (show && !hideAll) View.VISIBLE else View.GONE
    }

    override fun onInputDeviceAdded(deviceId: Int) {
        // So um comando novo a serio esconde os controles (no automatico). Outros aparelhos
        // (teclados, sensores, o proprio ecra a re-enumerar) nao mexem: nas versoes antigas
        // estes eventos escondiam os controles sem eles voltarem.
        val d = InputDevice.getDevice(deviceId)
        if (prefs.touchMode == TouchMode.AUTO && d != null && isGamepad(d)) {
            com.dfdx047.swordman.data.AppLog.i("controls", "comando ligado: ${d.name} (controles da tela escondidos)")
            autoShown = false
            view?.releaseAll()
        }
        updateVisibility()
    }

    override fun onInputDeviceRemoved(deviceId: Int) {
        if (prefs.touchMode == TouchMode.AUTO && !physicalPadConnected()) {
            com.dfdx047.swordman.data.AppLog.i("controls", "comando desligado (controles da tela de volta)")
            autoShown = true
        }
        updateVisibility()
    }

    override fun onInputDeviceChanged(deviceId: Int) = Unit

    private companion object {
        val STICK_AXES = intArrayOf(
            MotionEvent.AXIS_X, MotionEvent.AXIS_Y, MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ,
            MotionEvent.AXIS_HAT_X, MotionEvent.AXIS_HAT_Y, MotionEvent.AXIS_LTRIGGER,
            MotionEvent.AXIS_RTRIGGER, MotionEvent.AXIS_GAS, MotionEvent.AXIS_BRAKE,
        )
    }
}
