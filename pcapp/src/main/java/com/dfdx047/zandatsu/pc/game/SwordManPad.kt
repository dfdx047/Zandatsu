package com.dfdx047.zandatsu.pc.game

import android.view.KeyEvent

/**
 * Comando virtual dos controles por toque (copia da app do 360 sem o SDL): o jogo de PC nao tem SDL, os botoes e
 * eixos vao para o [Sink] do PcGameActivity (nativeTouchPad).
 *
 * Eixos: 0 LX, 1 LY, 2 RX, 3 RY, 4 LT, 5 RT (gatilhos de -1 solto a 1 premido).
 */
object SwordManPad {
    interface Sink {
        fun button(keycode: Int, down: Boolean)
        fun axis(axis: Int, value: Float)
    }

    @Volatile private var sink: Sink? = null
    @Volatile private var attached = false

    fun setSink(s: Sink?) { sink = s }
    @Synchronized fun isAttached(): Boolean = attached
    @Synchronized fun attach(): Boolean { attached = sink != null; return attached }
    @Synchronized fun detach() { attached = false }

    fun button(keycode: Int, down: Boolean) {
        if (attached) sink?.button(keycode, down)
    }

    fun axis(axis: Int, value: Float) {
        if (attached) sink?.axis(axis, value)
    }

    const val KEY_A = KeyEvent.KEYCODE_BUTTON_A
    const val KEY_B = KeyEvent.KEYCODE_BUTTON_B
    const val KEY_X = KeyEvent.KEYCODE_BUTTON_X
    const val KEY_Y = KeyEvent.KEYCODE_BUTTON_Y
    const val KEY_LB = KeyEvent.KEYCODE_BUTTON_L1
    const val KEY_RB = KeyEvent.KEYCODE_BUTTON_R1
    const val KEY_BACK = KeyEvent.KEYCODE_BUTTON_SELECT
    const val KEY_START = KeyEvent.KEYCODE_BUTTON_START
    const val KEY_L3 = KeyEvent.KEYCODE_BUTTON_THUMBL
    const val KEY_R3 = KeyEvent.KEYCODE_BUTTON_THUMBR
    const val KEY_UP = KeyEvent.KEYCODE_DPAD_UP
    const val KEY_DOWN = KeyEvent.KEYCODE_DPAD_DOWN
    const val KEY_LEFT = KeyEvent.KEYCODE_DPAD_LEFT
    const val KEY_RIGHT = KeyEvent.KEYCODE_DPAD_RIGHT
}
