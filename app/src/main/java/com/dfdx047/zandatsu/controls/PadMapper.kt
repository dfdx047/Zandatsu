package com.dfdx047.zandatsu.controls

import android.content.Context
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import com.dfdx047.zandatsu.data.AppPaths
import org.json.JSONObject
import org.libsdl.app.SwordManPad
import java.io.File

/** Entradas do comando da Xbox que o jogo conhece. */
enum class PadTarget(val label: String, val key: Int = 0, val axis: Int = -1, val sign: Float = 1f) {
    A("A", SwordManPad.KEY_A), B("B", SwordManPad.KEY_B), X("X", SwordManPad.KEY_X), Y("Y", SwordManPad.KEY_Y),
    LB("LB", SwordManPad.KEY_LB), RB("RB", SwordManPad.KEY_RB),
    LT("LT", axis = 4), RT("RT", axis = 5),
    BACK("Back", SwordManPad.KEY_BACK), START("Start", SwordManPad.KEY_START),
    L3("L3", SwordManPad.KEY_L3), R3("R3", SwordManPad.KEY_R3),
    UP("D-Pad ↑", SwordManPad.KEY_UP), DOWN("D-Pad ↓", SwordManPad.KEY_DOWN),
    LEFT("D-Pad ←", SwordManPad.KEY_LEFT), RIGHT("D-Pad →", SwordManPad.KEY_RIGHT),
    LX("Analógico E ↔", axis = 0), LY("Analógico E ↕", axis = 1),
    RX("Analógico D ↔", axis = 2), RY("Analógico D ↕", axis = 3);

    val isStick get() = axis in 0..3
    val isTrigger get() = axis in 4..5
}

/** Uma fonte fisica: tecla, ou eixo (com sinal, para um eixo que faz de dois botoes, como o HAT). */
data class PadSource(val key: Int = 0, val axis: Int = -1, val sign: Int = 0) {
    fun encode() = if (axis >= 0) "a$axis:$sign" else "k$key"
    val label: String
        get() = if (axis >= 0) {
            MotionEvent.axisToString(axis).removePrefix("AXIS_") + when (sign) { 1 -> "+"; -1 -> "-"; else -> "" }
        } else {
            KeyEvent.keyCodeToString(key).removePrefix("KEYCODE_")
        }

    companion object {
        fun decode(s: String): PadSource? = runCatching {
            if (s.startsWith("a")) {
                val (a, b) = s.drop(1).split(":")
                PadSource(axis = a.toInt(), sign = b.toInt())
            } else {
                PadSource(key = s.drop(1).toInt())
            }
        }.getOrNull()
    }
}

typealias PadMap = Map<PadTarget, PadSource>

/** Mapeamentos guardados por comando (vendor:product:nome) e o mapeamento automatico. */
object PadMapper {
    private val dir get() = File(AppPaths.internalDir, "controls/pads")

    fun deviceKey(d: InputDevice) = "%04x_%04x_%s".format(d.vendorId, d.productId, d.name.replace(Regex("[^A-Za-z0-9]"), "_").take(40))
    private fun file(d: InputDevice) = File(dir, deviceKey(d) + ".json")

    fun isPad(d: InputDevice): Boolean {
        if (d.isVirtual) return false
        val s = d.sources
        return s and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
            s and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK
    }

    fun pads(): List<InputDevice> = InputDevice.getDeviceIds().toList().mapNotNull { InputDevice.getDevice(it) }.filter(::isPad)

    fun load(d: InputDevice): PadMap? {
        val f = file(d)
        if (!f.isFile) return null
        return try {
            val o = JSONObject(f.readText())
            PadTarget.values().mapNotNull { t ->
                val src = o.optString(t.name, "")
                PadSource.decode(src)?.let { t to it }
            }.toMap()
        } catch (e: Exception) {
            null
        }
    }

    fun save(d: InputDevice, map: PadMap) {
        val o = JSONObject()
        map.forEach { (t, s) -> o.put(t.name, s.encode()) }
        file(d).parentFile?.mkdirs()
        file(d).writeText(o.toString(2))
    }

    fun clear(d: InputDevice) {
        file(d).delete()
    }

    private fun axes(d: InputDevice) = d.motionRanges.filter { it.source and InputDevice.SOURCE_JOYSTICK != 0 }.map { it.axis }.distinct()

    /** Mesma ordem dos eixos do SDL (SDLControllerManager.RangeComparator): os "aN" do GameControllerDB sao indices nela. */
    private fun sdlAxisOrder(d: InputDevice): List<Int> {
        fun rank(a: Int): Int {
            var r = when (a) {
                MotionEvent.AXIS_GAS -> MotionEvent.AXIS_BRAKE
                MotionEvent.AXIS_BRAKE -> MotionEvent.AXIS_GAS
                else -> a
            }
            if (r == MotionEvent.AXIS_Z) r = MotionEvent.AXIS_RZ - 1
            else if (r > MotionEvent.AXIS_Z && r < MotionEvent.AXIS_RZ) r--
            return r
        }
        // O SDL trata o HAT como chapeu, fora da lista de eixos: contar com ele poe os gatilhos no HAT.
        return axes(d).filter { it != MotionEvent.AXIS_HAT_X && it != MotionEvent.AXIS_HAT_Y }.sortedBy(::rank)
    }

    /** Botoes "bN" do SDL no Android (SDL_GamepadButton) -> tecla do Android. */
    private val sdlButtonKeys = mapOf(
        0 to KeyEvent.KEYCODE_BUTTON_A, 1 to KeyEvent.KEYCODE_BUTTON_B, 2 to KeyEvent.KEYCODE_BUTTON_X,
        3 to KeyEvent.KEYCODE_BUTTON_Y, 4 to KeyEvent.KEYCODE_BUTTON_SELECT, 5 to KeyEvent.KEYCODE_BUTTON_MODE,
        6 to KeyEvent.KEYCODE_BUTTON_START, 7 to KeyEvent.KEYCODE_BUTTON_THUMBL, 8 to KeyEvent.KEYCODE_BUTTON_THUMBR,
        9 to KeyEvent.KEYCODE_BUTTON_L1, 10 to KeyEvent.KEYCODE_BUTTON_R1, 11 to KeyEvent.KEYCODE_DPAD_UP,
        12 to KeyEvent.KEYCODE_DPAD_DOWN, 13 to KeyEvent.KEYCODE_DPAD_LEFT, 14 to KeyEvent.KEYCODE_DPAD_RIGHT,
    )

    private val dbNames = mapOf(
        "a" to PadTarget.A, "b" to PadTarget.B, "x" to PadTarget.X, "y" to PadTarget.Y,
        "leftshoulder" to PadTarget.LB, "rightshoulder" to PadTarget.RB,
        "lefttrigger" to PadTarget.LT, "righttrigger" to PadTarget.RT,
        "back" to PadTarget.BACK, "start" to PadTarget.START,
        "leftstick" to PadTarget.L3, "rightstick" to PadTarget.R3,
        "dpup" to PadTarget.UP, "dpdown" to PadTarget.DOWN, "dpleft" to PadTarget.LEFT, "dpright" to PadTarget.RIGHT,
        "leftx" to PadTarget.LX, "lefty" to PadTarget.LY, "rightx" to PadTarget.RX, "righty" to PadTarget.RY,
    )

    /** Linha do GameControllerDB do SDL para este vendor/product (bytes 4-5 e 8-9 do GUID, little-endian). */
    fun dbEntry(ctx: Context, d: InputDevice): String? {
        if (d.vendorId == 0 && d.productId == 0) return null
        val v = "%02x%02x".format(d.vendorId and 0xFF, d.vendorId shr 8 and 0xFF)
        val p = "%02x%02x".format(d.productId and 0xFF, d.productId shr 8 and 0xFF)
        return runCatching {
            ctx.assets.open("gamecontrollerdb_android.txt").bufferedReader().useLines { lines ->
                lines.firstOrNull { !it.startsWith("#") && it.length > 20 && it.substring(8, 12) == v && it.substring(16, 20) == p }
            }
        }.getOrNull()
    }

    private fun fromDb(entry: String, d: InputDevice): PadMap {
        val order = sdlAxisOrder(d)
        val out = HashMap<PadTarget, PadSource>()
        for (part in entry.split(",").drop(2)) {
            val i = part.indexOf(':')
            if (i <= 0) continue
            val t = dbNames[part.substring(0, i)] ?: continue
            val v = part.substring(i + 1)
            val n = v.trimStart('+', '-', 'a', 'b').trimEnd('~').toIntOrNull() ?: continue
            when {
                v.startsWith("b") -> sdlButtonKeys[n]?.let { out[t] = PadSource(key = it) }
                v.startsWith("a") -> order.getOrNull(n)?.let { out[t] = PadSource(axis = it) }
            }
        }
        return out
    }

    /**
     * Automatico: o layout padrao do Android (o que as teclas e os eixos do aparelho dizem) e, por cima, a linha do
     * GameControllerDB se este comando la estiver. Devolve o mapa e se veio da base.
     */
    fun auto(ctx: Context, d: InputDevice): Pair<PadMap, Boolean> {
        val map = HashMap<PadTarget, PadSource>()
        val keys = mapOf(
            PadTarget.A to KeyEvent.KEYCODE_BUTTON_A, PadTarget.B to KeyEvent.KEYCODE_BUTTON_B,
            PadTarget.X to KeyEvent.KEYCODE_BUTTON_X, PadTarget.Y to KeyEvent.KEYCODE_BUTTON_Y,
            PadTarget.LB to KeyEvent.KEYCODE_BUTTON_L1, PadTarget.RB to KeyEvent.KEYCODE_BUTTON_R1,
            PadTarget.BACK to KeyEvent.KEYCODE_BUTTON_SELECT, PadTarget.START to KeyEvent.KEYCODE_BUTTON_START,
            PadTarget.L3 to KeyEvent.KEYCODE_BUTTON_THUMBL, PadTarget.R3 to KeyEvent.KEYCODE_BUTTON_THUMBR,
            PadTarget.UP to KeyEvent.KEYCODE_DPAD_UP, PadTarget.DOWN to KeyEvent.KEYCODE_DPAD_DOWN,
            PadTarget.LEFT to KeyEvent.KEYCODE_DPAD_LEFT, PadTarget.RIGHT to KeyEvent.KEYCODE_DPAD_RIGHT,
        )
        val has = d.hasKeys(*keys.values.toIntArray())
        keys.entries.forEachIndexed { i, (t, k) -> if (has[i]) map[t] = PadSource(key = k) }
        val ax = axes(d)
        fun put(t: PadTarget, vararg candidates: Int) {
            candidates.firstOrNull { it in ax }?.let { map[t] = PadSource(axis = it) }
        }
        put(PadTarget.LX, MotionEvent.AXIS_X)
        put(PadTarget.LY, MotionEvent.AXIS_Y)
        val triggersOnZ = MotionEvent.AXIS_RX in ax && MotionEvent.AXIS_Z in ax
        if (triggersOnZ) {
            put(PadTarget.RX, MotionEvent.AXIS_RX)
            put(PadTarget.RY, MotionEvent.AXIS_RY)
            put(PadTarget.LT, MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_BRAKE, MotionEvent.AXIS_Z)
            put(PadTarget.RT, MotionEvent.AXIS_RTRIGGER, MotionEvent.AXIS_GAS, MotionEvent.AXIS_RZ)
        } else {
            put(PadTarget.RX, MotionEvent.AXIS_Z, MotionEvent.AXIS_RX)
            put(PadTarget.RY, MotionEvent.AXIS_RZ, MotionEvent.AXIS_RY)
            put(PadTarget.LT, MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_BRAKE)
            put(PadTarget.RT, MotionEvent.AXIS_RTRIGGER, MotionEvent.AXIS_GAS)
        }
        if (PadTarget.LT !in map && d.hasKeys(KeyEvent.KEYCODE_BUTTON_L2)[0]) map[PadTarget.LT] = PadSource(key = KeyEvent.KEYCODE_BUTTON_L2)
        if (PadTarget.RT !in map && d.hasKeys(KeyEvent.KEYCODE_BUTTON_R2)[0]) map[PadTarget.RT] = PadSource(key = KeyEvent.KEYCODE_BUTTON_R2)
        if (MotionEvent.AXIS_HAT_X in ax && MotionEvent.AXIS_HAT_Y in ax) {
            map[PadTarget.LEFT] = PadSource(axis = MotionEvent.AXIS_HAT_X, sign = -1)
            map[PadTarget.RIGHT] = PadSource(axis = MotionEvent.AXIS_HAT_X, sign = 1)
            map[PadTarget.UP] = PadSource(axis = MotionEvent.AXIS_HAT_Y, sign = -1)
            map[PadTarget.DOWN] = PadSource(axis = MotionEvent.AXIS_HAT_Y, sign = 1)
        }
        val entry = dbEntry(ctx, d) ?: return map to false
        val db = fromDb(entry, d)
        // A base nao fala de HAT no Android: o D-pad em eixos fica como o aparelho diz.
        return (map + db.filterKeys { !(it in setOf(PadTarget.UP, PadTarget.DOWN, PadTarget.LEFT, PadTarget.RIGHT) && map[it]?.axis != -1) }) to true
    }
}

/** No jogo: traduz os eventos de um comando com mapeamento guardado para o comando virtual. */
object PadRouter {
    private val cache = HashMap<Int, PadMap?>()
    private val held = HashSet<PadTarget>()

    private fun map(id: Int, d: InputDevice): PadMap? = cache.getOrPut(id) { PadMapper.load(d) }

    fun onKey(ev: KeyEvent): Boolean {
        val d = ev.device ?: return false
        if (!PadMapper.isPad(d)) return false
        val m = map(ev.deviceId, d) ?: return false
        val t = m.entries.firstOrNull { it.value.axis < 0 && it.value.key == ev.keyCode }?.key
        // Tecla de comando sem acao neste mapa: nao vai para o SDL, senao continuava a fazer o que fazia antes.
        if (t == null) return KeyEvent.isGamepadButton(ev.keyCode) || isDpadKey(ev.keyCode)
        if (ev.repeatCount > 0) return true
        val down = ev.action == KeyEvent.ACTION_DOWN
        if (t.isTrigger) SwordManPad.axis(t.axis, if (down) 1f else -1f) else if (t.key != 0) SwordManPad.button(t.key, down)
        return true
    }

    private fun isDpadKey(code: Int) = code == KeyEvent.KEYCODE_DPAD_UP || code == KeyEvent.KEYCODE_DPAD_DOWN ||
        code == KeyEvent.KEYCODE_DPAD_LEFT || code == KeyEvent.KEYCODE_DPAD_RIGHT ||
        code == KeyEvent.KEYCODE_DPAD_CENTER

    fun onMotion(ev: MotionEvent): Boolean {
        if (ev.source and InputDevice.SOURCE_JOYSTICK != InputDevice.SOURCE_JOYSTICK) return false
        val d = ev.device ?: return false
        val m = map(ev.deviceId, d) ?: return false
        for ((t, s) in m) {
            if (s.axis < 0) continue
            val v = ev.getAxisValue(s.axis)
            when {
                t.isStick -> SwordManPad.axis(t.axis, v)
                t.isTrigger -> {
                    val range = d.getMotionRange(s.axis, ev.source)
                    // Gatilho 0..1, ou -1..1 nos que descansam a -1: o comando virtual quer -1..1.
                    SwordManPad.axis(t.axis, if (range != null && range.min < -0.5f) v else v * 2f - 1f)
                }
                else -> {
                    val on = if (s.sign != 0) v * s.sign > 0.5f else v > 0.5f
                    if (on != (t in held)) {
                        if (on) held.add(t) else held.remove(t)
                        SwordManPad.button(t.key, on)
                    }
                }
            }
        }
        return true
    }
}
