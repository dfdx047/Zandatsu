package com.dfdx047.swordman.update

import android.content.Context
import android.content.Intent
import androidx.core.content.FileProvider
import com.dfdx047.swordman.BuildConfig
import com.dfdx047.swordman.SwordManApp
import com.dfdx047.swordman.data.AppPaths
import com.dfdx047.swordman.data.Http
import org.json.JSONObject
import java.io.File

data class UpdateInfo(
    val version: String,
    val title: String,
    val changelogPt: String,
    val changelogEn: String,
    val changelogEs: String,
    val pageUrl: String,
    val apkUrl: String?,
    val apkSize: Long,
) {
    /** language = codigo ISO do idioma do app ("pt", "es", "en"...); cai para o ingles. */
    fun changelog(language: String) = when (language) {
        "pt" -> changelogPt.ifBlank { changelogEn }
        "es" -> changelogEs.ifBlank { changelogEn }
        else -> changelogEn.ifBlank { changelogPt }
    }
}

/**
 * Verifica a ultima release do repositorio no GitHub. O texto da release pode
 * trazer o changelog nos dois idiomas, em secoes "## PT", "## EN" e "## ES" (ou
 * "## Português" / "## English"); sem secoes, o mesmo texto serve aos dois.
 */
object UpdateChecker {
    private const val LATEST =
        "https://api.github.com/repos/${SwordManApp.GITHUB_OWNER}/${SwordManApp.GITHUB_REPO}/releases/latest"

    fun currentVersion(): String = BuildConfig.VERSION_NAME

    /** null = ja esta na ultima versao. Lanca excecao se nao der para verificar. */
    fun check(): UpdateInfo? {
        val r = JSONObject(Http.getText(LATEST))
        val tag = r.optString("tag_name").removePrefix("v").removePrefix("V")
        if (tag.isBlank() || !isNewer(tag, currentVersion())) return null
        val body = r.optString("body")
        val (pt, en, es) = splitChangelog(body)
        val assets = r.optJSONArray("assets")
        var apkUrl: String? = null
        var apkSize = 0L
        if (assets != null) {
            for (i in 0 until assets.length()) {
                val a = assets.getJSONObject(i)
                if (a.optString("name").endsWith(".apk", true)) {
                    apkUrl = a.optString("browser_download_url")
                    apkSize = a.optLong("size")
                    break
                }
            }
        }
        return UpdateInfo(tag, r.optString("name").ifBlank { tag }, pt, en, es, r.optString("html_url"), apkUrl, apkSize)
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
        // Mesmo numero: a versao sem sufixo (final) e mais nova que a com sufixo.
        return sa.isEmpty() && sb.isNotEmpty()
    }

    fun downloadApk(info: UpdateInfo, onProgress: (Long, Long) -> Unit): File {
        val url = info.apkUrl ?: error("sem APK")
        return Http.download(url, File(AppPaths.updatesDir, "SwordMan-${info.version}.apk"), onProgress)
    }

    /** Abre o instalador do sistema (o usuario confirma; pode pedir "instalar apps desconhecidas"). */
    fun install(context: Context, apk: File) {
        val uri = FileProvider.getUriForFile(context, context.packageName + ".files", apk)
        val intent = Intent(Intent.ACTION_VIEW).apply {
            setDataAndType(uri, "application/vnd.android.package-archive")
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_ACTIVITY_NEW_TASK)
        }
        context.startActivity(intent)
    }
}
