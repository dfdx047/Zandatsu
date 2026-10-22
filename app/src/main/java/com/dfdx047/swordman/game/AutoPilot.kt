package com.dfdx047.swordman.game

import android.app.Activity
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.KeyEvent
import androidx.core.content.ContextCompat
import com.dfdx047.swordman.controls.ControlAction
import com.dfdx047.swordman.controls.MacroStep
import com.dfdx047.swordman.controls.PadInput
import com.dfdx047.swordman.data.AppLog
import com.dfdx047.swordman.data.AppPaths
import java.io.File

/**
 * Teste automatico pelo PC (tools/autotest.sh), sem ninguem a mexer no aparelho:
 *
 *  - "rec-start" / "rec-stop": grava os botoes do comando fisico (o caminho do titulo ate ao
 *    gameplay) em <appdir>/autoplay.seq, com os tempos contados a partir dos primeiros frames.
 *  - "auto": repete o autoplay.seq, espera [SETTLE_MS] no gameplay e corre o autoteste do render
 *    (que no fim exporta o zip para Download, onde o script o vai buscar).
 *  - "press A" / "selftest" / "capture": um botao, so o autoteste, ou captura de frames (.dfc).
 *  - "auto" com --ez capture true: captura 2 frames a meio da espera (mapa do frame).
 *  - "auto" com --ez attack true: golpes X/Y durante a espera e o autoteste (cena de luta).
 *
 * Comando: adb shell am broadcast -a com.dfdx047.swordman.CMD -p com.dfdx047.swordman --es cmd auto
 */
class AutoPilot(
    private val activity: Activity,
    private val press: (PadInput) -> Unit,
    private val selfTest: () -> Boolean,
    /** Captura de frames (.dfc) para o mapa do frame (render nativo, 22/10). */
    private val captureFrames: () -> Unit = {},
) {
    private val main = Handler(Looper.getMainLooper())
    private val seqFile get() = File(AppPaths.appDir, "autoplay.seq")

    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            val cmd = intent.getStringExtra("cmd")?.trim() ?: return
            AppLog.i("autopilot", "comando: $cmd")
            NativeGameState.logLine("autopilot: $cmd")
            when {
                cmd == "rec-start" -> startRecording()
                cmd == "rec-stop" -> stopRecording()
                cmd == "auto" -> runAuto(intent.getIntExtra("settle", SETTLE_MS / 1000) * 1000L,
                    intent.getBooleanExtra("capture", false), intent.getBooleanExtra("attack", false))
                cmd == "capture" -> captureFrames()
                cmd == "selftest" -> selfTest()
                cmd.startsWith("press ") -> parse(cmd.substringAfter(' '))?.let(press)
            }
        }
    }

    fun register() {
        ContextCompat.registerReceiver(activity, receiver, IntentFilter(ACTION), ContextCompat.RECEIVER_EXPORTED)
        current = this
    }

    fun unregister() {
        try {
            activity.unregisterReceiver(receiver)
        } catch (_: Exception) {
        }
        if (current === this) current = null
        main.removeCallbacksAndMessages(null)
    }

    // --------------------------------------------------------------- gravar

    private var recording = false
    private var recT0 = 0L
    private var recLast = 0L
    private val recLines = ArrayList<String>()

    private fun startRecording() {
        recLines.clear()
        recording = false
        whenRunning {
            recT0 = SystemClock.uptimeMillis()
            recLast = recT0
            recording = true
            AppLog.i("autopilot", "a gravar os botoes (primeiros frames ja apresentados)")
        }
    }

    private fun stopRecording() {
        recording = false
        try {
            seqFile.writeText(recLines.joinToString("\n") + "\n")
            AppLog.i("autopilot", "gravado ${seqFile.name}: ${recLines.size / 2} botoes")
        } catch (e: Exception) {
            AppLog.w("autopilot", "nao gravou ${seqFile.name}", e)
        }
    }

    private fun onKey(event: KeyEvent) {
        if (!recording || event.action != KeyEvent.ACTION_DOWN || event.repeatCount != 0) return
        val input = fromKeyCode(event.keyCode) ?: return
        val now = SystemClock.uptimeMillis()
        recLines += "wait ${now - recLast}"
        recLines += "press ${input.name}"
        recLast = now
    }

    // --------------------------------------------------------------- repetir

    private fun runAuto(settleMs: Long, withCapture: Boolean = false, attack: Boolean = false) {
        val lines = try {
            seqFile.readLines().map { it.trim() }.filter { it.isNotEmpty() && !it.startsWith("#") }
        } catch (_: Exception) {
            emptyList()
        }
        if (lines.isEmpty()) AppLog.w("autopilot", "sem ${seqFile.name}: autoteste onde o jogo estiver", null)
        whenRunning {
            // Fotos do caminho: os logos do inicio (videos CRI) e o menu antes de entrar no jogo
            // (no Turnip, 21/10: logos invisiveis e menu azul/verde).
            val logs = File(AppPaths.appDir, "logs")
            main.postDelayed({ TurnipSweep.capture(activity, File(logs, "autopilot_1_logos.jpg")) {} }, 6000)
            main.postDelayed({ TurnipSweep.capture(activity, File(logs, "autopilot_2_logos.jpg")) {} }, 12000)
            // Esperas x1.5 (os carregamentos variam de arranque para arranque; um botao carregado
            // durante um carregamento perde-se - Cube, 21/10: ficou no menu do titulo) e, no fim,
            // mais 3 "A" de 3 em 3 s (no menu entram no jogo; no gameplay sao so ataques).
            var t = 0L
            for (line in lines) {
                val arg = line.substringAfter(' ', "")
                when {
                    line.startsWith("wait") -> t += (arg.toLongOrNull() ?: 0L) * 3 / 2
                    line.startsWith("press") -> parse(arg)?.let { input -> main.postDelayed({ press(input) }, t) }
                }
            }
            if (lines.isNotEmpty()) {
                val menuAt = (t - 2000).coerceAtLeast(14000)
                main.postDelayed({ TurnipSweep.capture(activity, File(logs, "autopilot_3_menu.jpg")) {} }, menuAt)
                repeat(3) {
                    t += 3000
                    main.postDelayed({ press(PadInput.A) }, t)
                }
            }
            main.postDelayed({
                AppLog.i("autopilot", "caminho repetido; autoteste em ${settleMs / 1000} s")
            }, t)
            // Com "attack" (22/10): golpes leves (X) e fortes (Y) durante a espera e o autoteste,
            // para a cena ter os efeitos dos golpes (o que pesa nas lutas) e nao o Raiden parado.
            if (attack) {
                var a = t + 2000
                val end = t + settleMs + 240_000
                while (a < end) {
                    main.postDelayed({ press(PadInput.X) }, a)
                    if ((a / 450) % 3 == 0L) main.postDelayed({ press(PadInput.Y) }, a + 200)
                    a += 450
                }
            }
            // Com captura: 2 frames gravados em .dfc a meio da espera, antes do autoteste.
            if (withCapture) main.postDelayed({
                AppLog.i("autopilot", "captura de frames")
                captureFrames()
            }, t + settleMs / 2)
            main.postDelayed({ selfTest() }, t + settleMs)
        }
    }

    /** Espera os primeiros frames do jogo (o mesmo ponto de partida da gravacao). */
    private fun whenRunning(action: () -> Unit) {
        main.post(object : Runnable {
            override fun run() {
                if (NativeGameState.frames() > 30) action() else main.postDelayed(this, 250)
            }
        })
    }

    companion object {
        const val ACTION = "com.dfdx047.swordman.CMD"
        const val SETTLE_MS = 30_000
        private var current: AutoPilot? = null

        /** GameActivity.dispatchKeyEvent: botoes do comando fisico (para a gravacao). */
        @JvmStatic
        fun onKeyEvent(event: KeyEvent) {
            current?.onKey(event)
        }

        fun parse(name: String): PadInput? = PadInput.entries.firstOrNull { it.name.equals(name.trim(), true) }

        fun fromKeyCode(code: Int): PadInput? = when (code) {
            KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_ENTER -> PadInput.A
            KeyEvent.KEYCODE_BUTTON_B -> PadInput.B
            KeyEvent.KEYCODE_BUTTON_X -> PadInput.X
            KeyEvent.KEYCODE_BUTTON_Y -> PadInput.Y
            KeyEvent.KEYCODE_BUTTON_L1 -> PadInput.LB
            KeyEvent.KEYCODE_BUTTON_R1 -> PadInput.RB
            KeyEvent.KEYCODE_BUTTON_START -> PadInput.START
            KeyEvent.KEYCODE_BUTTON_SELECT -> PadInput.BACK
            KeyEvent.KEYCODE_BUTTON_THUMBL -> PadInput.L3
            KeyEvent.KEYCODE_BUTTON_THUMBR -> PadInput.R3
            KeyEvent.KEYCODE_DPAD_UP -> PadInput.UP
            KeyEvent.KEYCODE_DPAD_DOWN -> PadInput.DOWN
            KeyEvent.KEYCODE_DPAD_LEFT -> PadInput.LEFT
            KeyEvent.KEYCODE_DPAD_RIGHT -> PadInput.RIGHT
            else -> null
        }
    }
}
