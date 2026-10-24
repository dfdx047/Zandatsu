package com.dfdx047.zandatsu.game

import android.os.Handler
import android.os.HandlerThread
import android.os.SystemClock
import com.dfdx047.zandatsu.controls.ControlAction
import com.dfdx047.zandatsu.controls.MacroStep
import com.dfdx047.zandatsu.controls.PadInput
import org.libsdl.app.SwordManPad

/**
 * Junta tudo o que os controles por toque pedem (botoes premidos, os dois analogicos,
 * sequencias em curso) num estado de comando e envia so as diferencas para o
 * comando virtual do SDL. Corre a 60 Hz numa thread propria, como o jogo.
 * Tambem sincroniza a pausa (menus do app) e segue o estado jogo/menus quando o
 * runtime o souber (NativeGameState.fightFlag; no MGR ainda -1 = desconhecido).
 */
class HelperEngine {
    private val thread = HandlerThread("SwordManInput").apply { start() }
    private val handler = Handler(thread.looper)
    private val lock = Any()

    /** Entradas premidas diretamente, por elemento (id -> entradas). */
    private val held = HashMap<String, Set<PadInput>>()
    private class Running(val steps: List<MacroStep>, val holdLast: Boolean) {
        var index = 0
        var framesLeft = steps.firstOrNull()?.frames ?: 0
        var released = false
        val done get() = index >= steps.size
    }
    private val sequences = HashMap<String, Running>()
    private var stickX = 0f
    private var stickY = 0f
    private var lookX = 0f
    private var lookY = 0f
    private var sentRx = 0f
    private var sentRy = 0f

    private val sent = HashSet<PadInput>()
    private var sentLx = 0f
    private var sentLy = 0f
    private var running = false
    @Volatile private var side = NativeGameState.SIDE_UNKNOWN

    /** Menu dos controles aberto por cima do jogo: o jogo fica pausado. */
    @Volatile var menuOpen = false
        set(value) {
            field = value
            handler.post { step() }
        }

    /**
     * Imagem congelada (autoteste do render): o jogo fica parado no VdSwap e a GPU repete o
     * ultimo frame (patch 0105, cvar swordman_freeze_replay).
     */
    @Volatile var freeze = false
        set(value) {
            field = value
            handler.post { step() }
        }

    /** Pausar tambem enquanto o menu rapido do port (Back+Start) estiver aberto. */
    @Volatile var pauseQuickMenu = true

    /**
     * Luta ou menus (para trocar o layout dos controles). Chamado na thread principal
     * so quando muda. Com histerese: entra na luta logo que a bandeira do jogo vai a
     * 1 e so volta aos menus depois de [MENU_DELAY_TICKS] ticks seguidos a 0 (entre
     * assaltos, KO e apresentacao a bandeira tambem vai a 0).
     */
    var onBattleChanged: ((Boolean) -> Unit)? = null

    /**
     * Estado "comando completo" pedido pelo jogo (Dragon Burst: camara livre). No MGR o runtime
     * devolve 0 (nunca entra); fica para quando houver um estado equivalente.
     */
    var onMapChanged: ((Boolean) -> Unit)? = null
    @Volatile var onMap = false
        private set
    private var offMapTicks = 0

    /** Diagnostico (so com "Diagnostico de desempenho"): recebe linhas para logs/controles.log. */
    @Volatile var diag: ((String) -> Unit)? = null
    /** Ficheiro para os blocos dos lutadores (diagnostico do lado), ou null. */
    @Volatile var fightDumpPath: String? = null
    private var diagTicks = 0
    private var diagLast = ""
    @Volatile var inBattle = false
        private set
    private var zeroTicks = 0
    private val mainHandler = Handler(android.os.Looper.getMainLooper())

    /** Se o jogo esta pausado agora (pelo menu dos controles ou pelo menu rapido). */
    @Volatile var paused = false
        private set

    private val tick = object : Runnable {
        override fun run() {
            val start = SystemClock.uptimeMillis()
            step()
            if (running) handler.postAtTime(this, start + 16)
        }
    }

    fun start() {
        synchronized(lock) {
            if (running) return
            running = true
        }
        handler.post(tick)
    }

    fun stop() {
        synchronized(lock) {
            running = false
            held.clear()
            sequences.clear()
            stickX = 0f
            stickY = 0f
            lookX = 0f
            lookY = 0f
        }
        handler.removeCallbacks(tick)
        handler.post { flush(emptySet(), 0f, 0f, 0f, 0f) }
    }

    fun release() {
        stop()
        thread.quitSafely()
    }

    fun press(id: String, action: ControlAction) {
        synchronized(lock) {
            when (action) {
                is ControlAction.Press -> held[id] = action.inputs
                is ControlAction.Sequence -> if (action.steps.isNotEmpty()) sequences[id] = Running(action.steps, action.holdLast)
            }
        }
        // Resposta imediata para botoes simples (nao espera pelo proximo tick).
        if (action is ControlAction.Press) handler.post { step() }
    }

    fun releaseElement(id: String) {
        synchronized(lock) {
            held.remove(id)
            sequences[id]?.let { seq ->
                seq.released = true
                // Sequencia com o ultimo passo "segurado": soltar o dedo acaba-a.
                if (seq.holdLast && seq.index >= seq.steps.size - 1) sequences.remove(id)
            }
        }
        handler.post { step() }
    }

    fun setStick(x: Float, y: Float) {
        synchronized(lock) {
            stickX = x.coerceIn(-1f, 1f)
            stickY = y.coerceIn(-1f, 1f)
        }
    }

    /** Analogico direito (camara): zona da camara ou o "rstick" do layout completo. */
    fun setLook(x: Float, y: Float) {
        synchronized(lock) {
            lookX = x.coerceIn(-1f, 1f)
            lookY = y.coerceIn(-1f, 1f)
        }
        handler.post { step() }
    }

    /** R3 por um instante (toque rapido na zona da camara: centrar a camara). */
    fun tapR3() {
        press("look_tap", ControlAction.Sequence(listOf(MacroStep(setOf(PadInput.R3), 4)), holdLast = false))
        handler.post { step() }
    }

    /** Uma "pressao" da direcao do D-pad do ecra (ou do analogico em modo digital). */
    fun setDpad(id: String, inputs: Set<PadInput>) {
        synchronized(lock) {
            if (inputs.isEmpty()) held.remove(id) else held[id] = inputs
        }
        handler.post { step() }
    }

    private fun step() {
        paused = NativeGameState.syncPause(menuOpen || freeze, pauseQuickMenu)
        side = NativeGameState.p1Side()
        if (!paused) {
            trackBattle(NativeGameState.fightFlag())
            trackMap(NativeGameState.mapFlag())
        }
        diag?.let { d ->
            if (++diagTicks >= 30) {
                diagTicks = 0
                fightDumpPath?.let { NativeGameState.dumpFight(it, 0) }
                val line = NativeGameState.battleDebug() ?: ""
                if (line != diagLast) {
                    diagLast = line
                    d("$line layout=${if (onMap) "mapa" else if (inBattle) "luta" else "menus"}${if (paused) " (pausa)" else ""}")
                }
            }
        }
        val wanted = HashSet<PadInput>()
        val lx: Float
        val ly: Float
        val rx: Float
        val ry: Float
        synchronized(lock) {
            held.values.forEach { wanted.addAll(it) }
            val it = sequences.entries.iterator()
            while (it.hasNext()) {
                val (_, seq) = it.next()
                if (seq.done) {
                    it.remove()
                    continue
                }
                val stepNow = seq.steps[seq.index]
                wanted.addAll(stepNow.inputs)
                val last = seq.index == seq.steps.size - 1
                if (last && seq.holdLast && !seq.released) continue // segura ate soltar
                seq.framesLeft--
                if (seq.framesLeft <= 0) {
                    seq.index++
                    seq.framesLeft = seq.steps.getOrNull(seq.index)?.frames ?: 0
                }
            }
            lx = stickX
            ly = stickY
            rx = lookX
            ry = lookY
        }
        flush(resolveRelative(wanted), lx, ly, rx, ry)
    }

    private fun trackBattle(flag: Int) {
        if (flag < 0) return
        val next = when {
            flag == 1 -> {
                zeroTicks = 0
                true
            }
            inBattle && ++zeroTicks < MENU_DELAY_TICKS -> true
            else -> false
        }
        if (next != inBattle) {
            inBattle = next
            com.dfdx047.zandatsu.data.AppLog.i("controls", "estado: " + if (next) "jogo" else "menus")
            diag?.invoke("troca -> " + (if (next) "luta" else "menus") + " (bandeira=$flag)")
            mainHandler.post { onBattleChanged?.invoke(next) }
        }
    }

    private fun trackMap(flag: Int) {
        if (flag < 0) return
        val next = when {
            flag == 1 -> {
                offMapTicks = 0
                true
            }
            onMap && ++offMapTicks < MAP_DELAY_TICKS -> true
            else -> false
        }
        if (next != onMap) {
            onMap = next
            com.dfdx047.zandatsu.data.AppLog.i("controls", "comando completo: " + if (next) "sim" else "nao")
            diag?.invoke("troca -> " + if (next) "mapa" else "fora do mapa")
            mainHandler.post { onMapChanged?.invoke(next) }
        }
    }

    /** FORWARD/BACKWARD -> RIGHT/LEFT conforme o lado do P1 (esquerda se desconhecido). */
    private fun resolveRelative(inputs: Set<PadInput>): Set<PadInput> {
        if (PadInput.FORWARD !in inputs && PadInput.BACKWARD !in inputs) return inputs
        val p1Right = side == NativeGameState.SIDE_RIGHT
        val out = inputs.toMutableSet()
        if (out.remove(PadInput.BACKWARD)) out += if (p1Right) PadInput.RIGHT else PadInput.LEFT
        if (out.remove(PadInput.FORWARD)) out += if (p1Right) PadInput.LEFT else PadInput.RIGHT
        return out
    }

    private fun flush(wanted: Set<PadInput>, lx: Float, ly: Float, rx: Float, ry: Float) {
        if (!SwordManPad.isAttached()) {
            sent.clear()
            return
        }
        for (i in PadInput.entries) {
            val on = i in wanted
            if (on == (i in sent)) continue
            when (i) {
                PadInput.LT -> SwordManPad.axis(4, if (on) 1f else -1f)
                PadInput.RT -> SwordManPad.axis(5, if (on) 1f else -1f)
                else -> keyFor(i)?.let { SwordManPad.button(it, on) }
            }
            if (on) sent += i else sent -= i
        }
        if (lx != sentLx) {
            SwordManPad.axis(0, lx)
            sentLx = lx
        }
        if (ly != sentLy) {
            SwordManPad.axis(1, ly)
            sentLy = ly
        }
        if (rx != sentRx) {
            SwordManPad.axis(2, rx)
            sentRx = rx
        }
        if (ry != sentRy) {
            SwordManPad.axis(3, ry)
            sentRy = ry
        }
    }

    companion object {
        /** 1,5 s a 60 Hz (pedido do usuario: no maximo ~2 s). */
        const val MENU_DELAY_TICKS = 90
        /** ~1,5 s fora do estado de voo antes de sair do layout do mapa. */
        const val MAP_DELAY_TICKS = 90
    }

    private fun keyFor(i: PadInput): Int? = when (i) {
        PadInput.A -> SwordManPad.KEY_A
        PadInput.B -> SwordManPad.KEY_B
        PadInput.X -> SwordManPad.KEY_X
        PadInput.Y -> SwordManPad.KEY_Y
        PadInput.LB -> SwordManPad.KEY_LB
        PadInput.RB -> SwordManPad.KEY_RB
        PadInput.BACK -> SwordManPad.KEY_BACK
        PadInput.START -> SwordManPad.KEY_START
        PadInput.L3 -> SwordManPad.KEY_L3
        PadInput.R3 -> SwordManPad.KEY_R3
        PadInput.UP -> SwordManPad.KEY_UP
        PadInput.DOWN -> SwordManPad.KEY_DOWN
        PadInput.LEFT -> SwordManPad.KEY_LEFT
        PadInput.RIGHT -> SwordManPad.KEY_RIGHT
        else -> null
    }
}
