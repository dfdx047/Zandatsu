package com.dfdx047.zandatsu.drivers

import android.content.Context
import android.net.Uri
import android.opengl.EGL14
import android.opengl.EGLConfig
import android.opengl.GLES20
import android.os.Build
import com.dfdx047.zandatsu.data.AppPaths
import com.dfdx047.zandatsu.data.Http
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.util.zip.ZipInputStream

/**
 * Drivers Vulkan alternativos (Mesa) no formato do AdrenoTools: um .zip com meta.json e a .so (libraryName).
 * Turnip nas Adreno, RADV nas Samsung Xclipse (26/10). O GameActivity carrega a .so principal que estiver em
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
    val kind: DriverKind,
    /** meta.json "minApi" (0 = sem minimo). */
    val minApi: Int = 0,
)

/** Que driver e (pela libraryName): o Turnip so serve nas Adreno, o RADV so nas Xclipse. */
enum class DriverKind { TURNIP, RADV, OTHER }

class DriverException(val reason: Reason, detail: String = "") : Exception(detail) {
    enum class Reason { NO_LIBRARY, NOT_ZIP, IO, NETWORK, MIN_API, WRONG_GPU }
}

object DriverRepository {
    val TURNIP_SOURCES = listOf(
        DriverSource("MrPurple666", "purple-turnip", "Mr. Purple — Purple Turnip"),
        DriverSource("StevenMXZ", "freedreno_turnip-CI", "StevenMXZ — Turnip CI"),
        DriverSource("K11MCH1", "AdrenoToolsDrivers", "K11MCH1 — AdrenoTools Drivers"),
        // 23/10: o Turnip do ARMSX2 (pacote AdrenoTools, Adreno 6xx/7xx).
        DriverSource("bmdhacks", "armsx2-turnip", "ARMSX2 — Turnip (Adreno 6xx/7xx)", "ARMSX2"),
    )

    /** 26/10: RADV (Mesa) para as Samsung Xclipse. Zip do AdrenoTools com vulkan.radeon.so, minApi 34. */
    val RADV_SOURCES = listOf(
        DriverSource("JimVulkan", "radv-xclipse", "JimVulkan — RADV Xclipse", "JimVulkan"),
        DriverSource("Matheus-TestUser1", "radv-xclipse", "Matheus-TestUser1 — RADV Xclipse", "Matheus"),
    )

    /** O RADV das Xclipse precisa do Android 14 (minApi 34 no meta.json dos pacotes). */
    const val RADV_MIN_API = 34

    /**
     * As fontes que servem para a GPU deste aparelho: Turnip no Adreno, RADV na Xclipse (Android 14+); nenhuma no
     * resto. 24/10: o MaliSX2 saiu (imagem toda corrompida no RG Cube, Mali-G57); no Mali o jogo usa sempre o driver
     * do sistema (Vulkan 1.3).
     */
    fun sourcesForDevice(): List<DriverSource> = when {
        isAdreno() -> TURNIP_SOURCES
        isXclipse() && Build.VERSION.SDK_INT >= RADV_MIN_API -> RADV_SOURCES
        else -> emptyList()
    }

    /** Ha escolha de driver nesta GPU (Adreno: Turnip; Xclipse: RADV)? No resto so o driver do sistema. */
    fun driversOffered(): Boolean = isAdreno() || isXclipse()

    /** Este driver pode ser ativado nesta GPU? Turnip (e outros) so no Adreno, RADV so na Xclipse. */
    fun fitsDevice(kind: DriverKind): Boolean = when {
        isAdreno() -> kind != DriverKind.RADV
        isXclipse() -> kind == DriverKind.RADV
        else -> false
    }

    /** Pela biblioteca principal: vulkan.radeon.so = RADV; libvulkan_freedreno.so (ou nome Adreno) = Turnip. */
    fun kindOf(libraryName: String): DriverKind {
        val n = libraryName.lowercase(java.util.Locale.ROOT)
        return when {
            "radeon" in n || "radv" in n -> DriverKind.RADV
            "freedreno" in n || "turnip" in n || "adreno" in n -> DriverKind.TURNIP
            else -> DriverKind.OTHER
        }
    }

    /** Adreno (Qualcomm)? Os drivers Turnip so servem para Adreno. */
    fun isAdreno(): Boolean {
        val soc = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) Build.SOC_MANUFACTURER else ""
        return soc.equals("QTI", true) || soc.contains("qualcomm", true) ||
            Build.HARDWARE.contains("qcom", true) || Build.BOARD.contains("msm", true) ||
            Build.BOARD.contains("sm8", true)
    }

    /**
     * Samsung Xclipse (RDNA da AMD, Exynos 2200 em diante)? Pelo GL_RENDERER ("... Xclipse 940 ..."); sem ele pelo
     * SOC_MODEL das Exynos com Xclipse.
     */
    fun isXclipse(probe: Boolean = true): Boolean {
        if (isAdreno()) return false
        if (gpuName(probe).contains("Xclipse", true)) return true
        val soc = (if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) Build.SOC_MODEL ?: "" else "").uppercase(java.util.Locale.ROOT)
        return XCLIPSE_SOCS.any { soc.startsWith(it) }
    }

    /** Exynos 2200/2400/2500/2600 e 1480/1580 (Xclipse 920/940/950/960, 530/540). */
    private val XCLIPSE_SOCS = listOf("S5E9925", "S5E9945", "S5E9955", "S5E9965", "S5E8845", "S5E8855")

    @Volatile private var gpuNameCache: String? = null

    /**
     * O nome da GPU pelo GL_RENDERER (contexto GLES num pbuffer 1x1, criado e destruido aqui; "" se falhar). O Vulkan
     * so diz o nome depois de carregar o driver; o GL basta para saber o fabricante. O launcher pergunta uma vez e
     * guarda-o em gpu_name.txt; o processo do jogo (probe = false) so le esse ficheiro, sem abrir GL ao lado do Vulkan.
     */
    fun gpuName(probe: Boolean = true): String {
        gpuNameCache?.let { return it }
        val file = try { File(AppPaths.internalDir, "gpu_name.txt") } catch (e: Exception) { null }
        val saved = try { file?.takeIf { it.isFile }?.readText()?.trim() } catch (e: Exception) { null }
        if (saved != null) return saved.also { gpuNameCache = it }
        if (!probe) return ""
        return queryGlRenderer().also {
            gpuNameCache = it
            if (it.isNotEmpty()) try { file?.writeText(it) } catch (e: Exception) {}
        }
    }

    private fun queryGlRenderer(): String = try {
        val dpy = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY)
        val ver = IntArray(2)
        if (dpy == EGL14.EGL_NO_DISPLAY || !EGL14.eglInitialize(dpy, ver, 0, ver, 1)) {
            ""
        } else {
            val attribs = intArrayOf(
                EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
                EGL14.EGL_SURFACE_TYPE, EGL14.EGL_PBUFFER_BIT,
                EGL14.EGL_NONE,
            )
            val configs = arrayOfNulls<EGLConfig>(1)
            val n = IntArray(1)
            if (!EGL14.eglChooseConfig(dpy, attribs, 0, configs, 0, 1, n, 0) || n[0] == 0) {
                ""
            } else {
                val ctx = EGL14.eglCreateContext(
                    dpy, configs[0], EGL14.EGL_NO_CONTEXT, intArrayOf(EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE), 0,
                )
                val surf = EGL14.eglCreatePbufferSurface(dpy, configs[0], intArrayOf(EGL14.EGL_WIDTH, 1, EGL14.EGL_HEIGHT, 1, EGL14.EGL_NONE), 0)
                val name = if (EGL14.eglMakeCurrent(dpy, surf, surf, ctx)) GLES20.glGetString(GLES20.GL_RENDERER).orEmpty() else ""
                EGL14.eglMakeCurrent(dpy, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT)
                EGL14.eglDestroySurface(dpy, surf)
                EGL14.eglDestroyContext(dpy, ctx)
                // Sem eglTerminate: o display e o mesmo que o resto do processo (HWUI) usa.
                name.trim()
            }
        }
    } catch (e: Throwable) {
        ""
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
            val d = readInstalled(dest) ?: throw DriverException(DriverException.Reason.NO_LIBRARY, displayName)
            // meta.json "minApi" (o RADV das Xclipse pede o Android 14) e a GPU certa para o pacote.
            if (Build.VERSION.SDK_INT < d.minApi) throw DriverException(DriverException.Reason.MIN_API, d.minApi.toString())
            if (!fitsDevice(d.kind)) throw DriverException(DriverException.Reason.WRONG_GPU, displayName)
            return d
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

    private fun readMeta(dir: File): JSONObject? = File(dir, "meta.json").takeIf { it.isFile }?.let {
        try { JSONObject(it.readText()) } catch (e: Exception) { null }
    }

    /**
     * A biblioteca principal de uma pasta de driver: a "libraryName" do meta.json (o zip e achatado, so conta o nome);
     * sem ela a libvulkan/vulkan.*.so, e so depois qualquer .so. Antes ia a primeira .so e um zip do Turnip com a
     * notadreno_utils.so ao lado carregava a errada (issue #6).
     */
    fun libraryIn(dir: File): File? {
        val libName = readMeta(dir)?.optString("libraryName").orEmpty().substringAfterLast('/')
        File(dir, libName).takeIf { libName.isNotEmpty() && it.isFile }?.let { return it }
        val sos = dir.listFiles()?.filter { it.isFile && it.name.endsWith(".so") }?.sortedBy { it.name }.orEmpty()
        return sos.firstOrNull { it.name.startsWith("vulkan.") || it.name.startsWith("libvulkan") } ?: sos.firstOrNull()
    }

    private fun readInstalled(dir: File): InstalledDriver? {
        val meta = readMeta(dir)
        val lib = libraryIn(dir) ?: return null
        return InstalledDriver(
            dir = dir,
            name = meta?.optString("name").orEmpty().ifBlank { dir.name },
            description = meta?.optString("description").orEmpty(),
            version = meta?.optString("driverVersion").orEmpty(),
            author = meta?.optString("author").orEmpty(),
            library = lib,
            kind = kindOf(lib.name),
            minApi = meta?.optInt("minApi", 0) ?: 0,
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

    /** O tipo do driver ativo (null = driver do sistema). */
    fun activeKind(): DriverKind? = libraryIn(AppPaths.activeDriverDir)?.let { kindOf(it.name) }

    /**
     * Copia a pasta inteira do driver (a .so principal, as outras .so de que ela precisa e o meta.json) para driver/;
     * o GameActivity carrega a libraryName do meta.json.
     */
    fun activate(driver: InstalledDriver) {
        val dir = AppPaths.activeDriverDir
        dir.deleteRecursively()
        dir.mkdirs()
        driver.dir.listFiles()?.filter { it.isFile && !it.name.startsWith(".") }?.forEach {
            it.copyTo(File(dir, it.name), overwrite = true)
        }
        if (!File(dir, "meta.json").isFile) {
            File(dir, "meta.json").writeText(JSONObject().put("name", driver.name).put("libraryName", driver.library.name).toString(2))
        }
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
