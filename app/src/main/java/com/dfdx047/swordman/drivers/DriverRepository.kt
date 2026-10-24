package com.dfdx047.swordman.drivers

import android.content.Context
import android.net.Uri
import android.os.Build
import com.dfdx047.swordman.data.AppPaths
import com.dfdx047.swordman.data.Http
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.util.zip.ZipInputStream

/**
 * Drivers Vulkan alternativos (Turnip/Mesa) no formato do AdrenoTools: um .zip
 * com meta.json e a .so. O GameActivity carrega a .so que estiver em
 * <appDir>/driver/ (via libadrenotools); sem nada la usa o driver do sistema.
 * Nenhum e ativado por omissao: o usuario escolhe.
 */
data class DriverSource(val owner: String, val repo: String, val label: String, val short: String = owner) {
    val apiUrl get() = "https://api.github.com/repos/$owner/$repo/releases?per_page=20"
    val pageUrl get() = "https://github.com/$owner/$repo/releases"
}

data class DriverAsset(val name: String, val url: String, val size: Long)
data class DriverRelease(val source: DriverSource, val tag: String, val title: String, val date: String, val assets: List<DriverAsset>)

data class InstalledDriver(
    val dir: File,
    val name: String,
    val description: String,
    val version: String,
    val author: String,
    val library: File,
)

class DriverException(val reason: Reason, detail: String = "") : Exception(detail) {
    enum class Reason { NO_LIBRARY, NOT_ZIP, IO, NETWORK }
}

object DriverRepository {
    val SOURCES = listOf(
        DriverSource("MrPurple666", "purple-turnip", "Mr. Purple — Purple Turnip"),
        DriverSource("StevenMXZ", "freedreno_turnip-CI", "StevenMXZ — Turnip CI"),
        DriverSource("K11MCH1", "AdrenoToolsDrivers", "K11MCH1 — AdrenoTools Drivers"),
        // 23/10: o Turnip do ARMSX2 (pacote AdrenoTools, Adreno 6xx/7xx).
        DriverSource("bmdhacks", "armsx2-turnip", "ARMSX2 — Turnip (Adreno 6xx/7xx)", "ARMSX2"),
    )

    /**
     * As fontes que servem para a GPU deste aparelho: Turnip no Adreno; nenhuma no resto. 24/10: o MaliSX2 saiu
     * (imagem toda corrompida no RG Cube, Mali-G57); no Mali o jogo usa sempre o driver do sistema (Vulkan 1.3).
     */
    fun sourcesForDevice(): List<DriverSource> = if (isAdreno()) SOURCES else emptyList()

    /** Adreno (Qualcomm)? Os drivers Turnip so servem para Adreno. */
    fun isAdreno(): Boolean {
        val soc = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) Build.SOC_MANUFACTURER else ""
        return soc.equals("QTI", true) || soc.contains("qualcomm", true) ||
            Build.HARDWARE.contains("qcom", true) || Build.BOARD.contains("msm", true) ||
            Build.BOARD.contains("sm8", true)
    }

    fun socName(): String =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) "${Build.SOC_MANUFACTURER} ${Build.SOC_MODEL}" else Build.HARDWARE

    fun fetchReleases(source: DriverSource): List<DriverRelease> {
        val json = try {
            JSONArray(Http.getText(source.apiUrl))
        } catch (e: Exception) {
            throw DriverException(DriverException.Reason.NETWORK, e.message ?: "")
        }
        val out = ArrayList<DriverRelease>()
        for (i in 0 until json.length()) {
            val r = json.getJSONObject(i)
            val assets = r.optJSONArray("assets") ?: JSONArray()
            val list = (0 until assets.length()).map { assets.getJSONObject(it) }
                .filter { a -> a.optString("name").let { it.endsWith(".zip", true) || it.endsWith(".so") } }
                .map { DriverAsset(it.optString("name"), it.optString("browser_download_url"), it.optLong("size")) }
            if (list.isEmpty()) continue
            out += DriverRelease(
                source,
                r.optString("tag_name"),
                r.optString("name").ifBlank { r.optString("tag_name") },
                r.optString("published_at").take(10),
                list,
            )
        }
        return out
    }

    fun download(asset: DriverAsset, onProgress: (Long, Long) -> Unit): InstalledDriver {
        val tmp = File(AppPaths.cacheDir, "drivers/${asset.name}")
        try {
            Http.download(asset.url, tmp, onProgress)
        } catch (e: Exception) {
            throw DriverException(DriverException.Reason.NETWORK, e.message ?: "")
        }
        try {
            return installFile(tmp, asset.name)
        } finally {
            tmp.delete()
        }
    }

    fun importUri(context: Context, uri: Uri, displayName: String): InstalledDriver {
        val tmp = File(AppPaths.cacheDir, "drivers/import_${System.currentTimeMillis()}_$displayName")
        tmp.parentFile?.mkdirs()
        try {
            context.contentResolver.openInputStream(uri)?.use { input -> tmp.outputStream().use { input.copyTo(it) } }
                ?: throw DriverException(DriverException.Reason.IO, displayName)
            return installFile(tmp, displayName)
        } finally {
            tmp.delete()
        }
    }

    /** Instala um .zip do AdrenoTools ou uma .so solta em drivers/<nome>/. */
    private fun installFile(file: File, displayName: String): InstalledDriver {
        val baseName = displayName.substringBeforeLast('.').replace(Regex("""[\\/:*?"<>|]"""), "_")
        val dest = uniqueDir(baseName)
        dest.mkdirs()
        try {
            if (displayName.endsWith(".so")) {
                file.copyTo(File(dest, displayName), overwrite = true)
                File(dest, "meta.json").writeText(
                    JSONObject().put("name", baseName).put("libraryName", displayName).toString(2),
                )
            } else {
                var entries = 0
                ZipInputStream(file.inputStream().buffered()).use { zip ->
                    while (true) {
                        val e = zip.nextEntry ?: break
                        if (e.isDirectory) continue
                        // So ficheiros soltos (meta.json, .so): achata as pastas e evita zip-slip.
                        val name = e.name.substringAfterLast('/')
                        if (name.isEmpty() || name.startsWith(".")) continue
                        File(dest, name).outputStream().use { zip.copyTo(it) }
                        entries++
                    }
                }
                if (entries == 0) throw DriverException(DriverException.Reason.NOT_ZIP, displayName)
            }
            return readInstalled(dest) ?: throw DriverException(DriverException.Reason.NO_LIBRARY, displayName)
        } catch (e: DriverException) {
            dest.deleteRecursively()
            throw e
        } catch (e: java.util.zip.ZipException) {
            dest.deleteRecursively()
            throw DriverException(DriverException.Reason.NOT_ZIP, displayName)
        } catch (e: Exception) {
            dest.deleteRecursively()
            throw DriverException(DriverException.Reason.IO, e.message ?: "")
        }
    }

    private fun uniqueDir(base: String): File {
        var d = File(AppPaths.driversDir, base)
        var i = 2
        while (d.exists()) d = File(AppPaths.driversDir, "${base}_${i++}")
        return d
    }

    private fun readInstalled(dir: File): InstalledDriver? {
        val meta = File(dir, "meta.json").takeIf { it.isFile }?.let {
            try { JSONObject(it.readText()) } catch (e: Exception) { null }
        }
        val libName = meta?.optString("libraryName").orEmpty()
        val lib = File(dir, libName).takeIf { libName.isNotEmpty() && it.isFile }
            ?: dir.listFiles()?.firstOrNull { it.name.endsWith(".so") }
            ?: return null
        return InstalledDriver(
            dir = dir,
            name = meta?.optString("name").orEmpty().ifBlank { dir.name },
            description = meta?.optString("description").orEmpty(),
            version = meta?.optString("driverVersion").orEmpty(),
            author = meta?.optString("author").orEmpty(),
            library = lib,
        )
    }

    fun installed(): List<InstalledDriver> =
        AppPaths.driversDir.listFiles()?.filter { it.isDirectory }?.mapNotNull(::readInstalled)?.sortedBy { it.name }.orEmpty()

    /** Pasta do driver ativo (ficheiro .source no driver/ guarda a origem). */
    fun activeDir(): String? {
        val so = AppPaths.activeDriverDir.listFiles()?.firstOrNull { it.name.endsWith(".so") } ?: return null
        val src = File(AppPaths.activeDriverDir, ".source")
        return if (src.isFile) src.readText().trim() else so.name
    }

    fun activate(driver: InstalledDriver) {
        val dir = AppPaths.activeDriverDir
        dir.deleteRecursively()
        dir.mkdirs()
        driver.library.copyTo(File(dir, driver.library.name), overwrite = true)
        File(dir, ".source").writeText(driver.dir.name)
    }

    /** Volta ao driver do sistema. */
    fun deactivate() {
        AppPaths.activeDriverDir.deleteRecursively()
    }

    fun delete(driver: InstalledDriver) {
        if (activeDir() == driver.dir.name) deactivate()
        driver.dir.deleteRecursively()
    }
}
