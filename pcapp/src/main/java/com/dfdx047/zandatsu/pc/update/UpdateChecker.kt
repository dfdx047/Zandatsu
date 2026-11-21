package com.dfdx047.zandatsu.pc.update

import com.dfdx047.zandatsu.pc.BuildConfig
import com.dfdx047.zandatsu.pc.Links
import com.dfdx047.zandatsu.pc.data.Http
import org.json.JSONArray

data class UpdateInfo(
    val version: String,
    val title: String,
    val changelogPt: String,
    val changelogEn: String,
    val changelogEs: String,
    val pageUrl: String,
    val apkUrl: String?,
) {
    /** language = codigo ISO do idioma do app ("pt", "es", "en"...); cai para o ingles. */
    fun changelog(language: String) = when (language) {
        "pt" -> changelogPt.ifBlank { changelogEn }
        "es" -> changelogEs.ifBlank { changelogEn }
        else -> changelogEn.ifBlank { changelogPt }
    }
}

/**
 * Atualizacoes da app de PC (copia do UpdateChecker da app do 360). As releases do repositorio sao partilhadas com a
 * app do 360: so contam as que trazem um APK com "pc" no nome; a versao vem da tag ("pc-0.2", "v0.2-pc") ou, se a tag
 * for a da app do 360, do nome do APK ("Zandatsu-PC-0.2.apk"). O APK abre-se no navegador (sem instalador proprio).
 */
object UpdateChecker {
    private const val RELEASES =
        "https://api.github.com/repos/${Links.GITHUB_OWNER}/${Links.GITHUB_REPO}/releases?per_page=20"
    private val VERSION = Regex("""(\d+(?:\.\d+)+(?:-[A-Za-z0-9.]+)?)""")

    fun currentVersion(): String = BuildConfig.VERSION_NAME

    /** null = ja esta na ultima versao. Lanca excecao se nao der para verificar. */
    fun check(): UpdateInfo? {
        val list = JSONArray(Http.getText(RELEASES))
        for (i in 0 until list.length()) {
            val r = list.getJSONObject(i)
            if (r.optBoolean("draft") || r.optBoolean("prerelease")) continue
            val assets = r.optJSONArray("assets") ?: continue
            var apkName: String? = null
            var apkUrl: String? = null
            for (j in 0 until assets.length()) {
                val a = assets.getJSONObject(j)
                val name = a.optString("name")
                if (name.endsWith(".apk", true) && name.contains("pc", true)) {
                    apkName = name
                    apkUrl = a.optString("browser_download_url")
                    break
                }
            }
            if (apkName == null) continue
            val tag = r.optString("tag_name")
            val version = (if (tag.contains("pc", true)) VERSION.find(tag) else null)?.value
                ?: VERSION.find(apkName)?.value ?: continue
            // A release mais recente com APK de PC decide.
            if (!isNewer(version, currentVersion())) return null
            val (pt, en, es) = splitChangelog(r.optString("body"))
            return UpdateInfo(version, r.optString("name").ifBlank { tag }, pt, en, es, r.optString("html_url"), apkUrl)
        }
        return null
    }

    fun splitChangelog(body: String): Triple<String, String, String> {
        val pt = StringBuilder()
        val en = StringBuilder()
        val es = StringBuilder()
        var current: StringBuilder? = null
        var sawHeader = false
        for (line in body.lines()) {
            val h = line.trim().lowercase()
            if (h.startsWith("#")) {
                val title = h.trimStart('#').trim()
                when {
                    title == "pt" || title.startsWith("portugu") || title == "pt-br" -> { current = pt; sawHeader = true; continue }
                    title == "en" || title.startsWith("english") || title.startsWith("ingl") -> { current = en; sawHeader = true; continue }
                    title == "es" || title.startsWith("espa") -> { current = es; sawHeader = true; continue }
                }
            }
            current?.appendLine(line)
        }
        if (!sawHeader) return Triple(body.trim(), body.trim(), body.trim())
        return Triple(pt.toString().trim(), en.toString().trim(), es.toString().trim())
    }

    /** Compara "1.2.3" com sufixos opcionais ("1.2.3-beta" < "1.2.3"). */
    fun isNewer(remote: String, local: String): Boolean {
        fun parts(v: String): Pair<List<Int>, String> {
            val main = v.substringBefore('-')
            val suffix = v.substringAfter('-', "")
            return main.split('.').map { it.toIntOrNull() ?: 0 } to suffix
        }
        val (a, sa) = parts(remote)
        val (b, sb) = parts(local)
        for (i in 0 until maxOf(a.size, b.size)) {
            val x = a.getOrElse(i) { 0 }
            val y = b.getOrElse(i) { 0 }
            if (x != y) return x > y
        }
        return sa.isEmpty() && sb.isNotEmpty()
    }
}
