package com.dfdx047.zandatsu.ui

import android.app.Application
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.OpenableColumns
import androidx.annotation.StringRes
import androidx.appcompat.app.AppCompatDelegate
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.core.content.pm.ShortcutInfoCompat
import androidx.core.content.pm.ShortcutManagerCompat
import androidx.core.graphics.drawable.IconCompat
import androidx.core.os.LocaleListCompat
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.dfdx047.zandatsu.R
import com.dfdx047.zandatsu.data.AppLanguage
import com.dfdx047.zandatsu.data.AppLog
import com.dfdx047.zandatsu.data.GraphicsPreset
import com.dfdx047.zandatsu.data.RenutPreset
import com.dfdx047.zandatsu.data.RenutSupport
import com.dfdx047.zandatsu.data.withPreset
import com.dfdx047.zandatsu.data.withRenutPreset
import com.dfdx047.zandatsu.data.AppPaths
import com.dfdx047.zandatsu.data.CrashReport
import com.dfdx047.zandatsu.data.CrashReports
import com.dfdx047.zandatsu.data.ControlStyle
import com.dfdx047.zandatsu.data.DarkMode
import com.dfdx047.zandatsu.data.ErrorKind
import com.dfdx047.zandatsu.data.FileStatus
import com.dfdx047.zandatsu.data.GameConfig
import com.dfdx047.zandatsu.data.GameFiles
import com.dfdx047.zandatsu.data.IsoSource
import com.dfdx047.zandatsu.data.GameInstall
import com.dfdx047.zandatsu.data.GameOrientation
import com.dfdx047.zandatsu.data.GameLib
import com.dfdx047.zandatsu.data.ImportException
import com.dfdx047.zandatsu.data.ImportProgress
import com.dfdx047.zandatsu.data.LauncherPrefs
import com.dfdx047.zandatsu.data.ThemeMode
import com.dfdx047.zandatsu.data.frameMode
import com.dfdx047.zandatsu.drivers.DriverAsset
import com.dfdx047.zandatsu.drivers.DriverException
import com.dfdx047.zandatsu.drivers.DriverRelease
import com.dfdx047.zandatsu.drivers.DriverRepository
import com.dfdx047.zandatsu.drivers.DriverSource
import com.dfdx047.zandatsu.drivers.InstalledDriver
import com.dfdx047.zandatsu.game.GameActivity
import com.dfdx047.zandatsu.update.UpdateChecker
import com.dfdx047.zandatsu.update.UpdateInfo
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** Mensagem curta (snackbar), resolvida no ecra com o idioma da atividade. */
data class UiMessage(@StringRes val res: Int, val args: List<Any> = emptyList(), val id: Long = System.nanoTime())

/** Operacao longa em curso (importar, verificar, descarregar...). */
data class Busy(@StringRes val label: Int, val detail: String = "", val progress: Float? = null)

enum class UpdateState { IDLE, CHECKING, UP_TO_DATE, AVAILABLE, FAILED }

class LauncherViewModel(app: Application) : AndroidViewModel(app) {
    private val prefs = LauncherPrefs(app)

    // ------------------------------------------------------------- aparencia
    var themeMode by mutableStateOf(prefs.themeMode)
        private set
    var darkMode by mutableStateOf(prefs.darkMode)
        private set
    var appLanguage by mutableStateOf(currentAppLanguage())
        private set

    fun setTheme(v: ThemeMode) { themeMode = v; prefs.themeMode = v }

    // ------------------------------------------------------------- cheats
    var skipLogos by mutableStateOf(prefs.skipLogos)
        private set
    fun changeSkipLogos(v: Boolean) { skipLogos = v; prefs.skipLogos = v }
    fun setDark(v: DarkMode) { darkMode = v; prefs.darkMode = v }

    fun setLanguage(v: AppLanguage) {
        appLanguage = v
        AppCompatDelegate.setApplicationLocales(
            if (v == AppLanguage.SYSTEM) LocaleListCompat.getEmptyLocaleList() else LocaleListCompat.forLanguageTags(v.tag),
        )
    }

    private fun currentAppLanguage(): AppLanguage {
        val tag = AppCompatDelegate.getApplicationLocales().toLanguageTags()
        return when {
            tag.startsWith("pt") -> AppLanguage.PORTUGUESE
            tag.startsWith("en") -> AppLanguage.ENGLISH
            tag.startsWith("es") -> AppLanguage.SPANISH
            else -> AppLanguage.SYSTEM
        }
    }

    // ------------------------------------------------------- jogo e launcher
    var gameOrientation by mutableStateOf(prefs.gameOrientation)
        private set
    fun setOrientation(v: GameOrientation) { gameOrientation = v; prefs.gameOrientation = v }

    var turnipCompatFlags by mutableStateOf(prefs.turnipCompatFlags)
        private set
    fun setTurnipCompatFlag(bit: Int, on: Boolean) {
        turnipCompatFlags = if (on) turnipCompatFlags or bit else turnipCompatFlags and bit.inv()
        prefs.turnipCompatFlags = turnipCompatFlags
    }

    var turnipBisect by mutableStateOf(prefs.turnipBisect)
        private set
    var turnipBisectStep by mutableStateOf(prefs.turnipBisectStep)
        private set
    fun changeTurnipBisect(on: Boolean) {
        turnipBisect = on
        prefs.turnipBisect = on
        if (on) {
            turnipBisectStep = 0
            prefs.turnipBisectStep = 0
        }
    }
    /** Varredura automatica do Turnip (TurnipSweep): liga e abre o jogo no passo 0. */
    fun startTurnipSweep(context: Context, set: String = "driver") {
        com.dfdx047.zandatsu.game.TurnipSweep.start(context, AppPaths.appDir, set)
        turnipBisect = true
        turnipBisectStep = 0
        play(context)
    }

    /**
     * Ao voltar ao launcher com a varredura ligada: um crash conta como resultado do passo; se
     * ainda ha passos abre o jogo de novo, senao desliga e exporta os logs. true = tratou.
     */
    fun continueTurnipSweep(activity: android.app.Activity): Boolean {
        val sweep = com.dfdx047.zandatsu.game.TurnipSweep
        if (!sweep.isActive(activity)) return false
        // 23/10: a varredura saiu do launcher; uma que ficou ligada de antes e so parada.
        sweep.stop(activity)
        turnipBisect = false
        return false
        val r = try {
            CrashReports.lastGameExit(activity, prefs)
        } catch (_: Exception) {
            null
        }
        if (r != null) {
            val label = sweep.label(activity)
            sweep.append(AppPaths.appDir, "passo $label  CRASH (${r.detail})")
            prefs.clearDriverBootState()
        }
        refreshTurnipBisect()
        if (sweep.nextStep(activity) == 0) {
            sweep.stop(activity)
            turnipBisect = false
            sweep.append(AppPaths.appDir, "fim")
            exportLogs(activity)
            return true
        }
        android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({ play(activity) }, 1500)
        return true
    }

    /** O GameActivity avanca o passo; ao voltar ao launcher le-se de novo. */
    fun refreshTurnipBisect() {
        turnipBisectStep = prefs.turnipBisectStep
    }

    var controlStyle by mutableStateOf(prefs.controlStyle)
        private set
    // JvmName: evita o conflito com o setter privado da propriedade (mesma assinatura JVM).
    @JvmName("changeControlStyle")
    fun setControlStyle(v: ControlStyle) { controlStyle = v; prefs.controlStyle = v }

    var directional by mutableStateOf(prefs.directional)
        private set
    @JvmName("changeDirectional")
    fun setDirectional(v: com.dfdx047.zandatsu.data.Directional) { directional = v; prefs.directional = v }

    var buttonBadge by mutableStateOf(prefs.buttonBadge)
        private set
    @JvmName("changeButtonBadge")
    fun setButtonBadge(v: com.dfdx047.zandatsu.data.ButtonBadge) { buttonBadge = v; prefs.buttonBadge = v }

    var pauseOnMenus by mutableStateOf(prefs.pauseOnMenus)
        private set
    @JvmName("changePauseOnMenus")
    fun setPauseOnMenus(v: Boolean) { pauseOnMenus = v; prefs.pauseOnMenus = v }

    var touchMode by mutableStateOf(prefs.touchMode)
        private set
    @JvmName("changeTouchMode")
    fun setTouchMode(v: com.dfdx047.zandatsu.data.TouchMode) { touchMode = v; prefs.touchMode = v }

    var touchEnabled by mutableStateOf(prefs.touchEnabled)
        private set
    // JvmName: evita o conflito com o setter privado da propriedade (mesma assinatura JVM).
    @JvmName("changeTouchEnabled")
    fun setTouchEnabled(v: Boolean) { touchEnabled = v; prefs.touchEnabled = v }

    var touchHideWithPad by mutableStateOf(prefs.touchHideWithPad)
        private set
    // JvmName: evita o conflito com o setter privado da propriedade (mesma assinatura JVM).
    @JvmName("changeTouchHideWithPad")
    fun setTouchHideWithPad(v: Boolean) { touchHideWithPad = v; prefs.touchHideWithPad = v }

    var touchHaptics by mutableStateOf(prefs.touchHaptics)
        private set
    // JvmName: evita o conflito com o setter privado da propriedade (mesma assinatura JVM).
    @JvmName("changeTouchHaptics")
    fun setTouchHaptics(v: Boolean) { touchHaptics = v; prefs.touchHaptics = v }

    var touchOpacity by mutableStateOf(prefs.touchOpacity)
        private set
    // JvmName: evita o conflito com o setter privado da propriedade (mesma assinatura JVM).
    @JvmName("changeTouchOpacity")
    fun setTouchOpacity(v: Float) { touchOpacity = v; prefs.touchOpacity = v }

    var checkUpdatesOnStart by mutableStateOf(prefs.checkUpdatesOnStart)
        private set
    // JvmName: evita o conflito com o setter privado da propriedade (mesma assinatura JVM).
    @JvmName("changeCheckUpdatesOnStart")
    fun setCheckUpdatesOnStart(v: Boolean) { checkUpdatesOnStart = v; prefs.checkUpdatesOnStart = v }

    var config by mutableStateOf(GameConfig())
        private set

    /** Primeiro arranque: falta o jogador escolher um perfil (nada vem escolhido). */
    var needsProfile by mutableStateOf(!prefs.profileChosen)
        private set

    /**
     * O perfil do ecra do primeiro arranque (0 = Minimo .. 4 = Ultra): o do Renut se o jogo corre nele, senao o
     * do Xenia. As duas listas tem a mesma ordem.
     */
    fun chooseProfile(tier: Int) {
        updateConfig { c ->
            if (c.useRenut) c.withRenutPreset(RenutPreset.entries[tier]) else c.withPreset(GraphicsPreset.entries[tier])
        }
        prefs.profileChosen = true
        needsProfile = false
        AppLog.i("app", "perfil escolhido no primeiro arranque: $tier (${if (config.useRenut) "Renut" else "Xenia"})")
    }

    fun updateConfig(transform: (GameConfig) -> GameConfig) {
        val next = transform(config)
        if (next == config) return
        config = next
        viewModelScope.launch(Dispatchers.IO) { GameConfig.save(next) }
    }

    var busy by mutableStateOf<Busy?>(null)
        private set
    var message by mutableStateOf<UiMessage?>(null)
    private fun say(@StringRes res: Int, vararg args: Any) {
        message = UiMessage(res, args.toList())
    }

    var install by mutableStateOf<GameInstall?>(null)
        private set

    // ------------------------------------------------------- avisos de erro
    /** Aviso de erro aberto (crash, jogo nao abriu, driver, ISO corrompida...). */
    var errorReport by mutableStateOf<CrashReport?>(null)
        private set

    fun dismissError() {
        errorReport = null
    }

    private fun showError(kind: ErrorKind, detail: String) {
        errorReport = CrashReports.of(kind, detail)
    }

    /** Ao abrir/voltar ao launcher: o jogo fechou sozinho na ultima sessao? */
    fun checkLastRun() {
        val ctx = getApplication<Application>()
        viewModelScope.launch(Dispatchers.IO) {
            val r = try {
                CrashReports.lastGameExit(ctx, prefs)
            } catch (e: Exception) {
                AppLog.w("crash", "verificacao da ultima sessao falhou", e)
                null
            }
            driverBootFailed = prefs.driverBootFailed
            if (r != null) withContext(Dispatchers.Main) { errorReport = r }
        }
    }

    /** "Guardar log" do aviso de erro: o zip dos logs no destino escolhido. */
    fun saveLogTo(context: Context, uri: Uri) {
        viewModelScope.launch(Dispatchers.IO) {
            busy = Busy(R.string.busy_exporting_logs)
            try {
                com.dfdx047.zandatsu.data.LogExporter.exportTo(context.applicationContext, uri)
                say(R.string.msg_log_saved)
            } catch (e: Exception) {
                AppLog.e("logs", "guardar log falhou", e)
                say(R.string.err_logs_export, e.message ?: "-")
            } finally {
                busy = null
            }
        }
    }

    /** Ao voltar ao launcher: o menu rapido do jogo pode ter mudado o toml. */
    fun reloadAfterGame() {
        viewModelScope.launch(Dispatchers.IO) {
            val c = GameConfig.load()
            withContext(Dispatchers.Main) { config = c }
        }
        checkLastRun()
    }

    /** Com a ISO ativa: bytes de ficheiros extraidos de importacoes antigas que ja se podem apagar. */
    var staleBytes by mutableStateOf(0L)
        private set

    fun deleteStaleFiles() {
        viewModelScope.launch(Dispatchers.IO) {
            busy = Busy(R.string.busy_deleting_old)
            try {
                IsoSource.deleteStale()
            } finally {
                busy = null
            }
            staleBytes = IsoSource.staleBytes()
            install = GameFiles.check()
        }
    }

    fun refreshInstall(fullHash: Boolean) {
        viewModelScope.launch(Dispatchers.IO) {
            if (fullHash) busy = Busy(R.string.busy_verifying)
            val result = try {
                GameFiles.check { p ->
                    if (p is ImportProgress.Hashing && fullHash) {
                        busy = Busy(R.string.busy_verifying, p.file, p.done.toFloat() / p.total.coerceAtLeast(1))
                    }
                }
            } finally {
                if (fullHash) busy = null
            }
            install = result
            staleBytes = IsoSource.staleBytes()
            if (fullHash) {
                if (result.playable) say(R.string.msg_verify_ok)
                else say(R.string.msg_verify_problems)
            }
        }
    }

    private fun importProgress(p: ImportProgress): Busy = when (p) {
        is ImportProgress.Copying -> {
            // "ficheiro · 85 MB/s · falta 1 min 20 s"
            val rate = p.bytesPerSecond
            val extra = if (rate > 0) {
                val left = ((p.total - p.done).coerceAtLeast(0) / rate)
                val eta = if (left >= 60) "${left / 60} min ${left % 60} s" else "$left s"
                " · ${rate / (1 shl 20)} MB/s · " + getApplication<Application>().getString(R.string.busy_eta, eta)
            } else {
                ""
            }
            Busy(R.string.busy_importing, p.file.substringAfterLast('/') + extra, p.done.toFloat() / p.total.coerceAtLeast(1))
        }
        is ImportProgress.Hashing -> Busy(R.string.busy_checking_xex, p.file, p.done.toFloat() / p.total.coerceAtLeast(1))
        is ImportProgress.Reading -> Busy(R.string.busy_reading_disc, p.file)
        is ImportProgress.Unpacking -> Busy(R.string.busy_unpacking, "${p.done / (1 shl 20)} / ${p.total / (1 shl 20)} MB", p.done.toFloat() / p.total.coerceAtLeast(1))
    }

    /** Ultimo problema da importacao (fica no cartao dos arquivos ate a proxima). */
    var importIssue by mutableStateOf<Pair<Int, String>?>(null)

    private fun importError(e: ImportException) {
        val res = when (e.reason) {
            ImportException.Reason.NO_XEX -> R.string.err_import_no_xex
            ImportException.Reason.NO_DATA_FOLDER -> R.string.err_import_no_data
            ImportException.Reason.WRONG_REGION -> R.string.err_import_region
            ImportException.Reason.BAD_XEX -> R.string.err_import_bad_xex
            ImportException.Reason.NOT_A_DISC -> R.string.err_import_not_disc
            ImportException.Reason.MISSING_REQUIRED -> R.string.err_import_missing
            ImportException.Reason.NO_SPACE -> R.string.err_import_space
            ImportException.Reason.IO -> R.string.err_import_io
            ImportException.Reason.OTHER_GAME -> R.string.err_import_other_game
            ImportException.Reason.MODIFIED_XEX -> R.string.err_import_modified
            ImportException.Reason.TRUNCATED -> R.string.err_import_truncated
            ImportException.Reason.BAD_DATA -> R.string.err_import_bad_data
            ImportException.Reason.UNSUPPORTED_VERSION -> R.string.err_import_unsupported
        }
        importIssue = res to e.detail.ifEmpty { "-" }
        AppLog.e("import", "${e.reason}: ${e.detail}")
        val corrupt = e.reason in setOf(
            ImportException.Reason.BAD_XEX,
            ImportException.Reason.NOT_A_DISC,
            ImportException.Reason.TRUNCATED,
            ImportException.Reason.BAD_DATA,
            ImportException.Reason.MODIFIED_XEX,
        )
        if (corrupt || e.reason == ImportException.Reason.IO) {
            showError(if (corrupt) ErrorKind.BAD_ISO else ErrorKind.IMPORT, "${e.reason}: ${e.detail.ifEmpty { "-" }}")
        }
        say(
            when (e.reason) {
                ImportException.Reason.NO_XEX -> R.string.err_import_no_xex
                ImportException.Reason.NO_DATA_FOLDER -> R.string.err_import_no_data
                ImportException.Reason.WRONG_REGION -> R.string.err_import_region
                ImportException.Reason.BAD_XEX -> R.string.err_import_bad_xex
                ImportException.Reason.NOT_A_DISC -> R.string.err_import_not_disc
                ImportException.Reason.MISSING_REQUIRED -> R.string.err_import_missing
                ImportException.Reason.NO_SPACE -> R.string.err_import_space
                ImportException.Reason.IO -> R.string.err_import_io
                else -> res
            },
            e.detail.ifEmpty { "-" },
        )
    }

    /** Importa de uma pasta ja extraida (com default.xex e media/). */
    fun importGame(uri: Uri) {
        val ctx = getApplication<Application>()
        viewModelScope.launch(Dispatchers.IO) {
            busy = Busy(R.string.busy_importing)
            try {
                importIssue = null
                AppLog.i("import", "pasta: $uri")
                GameFiles.importFolder(ctx, uri) { p -> busy = importProgress(p) }
                AppLog.i("import", "pasta importada")
                say(R.string.msg_import_ok)
            } catch (e: ImportException) {
                importError(e)
            } finally {
                busy = null
            }
            install = GameFiles.check()
            staleBytes = IsoSource.staleBytes()
        }
    }

    /** Importa direto da imagem do disco (.iso do Xbox 360), como o instalador do PC. */
    fun importIso(uri: Uri) {
        val ctx = getApplication<Application>()
        viewModelScope.launch(Dispatchers.IO) {
            busy = Busy(R.string.busy_reading_disc)
            try {
                importIssue = null
                AppLog.i("import", "ISO: $uri")
                GameFiles.importIso(ctx, uri) { p -> busy = importProgress(p) }
                AppLog.i("import", "ISO importada")
                say(R.string.msg_import_ok)
            } catch (e: ImportException) {
                importError(e)
            } finally {
                busy = null
            }
            install = GameFiles.check()
            staleBytes = IsoSource.staleBytes()
        }
    }

    /** Grava as opcoes e abre o jogo. */
    fun play(context: Context) {
        val inst = install
        if (inst == null || !inst.playable) {
            say(if (inst?.xex?.status == FileStatus.DIFFERENT) R.string.err_import_region else R.string.msg_not_installed, "-")
            return
        }
        viewModelScope.launch {
            // O libgame.so deste APK (depois de uma atualizacao, ou de um import antigo).
            val ok = withContext(Dispatchers.IO) {
                if (GameLib.ready(context)) return@withContext true
                try {
                    GameLib.install(context) { p -> busy = importProgress(p) }
                    true
                } catch (e: ImportException) {
                    importError(e)
                    false
                } finally {
                    busy = null
                }
            }
            if (!ok) return@launch
            withContext(Dispatchers.IO) {
                GameConfig.save(config)
                GameConfig.saveSkipLogos(prefs.skipLogos)
            }
            RenutSupport.noteLaunch()
            AppLog.i("launcher", "jogar: ${if (config.useRenut) "renut" else "xenos"} ${config.resolution.label} ${config.frameMode} ${config.presentEffect}")
            context.startActivity(Intent(context, GameActivity::class.java).setAction(PLAY_ACTION))
        }
    }

    fun createShortcut(context: Context) {
        if (!ShortcutManagerCompat.isRequestPinShortcutSupported(context)) {
            say(R.string.msg_shortcut_unsupported)
            return
        }
        val info = ShortcutInfoCompat.Builder(context, "play_mgr")
            .setShortLabel(context.getString(R.string.shortcut_short))
            .setLongLabel(context.getString(R.string.shortcut_long))
            .setIcon(IconCompat.createWithResource(context, R.mipmap.ic_launcher))
            .setIntent(Intent(context, GameActivity::class.java).setAction(PLAY_ACTION))
            .build()
        if (ShortcutManagerCompat.requestPinShortcut(context, info, null)) say(R.string.msg_shortcut_requested)
        else say(R.string.msg_shortcut_unsupported)
    }

    private fun displayName(uri: Uri): String {
        val ctx = getApplication<Application>()
        return ctx.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
            if (c.moveToFirst()) c.getString(0) else null
        } ?: uri.lastPathSegment ?: "driver.zip"
    }

    // --------------------------------------------------------------- drivers
    val isAdreno = DriverRepository.isAdreno()
    val socName = DriverRepository.socName()
    /** O que a tabela do Turnip diz deste Snapdragon (GPU, geracao, estado, builds). */
    val turnipGpu = com.dfdx047.zandatsu.drivers.TurnipSupport.detect()
    var driverBootFailed by mutableStateOf(prefs.driverBootFailed)
        private set
    var installedDrivers by mutableStateOf<List<InstalledDriver>>(emptyList())
        private set
    var activeDriver by mutableStateOf<String?>(null)
        private set
    var driverSource by mutableStateOf(DriverRepository.SOURCES.first())
        private set
    var driverReleases by mutableStateOf<List<DriverRelease>>(emptyList())
        private set
    var driverReleasesLoading by mutableStateOf(false)
        private set
    var driverReleasesError by mutableStateOf(false)
        private set
    /**
     * Aviso de primeiro arranque (GPUs e drivers). Mostrado uma vez a todos; tambem se
     * abre pelo Sobre. Substitui o antigo aviso do Turnip (o texto ja fala do Turnip).
     */
    var showGpuNotice by mutableStateOf(!prefs.gpuNoticeShown)
        private set

    fun openGpuNotice() {
        showGpuNotice = true
    }

    fun dismissGpuNotice() {
        showGpuNotice = false
        prefs.gpuNoticeShown = true
        prefs.turnipNoticeShown = true
    }

    fun refreshDrivers() {
        viewModelScope.launch(Dispatchers.IO) {
            // Fora do Adreno so ha o driver do sistema: um driver ativo de antes (o MaliSX2, 23/10) sai.
            val old = DriverRepository.activeDir()
            if (!isAdreno && old != null) {
                AppLog.i("drivers", "nao e Adreno: $old desativado, driver do sistema")
                DriverRepository.deactivate()
            }
            installedDrivers = if (isAdreno) DriverRepository.installed() else emptyList()
            activeDriver = DriverRepository.activeDir()
        }
    }

    fun loadDriverReleases(source: DriverSource = driverSource) {
        driverSource = source
        viewModelScope.launch(Dispatchers.IO) {
            driverReleasesLoading = true
            driverReleasesError = false
            driverReleases = try {
                DriverRepository.fetchReleases(source)
            } catch (e: DriverException) {
                AppLog.w("drivers", "lista de ${source.label} falhou", e)
                driverReleasesError = true
                emptyList()
            } finally {
                driverReleasesLoading = false
            }
        }
    }

    fun downloadDriver(asset: DriverAsset) {
        viewModelScope.launch(Dispatchers.IO) {
            busy = Busy(R.string.busy_downloading_driver, asset.name)
            try {
                val d = DriverRepository.download(asset) { done, total ->
                    busy = Busy(R.string.busy_downloading_driver, asset.name, if (total > 0) done.toFloat() / total else null)
                }
                AppLog.i("drivers", "descarregado: ${d.name}")
                say(R.string.msg_driver_installed, d.name)
            } catch (e: DriverException) {
                say(driverError(e), e.message ?: "-")
                showError(ErrorKind.DRIVER_INSTALL, "${asset.name}: ${e.reason} ${e.message ?: ""}")
            } finally {
                busy = null
            }
            refreshDrivers()
        }
    }

    fun importDriver(uri: Uri) {
        val ctx = getApplication<Application>()
        viewModelScope.launch(Dispatchers.IO) {
            val name = displayName(uri)
            busy = Busy(R.string.busy_installing_driver, name)
            try {
                val d = DriverRepository.importUri(ctx, uri, name)
                AppLog.i("drivers", "importado: ${d.name}")
                say(R.string.msg_driver_installed, d.name)
            } catch (e: DriverException) {
                say(driverError(e), e.message ?: "-")
                showError(ErrorKind.DRIVER_INSTALL, "$name: ${e.reason} ${e.message ?: ""}")
            } finally {
                busy = null
            }
            refreshDrivers()
        }
    }

    private fun driverError(e: DriverException) = when (e.reason) {
        DriverException.Reason.NO_LIBRARY -> R.string.err_driver_no_lib
        DriverException.Reason.NOT_ZIP -> R.string.err_driver_not_zip
        DriverException.Reason.NETWORK -> R.string.err_network
        DriverException.Reason.IO -> R.string.err_driver_io
    }

    fun activateDriver(d: InstalledDriver) {
        viewModelScope.launch(Dispatchers.IO) {
            DriverRepository.activate(d)
            AppLog.i("drivers", "ativo: ${d.name}")
            prefs.clearDriverBootState()
            driverBootFailed = ""
            say(R.string.msg_driver_active, d.name)
            refreshDrivers()
        }
    }

    fun useSystemDriver() {
        viewModelScope.launch(Dispatchers.IO) {
            DriverRepository.deactivate()
            AppLog.i("drivers", "driver do sistema")
            prefs.clearDriverBootState()
            driverBootFailed = ""
            say(R.string.msg_driver_system)
            refreshDrivers()
        }
    }

    fun deleteDriver(d: InstalledDriver) {
        viewModelScope.launch(Dispatchers.IO) {
            DriverRepository.delete(d)
            AppLog.i("drivers", "removido: ${d.name}")
            refreshDrivers()
        }
    }

    // ------------------------------------------------------------------ logs
    fun exportLogs(context: Context) {
        viewModelScope.launch(Dispatchers.IO) {
            busy = Busy(R.string.busy_exporting_logs)
            try {
                val name = com.dfdx047.zandatsu.data.LogExporter.export(context.applicationContext)
                say(R.string.msg_logs_exported, name)
            } catch (e: Exception) {
                say(R.string.err_logs_export, e.message ?: "-")
            } finally {
                busy = null
            }
        }
    }

    // ------------------------------------------------------------ atualizacao
    var updateState by mutableStateOf(UpdateState.IDLE)
        private set
    var updateInfo by mutableStateOf<UpdateInfo?>(null)
        private set
    var showUpdateDialog by mutableStateOf(false)
    private var startupCheckDone = false

    fun checkUpdatesOnLaunch() {
        if (startupCheckDone || !prefs.checkUpdatesOnStart) return
        startupCheckDone = true
        checkUpdates(manual = false)
    }

    fun checkUpdates(manual: Boolean) {
        viewModelScope.launch(Dispatchers.IO) {
            updateState = UpdateState.CHECKING
            try {
                val info = UpdateChecker.check()
                updateInfo = info
                updateState = if (info == null) UpdateState.UP_TO_DATE else UpdateState.AVAILABLE
                if (info != null && (manual || prefs.skippedUpdate != info.version)) showUpdateDialog = true
                if (info == null && manual) say(R.string.msg_up_to_date)
            } catch (e: Exception) {
                updateState = UpdateState.FAILED
                if (manual) say(R.string.err_update_check)
            }
        }
    }

    fun skipUpdate() {
        prefs.skippedUpdate = updateInfo?.version
        showUpdateDialog = false
    }

    fun downloadUpdate(context: Context) {
        val info = updateInfo ?: return
        if (info.apkUrl == null) {
            context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(info.pageUrl)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
            return
        }
        showUpdateDialog = false
        viewModelScope.launch(Dispatchers.IO) {
            busy = Busy(R.string.busy_downloading_update, info.version)
            try {
                val apk = UpdateChecker.downloadApk(info) { done, total ->
                    busy = Busy(R.string.busy_downloading_update, info.version, if (total > 0) done.toFloat() / total else null)
                }
                withContext(Dispatchers.Main) { UpdateChecker.install(context, apk) }
            } catch (e: Exception) {
                say(R.string.err_network, e.message ?: "-")
            } finally {
                busy = null
            }
        }
    }

    // Fica no fim da classe: as propriedades acima ja tem de estar inicializadas.
    init {
        refreshDrivers()  // tambem tira um driver alternativo fora do Adreno
        viewModelScope.launch(Dispatchers.IO) {
            val loaded = GameConfig.load()
            withContext(Dispatchers.Main) { config = loaded }
            // Garante que o toml existe com as opcoes do launcher desde o primeiro arranque.
            GameConfig.save(loaded)
        }
        refreshInstall(false)
        refreshDrivers()
    }

    companion object {
        const val PLAY_ACTION = "com.dfdx047.zandatsu.PLAY"
    }
}
