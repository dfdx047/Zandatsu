package com.dfdx047.swordman.game

/**
 * Ponte com o jogo (game_state_jni.cpp, em libmain.so): pausa, cvars ao vivo, volume,
 * log e o estado jogo/menus (ainda desconhecido no MGR: fightFlag devolve -1).
 */
object NativeGameState {
    const val SIDE_UNKNOWN = 0
    /** P1 a esquerda do adversario: "para tras" = esquerda. */
    const val SIDE_LEFT = 1
    /** P1 a direita do adversario: "para tras" = direita. */
    const val SIDE_RIGHT = 2

    @Volatile
    private var available = true

    /** Lado do P1 neste momento (SIDE_*). */
    fun p1Side(): Int {
        if (!available) return SIDE_UNKNOWN
        return try {
            nativeP1Side()
        } catch (e: UnsatisfiedLinkError) {
            available = false
            SIDE_UNKNOWN
        }
    }

    /**
     * Pausa (GPU + audio) enquanto [overlayOpen] ou, com [pauseQuickMenu], enquanto o
     * menu rapido do port estiver aberto. Devolve se o jogo esta pausado.
     */
    fun syncPause(overlayOpen: Boolean, pauseQuickMenu: Boolean): Boolean {
        if (!available) return false
        return try {
            nativeSyncPause(overlayOpen, pauseQuickMenu)
        } catch (e: UnsatisfiedLinkError) {
            available = false
            false
        }
    }

    /** 1 = luta a decorrer, 0 = fora da luta, -1 = desconhecido (jogo ainda a arrancar). */
    fun fightFlag(): Int {
        if (!available) return -1
        return try {
            nativeFightFlag()
        } catch (e: UnsatisfiedLinkError) {
            available = false
            -1
        }
    }

    /** 1 = camara livre (modo foto) ligada, 0 = nao, -1 = desconhecido. */
    fun mapFlag(): Int {
        if (!available) return -1
        return try {
            nativeMapFlag()
        } catch (e: UnsatisfiedLinkError) {
            // libmain.so antiga sem esta funcao: so desliga a deteccao do mapa.
            -1
        }
    }

    /** Linha de diagnostico da deteccao luta/menus (bandeira, configuracao, posicoes). */
    fun battleDebug(): String? {
        if (!available) return null
        return try {
            nativeBattleDebug()
        } catch (e: UnsatisfiedLinkError) {
            null
        }
    }

    /** Grava os blocos dos lutadores em [path] (diagnostico do lado do P1). */
    fun dumpFight(path: String, kind: Int) {
        if (!available) return
        try {
            nativeDumpFight(path, kind, System.currentTimeMillis())
        } catch (_: UnsatisfiedLinkError) {
        }
    }

    /** Muda opcoes do jogo ao vivo e grava o mgr.toml. */
    /** persist = false: so ao vivo, sem gravar no mgr.toml (autoteste, imagem congelada). */
    fun setCvars(values: Map<String, String>, persist: Boolean = true): Boolean {
        if (!available || values.isEmpty()) return false
        return try {
            nativeSetCvars(values.keys.toTypedArray(), values.values.toTypedArray(), persist)
        } catch (e: UnsatisfiedLinkError) {
            available = false
            false
        }
    }

    /** Valor atual de uma cvar do jogo ("" sem jogo). */
    fun getCvar(name: String): String {
        if (!available) return ""
        return try {
            nativeGetCvar(name)
        } catch (e: UnsatisfiedLinkError) {
            ""
        }
    }

    /** Corre um comando do jogo (ex.: save_export, save_import <zip>). */
    fun invokeCommand(name: String, args: String = ""): Boolean {
        if (!available) return false
        return try {
            nativeInvokeCommand(name, args)
        } catch (e: UnsatisfiedLinkError) {
            false
        }
    }

    /** Som de menu do proprio jogo: 0 mover, 1 confirmar, 2 voltar, 3 nao da, 5 abrir, 6 fechar. */
    fun menuSound(sound: Int) {
        if (!available) return
        try {
            nativeMenuSound(sound)
        } catch (_: UnsatisfiedLinkError) {
        }
    }

    /** Escreve uma linha no log do jogo (swordman_last.log). */
    fun logLine(line: String) {
        if (!available) return
        try {
            nativeLogLine(line)
        } catch (_: UnsatisfiedLinkError) {
        }
    }

    /**
     * Taxa real da tela (Hz) para o presenter (patch 0092): so gera quadros interpolados se a tela
     * tiver pelo menos ~1,8x os FPS do jogo. Pode ser chamado antes do jogo arrancar.
     */
    fun setDisplayHz(hz: Float) {
        try {
            nativeSetDisplayHz(hz)
        } catch (_: UnsatisfiedLinkError) {
        }
    }

    /** Como os quadros chegam a tela: modo de apresentacao, Hz, interpolacao (autoteste). */
    fun presentInfo(): String {
        if (!available) return ""
        return try {
            nativePresentInfo()
        } catch (_: UnsatisfiedLinkError) {
            ""
        }
    }

    /** Volume geral 0-100 (ganho do audio do SDK). Pode ser chamado antes do jogo arrancar. */
    fun setVolume(percent: Int) {
        try {
            nativeSetVolume(percent.coerceIn(0, 100))
        } catch (_: UnsatisfiedLinkError) {
        }
    }

    /** Frames apresentados desde o arranque (0 sem jogo): o launcher sabe se o jogo chegou a abrir. */
    fun frames(): Long {
        if (!available) return 0
        return try {
            nativeFrames()
        } catch (_: UnsatisfiedLinkError) {
            0
        }
    }

    /** Tempo de GPU acumulado: [frames medidos, microssegundos] (0107; zeros sem jogo). */
    fun gpuStats(): LongArray {
        if (!available) return LongArray(2)
        return try {
            nativeGpuStats()
        } catch (_: UnsatisfiedLinkError) {
            LongArray(2)
        }
    }

    /** Pixel shaders saltados (hashes separados por virgula; "" = nenhum), sem passar pela cvar. */
    fun setSkipPs(hashes: String) {
        if (!available) return
        try {
            nativeSetSkipPs(hashes)
        } catch (_: UnsatisfiedLinkError) {
        }
    }

    /** Censo de pixel shaders desde o ultimo reset: "HASH:draws:pitch:prim,..." por draws. */
    fun psCensus(reset: Boolean): String {
        if (!available) return ""
        return try {
            nativePsCensus(reset)
        } catch (_: UnsatisfiedLinkError) {
            ""
        }
    }

    const val SOUND_MOVE = 0
    const val SOUND_CONFIRM = 1
    const val SOUND_BACK = 2
    const val SOUND_DENIED = 3
    const val SOUND_OPEN = 5
    const val SOUND_CLOSE = 6

    /** Solta a pausa sem condicoes (ao fechar o jogo). */
    fun forceResume() {
        if (!available) return
        try {
            nativeForceResume()
        } catch (e: UnsatisfiedLinkError) {
            available = false
        }
    }

    @JvmStatic
    private external fun nativeForceResume()

    @JvmStatic private external fun nativeGetCvar(name: String): String
    @JvmStatic private external fun nativeInvokeCommand(name: String, args: String): Boolean
    @JvmStatic private external fun nativeMenuSound(sound: Int)
    @JvmStatic private external fun nativeLogLine(line: String)
    @JvmStatic private external fun nativeSetDisplayHz(hz: Float)
    @JvmStatic private external fun nativePresentInfo(): String
    @JvmStatic private external fun nativeSetVolume(percent: Int)
    @JvmStatic private external fun nativeFrames(): Long
    @JvmStatic private external fun nativeGpuStats(): LongArray
    @JvmStatic private external fun nativeSetSkipPs(hashes: String)
    @JvmStatic private external fun nativePsCensus(reset: Boolean): String

    @JvmStatic
    private external fun nativeFightFlag(): Int
    @JvmStatic private external fun nativeMapFlag(): Int
    @JvmStatic private external fun nativeBattleDebug(): String
    @JvmStatic private external fun nativeDumpFight(path: String, kind: Int, ms: Long): Boolean

    @JvmStatic
    private external fun nativeSetCvars(names: Array<String>, values: Array<String>, persist: Boolean): Boolean

    @JvmStatic
    private external fun nativeP1Side(): Int

    @JvmStatic
    private external fun nativeSyncPause(overlayOpen: Boolean, pauseQuickMenu: Boolean): Boolean
}
