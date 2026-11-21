package com.dfdx047.zandatsu.pc

import android.app.NativeActivity
import android.os.Bundle
import android.system.Os
import android.util.Log
import android.os.SystemClock
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.WindowInsets
import android.view.WindowInsetsController
import com.dfdx047.zandatsu.drivers.DriverRepository
import com.dfdx047.zandatsu.game.GameOverlay
import org.libsdl.app.SwordManPad
import java.io.File
import kotlin.math.roundToInt

/**
 * Jogo de PC (MGR:R) num processo proprio (:pcgame), como o GameActivity do 360. NativeActivity: o laco nativo e o
 * pc/android/mgr_android_main.cpp, carregado pelo libmgrlab.so (so o carregador, sem codigo do jogo).
 * Antes do super.onCreate: desencripta o libgame para um memfd e passa tudo ao nativo por variaveis de ambiente
 * (MGR_LIB_FD, MGR_EXE, MGR_DRIVER_DIR/NAME, MGR_HOOK_DIR, MGR_RENDER_SIZE).
 *
 * Input (20/11): a fila de input fica com o Java (takeInputQueue(null)), para os controles por toque do 360
 * (GameOverlay: skin, editor, Blade Mode por deslize, menu lateral) funcionarem por cima do jogo. Teclas e eixos do
 * comando fisico vao para o nativo por JNI (nativeKey/nativeMotion, registados pelo mgr_android_main.cpp); o
 * comando da tela chega pelo SwordManPad.Sink (nativeTouchPad); um toque curto fora dos controles = clique do rato.
 */
class PcGameActivity : NativeActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        try {
            val exe = File(PcGame.exePath(this).ifEmpty { PcGame.findExe().orEmpty().also { PcGame.setExePath(this, it) } })
            if (!exe.isFile) throw PcGame.PcException("executavel nao encontrado: ${exe.path}")
            val t0 = System.nanoTime()
            val fd = PcGame.decryptToMemfd(this, exe)
            Log.i(TAG, "libmgrpc desencriptado em ${(System.nanoTime() - t0) / 1_000_000} ms (fd $fd)")
            Os.setenv("MGR_LIB_FD", fd.toString(), true)
            Os.setenv("MGR_EXE", exe.absolutePath, true)
            Os.setenv("MGR_HOOK_DIR", applicationInfo.nativeLibraryDir + "/", true)
            if (PcGame.profile(this).isEmpty()) PcGame.applyProfile(this, exe.absolutePath, PcGame.defaultProfile())
            Os.setenv("MGR_RENDER_SIZE", PcGame.renderSize(this), true)
            Os.setenv("MGR_FPS_CAP", PcGame.fpsCap(this).toString(), true)
            // FSR / CAS e interpolacao no present do DXVK (PcGame.dxvkPresentEnv).
            val hz = if (PcGame.frameInterp(this)) preferSixtyHz() else display?.refreshRate?.roundToInt() ?: 60
            PcGame.dxvkPresentEnv(this, hz).forEach { (k, v) -> Os.setenv(k, v, true) }
            Log.i(TAG, "present: fsr=${PcGame.fsrMode(this)} out=${PcGame.fsrOut(this)} fi=${PcGame.frameInterp(this)} tela=$hz Hz")
            val drv = PcGame.driverDir(this)
            val installed = DriverRepository.installed().firstOrNull { it.dir.absolutePath == drv }
            if (installed != null) {
                // adrenotools abre o driver com dlopen: tem de estar numa pasta interna (o externo e noexec).
                val dir = File(filesDir, "pcdriver").apply { mkdirs() }
                val lib = File(dir, installed.library.name)
                if (lib.length() != installed.library.length()) installed.library.copyTo(lib, overwrite = true)
                Os.setenv("MGR_DRIVER_DIR", dir.absolutePath + "/", true)
                Os.setenv("MGR_DRIVER_NAME", lib.name, true)
                Log.i(TAG, "driver ${installed.name} (${installed.library})")
            } else Log.i(TAG, "driver do sistema")
        } catch (e: Exception) {
            Log.e(TAG, "nao arrancou", e)
            Os.setenv("MGR_START_ERROR", e.message ?: e.toString(), true)
        }
        SwordManPad.setSink(pad)
        super.onCreate(savedInstanceState)
        // Antes de a janela ser adicionada (onResume): a fila de input fica com as Views, nao com o nativo.
        window.takeInputQueue(null)
        immersive()
        // A NativeActivity da a superficie da janela ao jogo (takeSurface): as Views dela nao sao desenhadas. Os
        // controles ficam numa janela-painel por cima (TYPE_APPLICATION_PANEL, sem foco: as teclas ficam na janela
        // do jogo e vao para o nativo); os toques no painel passam por overlayTouch (toque curto = clique).
        overlay = GameOverlay(this, pcMode = true, host = layer).also {
            it.onMenuChanged = { open -> toNative { nativePause(open) } }
            it.onCreate()
        }
        window.decorView.post { showLayer() }
    }

    private val layer: android.widget.FrameLayout by lazy {
        object : android.widget.FrameLayout(this) {
            override fun dispatchTouchEvent(ev: MotionEvent): Boolean { overlayTouch(ev); return super.dispatchTouchEvent(ev) }
        }
    }

    private fun showLayer() {
        val token = window.decorView.windowToken
        if (token == null) { window.decorView.postDelayed({ showLayer() }, 100); return }
        if (layer.parent != null) return
        val lp = android.view.WindowManager.LayoutParams(
            android.view.WindowManager.LayoutParams.MATCH_PARENT, android.view.WindowManager.LayoutParams.MATCH_PARENT,
            android.view.WindowManager.LayoutParams.TYPE_APPLICATION_PANEL,
            android.view.WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or android.view.WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN or
                android.view.WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
            android.graphics.PixelFormat.TRANSLUCENT,
        ).apply {
            this.token = token
            layoutInDisplayCutoutMode = android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
        }
        try { windowManager.addView(layer, lp) } catch (e: Exception) { Log.e(TAG, "camada dos controles", e) }
    }

    private var overlay: GameOverlay? = null

    /** Comando da tela (HelperEngine -> SwordManPad): estado completo enviado ao nativo a cada mudanca. */
    private val pad = object : SwordManPad.Sink {
        private var buttons = 0
        private var hat = 0
        private val axes = floatArrayOf(0f, 0f, 0f, 0f, 0f, 0f)
        override fun button(keycode: Int, down: Boolean) {
            synchronized(this) {
                val bit = BUTTON_BITS[keycode]
                if (bit != null) buttons = if (down) buttons or (1 shl bit) else buttons and (1 shl bit).inv()
                val h = HAT_BITS[keycode]
                if (h != null) hat = if (down) hat or h else hat and h.inv()
                send()
            }
        }
        override fun axis(axis: Int, value: Float) {
            synchronized(this) {
                if (axis !in 0..5) return
                axes[axis] = if (axis >= 4) (value + 1f) / 2f else value   // gatilhos -1..1 -> 0..1
                send()
            }
        }
        private fun send() = try { nativeTouchPad(buttons, hat, axes) } catch (_: UnsatisfiedLinkError) {}
    }

    private external fun nativeKey(event: KeyEvent)
    private external fun nativeMotion(event: MotionEvent)
    private external fun nativeTouchPad(buttons: Int, hat: Int, axes: FloatArray)
    private external fun nativePointer(x: Float, y: Float, phase: Int)
    private external fun nativePause(paused: Boolean)

    private fun toNative(block: () -> Unit): Boolean = try { block(); true } catch (_: UnsatisfiedLinkError) { false }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        overlay?.let { o ->
            o.onPhysicalInput(event)
            if (o.onMenuKeyEvent(event)) return true
            if (event.keyCode == KeyEvent.KEYCODE_BACK) {
                if (event.action == KeyEvent.ACTION_UP && !event.isCanceled) { if (o.isMenuOpen()) o.onBackPressed() else o.openMenu() }
                return true
            }
        }
        if (toNative { nativeKey(event) }) return true
        return super.dispatchKeyEvent(event)
    }

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        overlay?.let { o ->
            o.onPhysicalInput(event)
            if (o.onMenuMotion(event)) return true
        }
        if (toNative { nativeMotion(event) }) return true
        return super.dispatchGenericMotionEvent(event)
    }

    private var tapX = 0f
    private var tapY = 0f
    private var tapAt = 0L
    private var tapping = false

    /** Toque curto fora dos controles da tela = clique do rato nesse ponto (menus do jogo). */
    private fun overlayTouch(event: MotionEvent) {
        val o = overlay
        if (o != null && !o.isMenuOpen()) {
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    tapping = !o.touchOwnsPoint(event.x, event.y)
                    tapX = event.x; tapY = event.y; tapAt = SystemClock.uptimeMillis()
                }
                MotionEvent.ACTION_POINTER_DOWN, MotionEvent.ACTION_CANCEL -> tapping = false
                MotionEvent.ACTION_MOVE -> if (tapping && Math.hypot((event.x - tapX).toDouble(), (event.y - tapY).toDouble()) > 40) tapping = false
                MotionEvent.ACTION_UP -> if (tapping && SystemClock.uptimeMillis() - tapAt < 450) {
                    tapping = false
                    val w = window.decorView.width.toFloat().coerceAtLeast(1f); val h = window.decorView.height.toFloat().coerceAtLeast(1f)
                    toNative { nativePointer(tapX / w, tapY / h, 0); nativePointer(tapX / w, tapY / h, 2) }
                }
            }
        }
    }

    override fun onResume() { super.onResume(); overlay?.onResume() }
    override fun onPause() { overlay?.onPause(); super.onPause() }
    override fun onDestroy() { if (layer.parent != null) runCatching { windowManager.removeViewImmediate(layer) }; overlay?.onDestroy(); overlay = null; SwordManPad.setSink(null); super.onDestroy() }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) immersive()
    }

    /**
     * Interpolacao 30 -> 60: o DXVK apresenta um frame gerado e um real por cada frame do jogo, cada um num vblank
     * (FIFO), por isso a tela deve estar a 60 Hz. Pede o modo de 60 Hz com a mesma resolucao, se existir; devolve os
     * Hz com que o DXVK deve contar (DXVK_FI_HZ: a 120 Hz cada frame e apresentado duas vezes).
     */
    private fun preferSixtyHz(): Int {
        val d = display ?: return 60
        val cur = d.mode
        val sixty = d.supportedModes.filter { it.physicalWidth == cur.physicalWidth && it.physicalHeight == cur.physicalHeight }
            .minByOrNull { kotlin.math.abs(it.refreshRate - 60f) }
        if (sixty != null && kotlin.math.abs(sixty.refreshRate - 60f) < 1f) {
            window.attributes = window.attributes.apply { preferredDisplayModeId = sixty.modeId }
            return 60
        }
        return cur.refreshRate.roundToInt()
    }

    private fun immersive() {
        window.decorView.windowInsetsController?.let {
            it.hide(WindowInsets.Type.systemBars())
            it.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    companion object {
        private const val TAG = "MGRPC"
        /** Bits do estado XInput do nativo (A B X Y LB RB BACK START LS RS, pilotButton em mgr_android_main.cpp). */
        private val BUTTON_BITS = mapOf(
            KeyEvent.KEYCODE_BUTTON_A to 0, KeyEvent.KEYCODE_BUTTON_B to 1, KeyEvent.KEYCODE_BUTTON_X to 2,
            KeyEvent.KEYCODE_BUTTON_Y to 3, KeyEvent.KEYCODE_BUTTON_L1 to 4, KeyEvent.KEYCODE_BUTTON_R1 to 5,
            KeyEvent.KEYCODE_BUTTON_SELECT to 6, KeyEvent.KEYCODE_BUTTON_START to 7,
            KeyEvent.KEYCODE_BUTTON_THUMBL to 8, KeyEvent.KEYCODE_BUTTON_THUMBR to 9,
        )
        /** Cruz: 1 cima, 2 baixo, 4 esquerda, 8 direita. */
        private val HAT_BITS = mapOf(
            KeyEvent.KEYCODE_DPAD_UP to 1, KeyEvent.KEYCODE_DPAD_DOWN to 2, KeyEvent.KEYCODE_DPAD_LEFT to 4, KeyEvent.KEYCODE_DPAD_RIGHT to 8,
        )
    }
}
