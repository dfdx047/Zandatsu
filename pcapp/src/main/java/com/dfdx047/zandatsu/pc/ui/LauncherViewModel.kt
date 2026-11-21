package com.dfdx047.zandatsu.pc.ui

import android.app.Application
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Environment
import androidx.annotation.StringRes
import androidx.appcompat.app.AppCompatDelegate
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.core.content.pm.ShortcutInfoCompat
import androidx.core.content.pm.ShortcutManagerCompat
import androidx.core.graphics.drawable.IconCompat
import androidx.core.os.LocaleListCompat
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.dfdx047.zandatsu.pc.GraphicOptions
import com.dfdx047.zandatsu.pc.PcGame
import com.dfdx047.zandatsu.pc.PcGameActivity
import com.dfdx047.zandatsu.pc.PlayShortcutActivity
import com.dfdx047.zandatsu.pc.R
import com.dfdx047.zandatsu.pc.data.AppLanguage
import com.dfdx047.zandatsu.pc.data.AppLog
import com.dfdx047.zandatsu.pc.data.DarkMode
import com.dfdx047.zandatsu.pc.data.GameOrientation
import com.dfdx047.zandatsu.pc.data.LauncherPrefs
import com.dfdx047.zandatsu.pc.data.ThemeMode
import com.dfdx047.zandatsu.pc.update.UpdateChecker
import com.dfdx047.zandatsu.pc.update.UpdateInfo
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.security.MessageDigest

/** Mensagem curta (snackbar) com argumentos. */
class UiMessage(@StringRes val res: Int, val args: List<Any> = emptyList())

enum class ExeCheck { NONE, CHECKING, OK, WRONG, ERROR }
enum class UpdateState { IDLE, CHECKING, UP_TO_DATE, AVAILABLE, FAILED }

/**
 * Estado da launcher de PC (o equivalente do LauncherViewModel da app do 360, sem o 360): tema, idioma, orientacao,
 * o executavel do jogo e a verificacao da versao, JOGAR, o atalho na tela inicial e as atualizacoes.
 */
class LauncherViewModel(app: Application) : AndroidViewModel(app) {
    private val ctx: Context get() = getApplication()
    val prefs = LauncherPrefs(app)

    var message by mutableStateOf<UiMessage?>(null)
    fun say(@StringRes res: Int, vararg args: Any) { message = UiMessage(res, args.toList()) }

    /** Sobe a cada onResume (volta do jogo, do editor, das permissoes): os ecras releem as preferencias. */
    var resumes by mutableIntStateOf(0)
        private set

    // ------------------------------------------------------------------------------------------------- aparencia
    var themeMode by mutableStateOf(prefs.themeMode)
        private set
    var darkMode by mutableStateOf(prefs.darkMode)
        private set
    fun setTheme(v: ThemeMode) { themeMode = v; prefs.themeMode = v }
    fun setDark(v: DarkMode) { darkMode = v; prefs.darkMode = v }

    val appLanguage: AppLanguage
        get() {
            val current = AppCompatDelegate.getApplicationLocales().toLanguageTags().substringBefore('-')
            return AppLanguage.entries.firstOrNull { it.tag.isNotEmpty() && it.tag == current } ?: AppLanguage.SYSTEM
        }
    fun setLanguage(v: AppLanguage) {
        AppCompatDelegate.setApplicationLocales(
            if (v.tag.isEmpty()) LocaleListCompat.getEmptyLocaleList() else LocaleListCompat.forLanguageTags(v.tag),
        )
    }

    var gameOrientation by mutableStateOf(prefs.gameOrientation)
        private set
    fun setOrientation(v: GameOrientation) { gameOrientation = v; prefs.gameOrientation = v }

    // --------------------------------------------------------------------------------------------- jogo (exe)
    val packaged: Boolean = PcGame.available(app)
    var exe by mutableStateOf("")
        private set
    var access by mutableStateOf(false)
        private set
    var check by mutableStateOf(ExeCheck.NONE)
        private set
    var checkDetail by mutableStateOf("")
        private set
    val exeFound: Boolean get() = exe.isNotEmpty() && File(exe).isFile
    val canPlay: Boolean get() = exeFound && access && packaged && check != ExeCheck.WRONG
    /** Opcoes do jogo nao gravadas antes de JOGAR: o dialogo pergunta se joga na mesma. */
    var goWarning by mutableStateOf<String?>(null)
    private var checkJob: Job? = null
    private var checkedFor = ""

    init {
        exe = PcGame.exePath(app).ifEmpty { PcGame.findExe().orEmpty().also { if (it.isNotEmpty()) PcGame.setExePath(app, it) } }
        if (PcGame.profile(app).isEmpty() && exe.isNotEmpty()) PcGame.applyProfile(app, PcGame.defaultProfile())
        refreshGame()
    }

    fun onResume() {
        resumes++
        refreshGame()
    }

    fun setExePath(path: String) {
        exe = path.trim()
        PcGame.setExePath(ctx, exe)
        if (PcGame.profile(ctx).isEmpty() && exe.isNotEmpty()) PcGame.applyProfile(ctx, PcGame.defaultProfile())
        refreshGame()
    }

    fun searchExe() {
        val found = PcGame.findExe()
        if (found != null) setExePath(found) else say(R.string.msg_exe_not_found, PcGame.EXE_NAME)
    }

    /** Acesso a todos os ficheiros e verificacao do exe: a chave do libmgrpc e o SHA-256 da .text do exe do jogador. */
    fun refreshGame() {
        access = Environment.isExternalStorageManager()
        val key = "$exe|$access"
        if (key == checkedFor && check != ExeCheck.NONE) return
        checkedFor = key
        checkJob?.cancel()
        check = ExeCheck.NONE
        if (!exeFound || !access || !packaged) return
        check = ExeCheck.CHECKING
        val path = exe
        checkJob = viewModelScope.launch {
            val r = withContext(Dispatchers.IO) {
                runCatching {
                    val meta = JSONObject(ctx.assets.open("libmgrpc.json").bufferedReader().readText())
                    val k = PcGame.textKey(File(path))
                    val sha = MessageDigest.getInstance("SHA-256").digest(k).joinToString("") { "%02x".format(it) }
                    (sha == meta.getString("key_sha256")) to meta.optString("exe_sha1").take(12)
                }
            }
            r.onSuccess { (ok, sha1) -> check = if (ok) ExeCheck.OK else ExeCheck.WRONG; checkDetail = sha1 }
                .onFailure { check = ExeCheck.ERROR; checkDetail = it.message ?: it.toString() }
        }
    }

    /** Grava as opcoes do jogo outra vez (com verificacao) e abre o jogo. */
    fun play(context: Context) {
        if (!canPlay) {
            say(R.string.msg_not_ready)
            return
        }
        viewModelScope.launch {
            val r = withContext(Dispatchers.IO) { GraphicOptions.write(context) }
            if (r.ok) launch(context) else goWarning = context.getString(R.string.go_write_failed, r.path, r.error ?: "?")
        }
    }

    fun launch(context: Context) {
        AppLog.i("launcher", "jogar: perfil ${PcGame.profile(context)} ${PcGame.renderScale(context)}% ${PcGame.fpsCap(context)} fps")
        context.startActivity(Intent(context, PcGameActivity::class.java))
    }

    /** Atalho fixo na tela inicial: abre o jogo direto (PlayShortcutActivity grava as opcoes e lanca). */
    fun createShortcut(context: Context) {
        if (!ShortcutManagerCompat.isRequestPinShortcutSupported(context)) {
            say(R.string.msg_shortcut_unsupported)
            return
        }
        val info = ShortcutInfoCompat.Builder(context, "play_mgr_pc")
            .setShortLabel(context.getString(R.string.shortcut_short))
            .setLongLabel(context.getString(R.string.shortcut_long))
            .setIcon(IconCompat.createWithResource(context, R.mipmap.ic_launcher))
            .setIntent(Intent(context, PlayShortcutActivity::class.java).setAction(PlayShortcutActivity.ACTION))
            .build()
        if (ShortcutManagerCompat.requestPinShortcut(context, info, null)) say(R.string.msg_shortcut_requested)
        else say(R.string.msg_shortcut_unsupported)
    }

    // -------------------------------------------------------------------------------------------- atualizacoes
    var checkUpdatesOnStart by mutableStateOf(prefs.checkUpdatesOnStart)
        private set
    fun changeCheckUpdatesOnStart(v: Boolean) { checkUpdatesOnStart = v; prefs.checkUpdatesOnStart = v }
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
        viewModelScope.launch {
            updateState = UpdateState.CHECKING
            try {
                val info = withContext(Dispatchers.IO) { UpdateChecker.check() }
                updateInfo = info
                updateState = if (info == null) UpdateState.UP_TO_DATE else UpdateState.AVAILABLE
                if (info != null && (manual || prefs.skippedUpdate != info.version)) showUpdateDialog = true
                if (info == null && manual) say(R.string.msg_up_to_date)
            } catch (e: Exception) {
                AppLog.w("update", "falhou a verificacao", e)
                updateState = UpdateState.FAILED
                if (manual) say(R.string.err_update_check)
            }
        }
    }

    fun skipUpdate() {
        prefs.skippedUpdate = updateInfo?.version
        showUpdateDialog = false
    }

    /** O APK (ou a pagina da release) abre no navegador; o sistema instala por cima (mesma assinatura). */
    fun downloadUpdate(context: Context) {
        val info = updateInfo ?: return
        showUpdateDialog = false
        runCatching { context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(info.apkUrl ?: info.pageUrl))) }
    }
}
