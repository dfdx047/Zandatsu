package com.dfdx047.zandatsu.pc.game

/**
 * Jogo de PC: sem a ponte do libgame do 360 (game_state_jni.cpp). O HelperEngine copiado da app do 360 pergunta o
 * estado do jogo aqui; no PC a pausa vai pelo PcGameActivity.nativePause e o estado jogo/menus e desconhecido (-1):
 * a troca de layout fica manual (botao de troca).
 */
object NativeGameState {
    const val SIDE_UNKNOWN = 0
    const val SIDE_LEFT = 1
    const val SIDE_RIGHT = 2

    fun p1Side(): Int = SIDE_UNKNOWN
    fun syncPause(overlayOpen: Boolean, pauseQuickMenu: Boolean): Boolean = overlayOpen && pauseQuickMenu
    fun playMode(): Int = -1
    fun mapFlag(): Int = 0
    fun dumpFight(path: String, kind: Int) {}
    fun battleDebug(): String? = null
}
