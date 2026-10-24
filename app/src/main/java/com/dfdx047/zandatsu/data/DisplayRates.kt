package com.dfdx047.zandatsu.data

import android.content.Context
import android.hardware.display.DisplayManager
import android.view.Display

/**
 * Taxas de atualizacao da tela (patch 0092). Os modos de 120 FPS so aparecem se a tela tiver um
 * modo de 110 Hz ou mais na resolucao atual (Odin 3: 120 Hz; RP5 e RG Cube: so 60 Hz).
 */
object DisplayRates {
    /** Abaixo disto nao vale a pena oferecer 120 FPS. */
    const val MIN_120_HZ = 110f

    private fun display(context: Context): Display? =
        (context.getSystemService(Context.DISPLAY_SERVICE) as? DisplayManager)
            ?.getDisplay(Display.DEFAULT_DISPLAY)

    /** Maior taxa da tela na resolucao atual (Hz), ou 60 se nao der para saber. */
    @JvmStatic
    fun maxRefreshHz(context: Context): Float {
        val d = display(context) ?: return 60f
        return maxRefreshHz(d)
    }

    @JvmStatic
    fun maxRefreshHz(d: Display): Float = try {
        val cur = d.mode
        d.supportedModes
            .filter { it.physicalWidth == cur.physicalWidth && it.physicalHeight == cur.physicalHeight }
            .maxOfOrNull { it.refreshRate } ?: d.refreshRate
    } catch (e: RuntimeException) {
        60f
    }

    @JvmStatic
    fun supports120(context: Context): Boolean = maxRefreshHz(context) >= MIN_120_HZ

    /**
     * O modo da tela (mesma resolucao) mais perto de [hz]: o exato (+-1 Hz) se existir; senao, para
     * 120, o mais alto ate 125 Hz e acima de 110; para 60, o de 60 Hz. null se nao houver.
     */
    @JvmStatic
    fun pickMode(d: Display, hz: Int): Display.Mode? {
        val cur = d.mode
        val same = d.supportedModes.filter {
            it.physicalWidth == cur.physicalWidth && it.physicalHeight == cur.physicalHeight
        }
        same.firstOrNull { kotlin.math.abs(it.refreshRate - hz) < 1f }?.let { return it }
        if (hz >= 100) {
            return same.filter { it.refreshRate >= MIN_120_HZ && it.refreshRate <= 125f }
                .maxByOrNull { it.refreshRate }
        }
        return null
    }

    /** Taxa de saida que a config pede (120 nos dois modos de 120 FPS, senao 60). */
    @JvmStatic
    fun wantedHz(context: Context): Int = try {
        AppPaths.init(context)
        GameConfig.load().frameMode.outputHz
    } catch (e: Exception) {
        60
    }
}
