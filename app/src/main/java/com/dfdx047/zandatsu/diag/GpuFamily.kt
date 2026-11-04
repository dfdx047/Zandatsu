package com.dfdx047.zandatsu.diag

import java.util.Locale

/**
 * A familia da GPU (pelo GL_RENDERER) e se ela tem problema conhecido nas issues (04/11): Mali-G615 (crash do
 * compilador, lenta), Mali-G710/G720/G925 (tela cortada, minimapa), Mali-G57 MC2 (o Nativo perde o dispositivo),
 * PowerVR (crash ao criar pipelines em paralelo), Adreno 7xx com o driver do sistema e Xclipse.
 */
data class GpuFamily(val name: String, val family: String, val knownProblem: Boolean) {
    companion object {
        private fun socModel(): String =
            if (android.os.Build.VERSION.SDK_INT >= 31) android.os.Build.SOC_MODEL.orEmpty() else android.os.Build.HARDWARE.orEmpty()

        /** SoCs com Mali-G57 MC2 (o GL_RENDERER so diz "Mali-G57"): Helio G99/G96, Dimensity 700/6020/6080/6100+. */
        private val G57_MC2 = setOf("MT6789", "MT8781", "MT6781", "MT6833", "MT6835")

        fun of(glRenderer: String, customDriver: Boolean, soc: String = socModel()): GpuFamily {
            val name0 = glRenderer.trim().ifEmpty { "?" }
            val name = if (Regex("""(?i)mali-g57\b""").containsMatchIn(name0) && "mc" !in name0.lowercase(Locale.ROOT) &&
                soc.uppercase(Locale.ROOT) in G57_MC2) "$name0 MC2" else name0
            val n = name.lowercase(Locale.ROOT)
            val mali = Regex("""(?:mali|immortalis)-g(\d+)""").find(n)?.groupValues?.get(1)
            val adreno = Regex("""adreno[^\d]*(\d+)""").find(n)?.groupValues?.get(1)
            return when {
                mali != null -> {
                    val known = mali in setOf("615", "710", "715", "720", "925") || (mali == "57" && "mc2" in n)
                    GpuFamily(name, "mali-g$mali" + if (mali == "57" && "mc2" in n) "-mc2" else "", known)
                }
                "powervr" in n || n.startsWith("img ") || "bxm" in n -> GpuFamily(name, "powervr", true)
                "xclipse" in n -> GpuFamily(name, "xclipse", true)
                adreno != null -> {
                    val series = adreno.first() + "xx"
                    GpuFamily(name, "adreno-$series", series == "7xx" && !customDriver)
                }
                else -> GpuFamily(name, n.substringBefore(' ').ifEmpty { "?" }, false)
            }
        }
    }
}
