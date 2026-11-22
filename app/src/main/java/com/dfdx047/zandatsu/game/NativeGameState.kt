package com.dfdx047.zandatsu.game

/**
 * Ponte com o jogo (game_state_jni.cpp, em libgame.so): pausa, cvars ao vivo, volume,
 * log e o estado jogo/menus (ainda desconhecido no MGR: fightFlag devolve -1).
 */
object NativeGameState {
    @Volatile
    private var available = true

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

    /** Modo do jogo (game_state_jni.cpp): 0 titulo, 1 em jogo, 3 pausa, outros menus; -1 desconhecido. */
    fun playMode(): Int {
        if (!available) return -1
        return try {
            nativePlayMode()
        } catch (e: UnsatisfiedLinkError) {
            -1
        }
    }

    /** A GPU travou/falhou (gpu_reset.cpp): o motivo; "" enquanto ela estiver bem. */
    @JvmStatic
    fun gpuReset(): String {
        if (!available) return ""
        return try {
            nativeGpuReset()
        } catch (e: UnsatisfiedLinkError) {
            ""
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

    /** Refreshes de 60 Hz por frame que o jogo pede (1 = 60 fps, 2 = 30, 3 = 20); 1 sem o jogo ou sem o Renut. */
    fun frameRefreshes(): Int {
        if (!available) return 1
        return try {
            nativeFrameRefreshes()
        } catch (_: UnsatisfiedLinkError) {
            1
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
    @JvmStatic private external fun nativeLogLine(line: String)
    @JvmStatic private external fun nativeSetDisplayHz(hz: Float)
    @JvmStatic private external fun nativePresentInfo(): String
    @JvmStatic private external fun nativeSetVolume(percent: Int)
    @JvmStatic private external fun nativeFrames(): Long
    @JvmStatic private external fun nativeGpuReset(): String
    @JvmStatic private external fun nativeGpuStats(): LongArray
    @JvmStatic private external fun nativeFrameRefreshes(): Int
    @JvmStatic private external fun nativeSetSkipPs(hashes: String)
    @JvmStatic private external fun nativePsCensus(reset: Boolean): String

    @JvmStatic
    private external fun nativeFightFlag(): Int
    @JvmStatic private external fun nativePlayMode(): Int
    @JvmStatic private external fun nativeBattleDebug(): String
    @JvmStatic private external fun nativeDumpFight(path: String, kind: Int, ms: Long): Boolean

    @JvmStatic
    private external fun nativeSetCvars(names: Array<String>, values: Array<String>, persist: Boolean): Boolean

    @JvmStatic
    private external fun nativeSyncPause(overlayOpen: Boolean, pauseQuickMenu: Boolean): Boolean
}
