package com.dfdx047.zandatsu.data

import android.content.Context
import org.json.JSONArray
import java.io.File
import java.text.DateFormat
import java.util.Date
import java.util.Locale

data class AchievementItem(
    val id: Int,
    val label: String,
    val description: String,
    val unachieved: String,
    val gamerscore: Int,
    val flags: Int,
    val unlocked: Boolean,
    val unlockedAtMs: Long?,
) {
    /** Xbox 360 XACHIEVEMENT_DETAILS_SHOWUNACHIEVED (bit 3 = 0x8): quando ausente, e uma conquista secreta. */
    val isSecret: Boolean get() = (flags and 0x00000008) == 0

    fun formattedDate(): String? {
        val ms = unlockedAtMs ?: return null
        if (ms <= 0L) return null
        return try {
            DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.SHORT, Locale.getDefault()).format(Date(ms))
        } catch (_: Exception) {
            null
        }
    }
}

data class AchievementsSummary(
    val items: List<AchievementItem>,
    val gameInstalled: Boolean,
) {
    val totalCount: Int get() = items.size
    val unlockedCount: Int get() = items.count { it.unlocked }
    val totalScore: Int get() = items.sumOf { it.gamerscore }
    val unlockedScore: Int get() = items.filter { it.unlocked }.sumOf { it.gamerscore }
}

/**
 * Conquistas (issue #91): le os metadados XDBF diretamente do default.xex do jogador (via libmain.so, com cache em
 * cacheDir para abrir instantaneamente) e junta o estado desbloqueado gravado pelo runtime em
 * `<appDir>/mgr/achievements/4B4E080A.toml`. Nenhuma string do jogo fica embutida no APK.
 */
object AchievementsRepo {
    private const val TITLE_ID = "4B4E080A"
    private const val FILETIME_EPOCH_DIFF = 116444736000000000L

    private fun xLanguageForLocale(context: Context): Int {
        val lang = try {
            context.resources.configuration.locales[0].language.lowercase(Locale.ROOT)
        } catch (_: Exception) {
            Locale.getDefault().language.lowercase(Locale.ROOT)
        }
        return when (lang) {
            "en" -> 1
            "ja" -> 2
            "de" -> 3
            "fr" -> 4
            "es" -> 5
            "it" -> 6
            "ko" -> 7
            "pt" -> 9
            "zh" -> 10
            "pl" -> 11
            "ru" -> 12
            else -> 1
        }
    }

    private fun loadMetadataJson(context: Context, xex: File): String? {
        val langId = xLanguageForLocale(context)
        val cacheName = "achievements_${langId}_${xex.length()}_${xex.lastModified()}.json"
        val cacheFile = File(AppPaths.cacheDir, cacheName)
        if (cacheFile.isFile && cacheFile.length() > 2) {
            runCatching { return cacheFile.readText() }
        }
        val json = GameLib.readAchievementsJson(xex, langId) ?: return null
        if (json.length > 2) {
            runCatching {
                AppPaths.cacheDir.listFiles { _, name -> name.startsWith("achievements_") && name.endsWith(".json") }
                    ?.forEach { it.delete() }
                cacheFile.writeText(json)
            }
        }
        return json
    }

    private fun loadUnlockedMap(): Map<Int, Long> {
        val candidates = listOf(
            File(AppPaths.appDir, "mgr/achievements/$TITLE_ID.toml"),
            File(AppPaths.gameDir, "achievements/$TITLE_ID.toml"),
            File(AppPaths.appDir, "achievements/$TITLE_ID.toml"),
        )
        val out = LinkedHashMap<Int, Long>()
        for (f in candidates) {
            if (!f.isFile) continue
            val map = runCatching { TomlLite.read(f) }.getOrNull() ?: continue
            // Formato atual do SDK: [unlocked.<id>]\nfiletime = <u64> -> TomlLite achata para "unlocked_<id>_filetime".
            for ((k, v) in map) {
                if (k.startsWith("unlocked_") && k.endsWith("_filetime")) {
                    val id = k.removePrefix("unlocked_").removeSuffix("_filetime").toIntOrNull() ?: continue
                    val ft = (v as? TomlLite.Value.Num)?.v ?: 0L
                    val ms = if (ft > FILETIME_EPOCH_DIFF) (ft - FILETIME_EPOCH_DIFF) / 10_000L else 0L
                    out[id] = maxOf(out[id] ?: 0L, ms)
                } else if (k == "unlocked" && v is TomlLite.Value.Raw) {
                    // Formato legado: unlocked = [1, 2, 3]
                    Regex("\\d+").findAll(v.v).forEach { m ->
                        m.value.toIntOrNull()?.let { id -> out.putIfAbsent(id, 0L) }
                    }
                }
            }
        }
        return out
    }

    fun load(context: Context): AchievementsSummary {
        val xex = AppPaths.xexFile
        if (!xex.isFile) return AchievementsSummary(emptyList(), gameInstalled = false)
        val json = loadMetadataJson(context, xex) ?: return AchievementsSummary(emptyList(), gameInstalled = true)
        val unlocked = loadUnlockedMap()
        val list = ArrayList<AchievementItem>()
        runCatching {
            val arr = JSONArray(json)
            for (i in 0 until arr.length()) {
                val o = arr.getJSONObject(i)
                val id = o.optInt("id", 0)
                val isUnlocked = unlocked.containsKey(id)
                val ms = unlocked[id]?.takeIf { it > 0L }
                list += AchievementItem(
                    id = id,
                    label = o.optString("label", "#$id").ifBlank { "#$id" },
                    description = o.optString("desc", ""),
                    unachieved = o.optString("unachieved", ""),
                    gamerscore = o.optInt("score", 0),
                    flags = o.optInt("flags", 0),
                    unlocked = isUnlocked,
                    unlockedAtMs = ms,
                )
            }
        }
        return AchievementsSummary(list, gameInstalled = true)
    }
}
