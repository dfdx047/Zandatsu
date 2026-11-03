package com.dfdx047.zandatsu.data

import android.content.Context
import android.view.WindowManager

/**
 * A tela deste aparelho, lida ao abrir a app (launcher e processo do jogo): a proporcao "auto" e a opcao "usar a
 * resolucao da tela" (02/11) partem daqui. As barras em si ja saem da janela real (FitFrame, backend/present.cpp).
 */
object ScreenInfo {
    /** Em pixels, deitada; 0 antes do init. */
    var long = 0
        private set
    var short = 0
        private set

    fun init(context: Context) {
        val m = context.resources.displayMetrics
        val bounds = runCatching {
            context.getSystemService(WindowManager::class.java).maximumWindowMetrics.bounds
        }.getOrNull()
        val w = bounds?.width() ?: m.widthPixels
        val h = bounds?.height() ?: m.heightPixels
        long = maxOf(w, h)
        short = minOf(w, h)
    }

    /** A mesma margem do DisplayRates.isWiderThan16x9. */
    val widerThan16x9: Boolean
        get() = short > 0 && long.toFloat() / short > 16f / 9f + 0.02f

    /**
     * O Renut so tem alturas fixas (render_scale.h): a menor que cobre a tela. Ex.: 1080 numa tela de 1080, 1440 numa
     * de 1220 (nao ha 1220). Sem tela lida: 720p.
     */
    fun matchedResolution(): InternalResolution {
        if (short <= 0) return InternalResolution.P720
        return HEIGHTS.firstOrNull { it.first >= short }?.second ?: InternalResolution.P2160
    }

    private val HEIGHTS = listOf(
        360 to InternalResolution.P360,
        540 to InternalResolution.P540,
        720 to InternalResolution.P720,
        1080 to InternalResolution.P1080,
        1440 to InternalResolution.P1440,
        2160 to InternalResolution.P2160,
    )
}
