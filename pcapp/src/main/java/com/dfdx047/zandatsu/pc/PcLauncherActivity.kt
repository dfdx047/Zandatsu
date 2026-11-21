package com.dfdx047.zandatsu.pc

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Gamepad
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.SportsEsports
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationRail
import androidx.compose.material3.NavigationRailItem
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.core.os.LocaleListCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import com.dfdx047.zandatsu.pc.controls.ControlsEditorActivity
import com.dfdx047.zandatsu.pc.data.AppLanguage
import com.dfdx047.zandatsu.pc.data.AppPaths
import com.dfdx047.zandatsu.pc.data.ButtonBadge
import com.dfdx047.zandatsu.pc.data.ControlStyle
import com.dfdx047.zandatsu.pc.data.DarkMode
import com.dfdx047.zandatsu.pc.data.Directional
import com.dfdx047.zandatsu.pc.data.LauncherPrefs
import com.dfdx047.zandatsu.pc.data.PcTouchMode
import com.dfdx047.zandatsu.pc.data.ThemeMode
import com.dfdx047.zandatsu.pc.drivers.DriverAsset
import com.dfdx047.zandatsu.pc.drivers.DriverException
import com.dfdx047.zandatsu.pc.drivers.DriverRelease
import com.dfdx047.zandatsu.pc.drivers.DriverRepository
import com.dfdx047.zandatsu.pc.drivers.DriverSource
import com.dfdx047.zandatsu.pc.drivers.InstalledDriver
import com.dfdx047.zandatsu.pc.ui.theme.DfColors
import com.dfdx047.zandatsu.pc.ui.theme.LogoStyle
import com.dfdx047.zandatsu.pc.ui.theme.SwordManTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.security.MessageDigest
import java.text.DateFormat
import java.util.Date
import kotlin.math.roundToInt

/**
 * Launcher da app Zandatsu PC (so o port de PC do MGR:R). Separadores: Jogo, Graficos, Controles, Sistema, Sobre.
 * As opcoes ficam nas preferencias (PcGame, GraphicOptions, LauncherPrefs) e o PcGameActivity passa-as ao nativo por
 * variaveis de ambiente.
 */
class PcLauncherActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        AppPaths.init(this)
        setContent {
            SwordManTheme(themeMode = ThemeMode.GAME, darkMode = DarkMode.DARK) {
                Surface(Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) { PcLauncher() }
            }
        }
    }
}

private enum class Tab(val label: Int, val icon: ImageVector) {
    GAME(R.string.tab_game, Icons.Filled.SportsEsports),
    GRAPHICS(R.string.tab_graphics, Icons.Filled.Tune),
    CONTROLS(R.string.tab_controls, Icons.Filled.Gamepad),
    SYSTEM(R.string.tab_system, Icons.Filled.Settings),
    ABOUT(R.string.tab_about, Icons.Filled.Info),
}

@Composable
private fun PcLauncher() {
    var tab by rememberSaveable { mutableStateOf(Tab.GAME) }
    // Volta de outra app (permissao, SAF, jogo): os separadores releem o estado.
    var resumes by remember { mutableIntStateOf(0) }
    val owner = LocalLifecycleOwner.current
    androidx.compose.runtime.DisposableEffect(owner) {
        val obs = LifecycleEventObserver { _, e -> if (e == Lifecycle.Event.ON_RESUME) resumes++ }
        owner.lifecycle.addObserver(obs)
        onDispose { owner.lifecycle.removeObserver(obs) }
    }
    BoxWithConstraints(Modifier.fillMaxSize().safeDrawingPadding()) {
        val wide = maxWidth > maxHeight
        val content: @Composable () -> Unit = {
            key(resumes) {
                Column(
                    Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    when (tab) {
                        Tab.GAME -> GameTab()
                        Tab.GRAPHICS -> GraphicsTab()
                        Tab.CONTROLS -> ControlsTab()
                        Tab.SYSTEM -> SystemTab()
                        Tab.ABOUT -> AboutTab()
                    }
                }
            }
        }
        if (wide) {
            Row(Modifier.fillMaxSize()) {
                NavigationRail {
                    Spacer(Modifier.height(8.dp))
                    Tab.entries.forEach { t ->
                        NavigationRailItem(tab == t, { tab = t }, { Icon(t.icon, null) }, label = { Text(stringResource(t.label)) })
                    }
                }
                Box(Modifier.weight(1f)) { content() }
            }
        } else {
            Column(Modifier.fillMaxSize()) {
                Box(Modifier.weight(1f)) { content() }
                NavigationBar {
                    Tab.entries.forEach { t ->
                        NavigationBarItem(tab == t, { tab = t }, { Icon(t.icon, null) }, label = { Text(stringResource(t.label), maxLines = 1) })
                    }
                }
            }
        }
    }
}

// ------------------------------------------------------------------------------------------------ pecas comuns

@Composable
private fun Header(text: String) = Text(text, style = MaterialTheme.typography.titleLarge)

@Composable
private fun Section(text: String) = Text(text, style = MaterialTheme.typography.titleMedium, modifier = Modifier.padding(top = 4.dp))

@Composable
private fun Hint(text: String) =
    Text(text, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)

@Composable
private fun Warning(text: String) = Card(
    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.errorContainer.copy(alpha = 0.35f)),
    modifier = Modifier.fillMaxWidth(),
) { Text(text, Modifier.padding(12.dp), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.error) }

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun <T> Chips(options: List<Pair<T, String>>, selected: T, onSelect: (T) -> Unit) {
    FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        options.forEach { (v, l) -> FilterChip(selected == v, { onSelect(v) }, { Text(l) }) }
    }
}

@Composable
private fun SwitchRow(title: String, subtitle: String? = null, checked: Boolean, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text(title, style = MaterialTheme.typography.bodyLarge)
            if (subtitle != null) Hint(subtitle)
        }
        Switch(checked, onChange)
    }
}

@Composable
private fun StatusRow(label: String, value: String, ok: Boolean?) {
    Row(Modifier.fillMaxWidth()) {
        Text(label, Modifier.weight(1f), style = MaterialTheme.typography.bodyMedium)
        Text(value, style = MaterialTheme.typography.bodyMedium, fontWeight = FontWeight.Bold,
            color = when (ok) { true -> DfColors.Ok; false -> DfColors.Error; null -> MaterialTheme.colorScheme.onSurfaceVariant })
    }
}

private fun formatTime(ms: Long): String = DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.MEDIUM).format(Date(ms))

// ------------------------------------------------------------------------------------------------ Jogo

private enum class ExeCheck { NONE, CHECKING, OK, WRONG, ERROR }

@Composable
private fun GameTab() {
    val c = LocalContext.current
    val scope = rememberCoroutineScope()
    var exe by remember { mutableStateOf(PcGame.exePath(c).ifEmpty { PcGame.findExe().orEmpty().also { if (it.isNotEmpty()) PcGame.setExePath(c, it) } }) }
    remember(exe) { if (PcGame.profile(c).isEmpty() && exe.isNotEmpty()) PcGame.applyProfile(c, PcGame.defaultProfile()); 0 }
    val access = Environment.isExternalStorageManager()
    val found = exe.isNotEmpty() && File(exe).isFile
    val packaged = remember { PcGame.available(c) }
    var check by remember { mutableStateOf(ExeCheck.NONE) }
    var checkDetail by remember { mutableStateOf("") }
    // Verificacao do exe: a chave do libmgrpc e o SHA-256 da .text do exe do jogador (o mesmo teste do arranque).
    LaunchedEffect(exe, access) {
        check = ExeCheck.NONE
        if (!found || !access || !packaged) return@LaunchedEffect
        check = ExeCheck.CHECKING
        val r = withContext(Dispatchers.IO) {
            runCatching {
                val meta = JSONObject(c.assets.open("libmgrpc.json").bufferedReader().readText())
                val key = PcGame.textKey(File(exe))
                val sha = MessageDigest.getInstance("SHA-256").digest(key).joinToString("") { "%02x".format(it) }
                (sha == meta.getString("key_sha256")) to meta.optString("exe_sha1").take(12)
            }
        }
        r.onSuccess { (ok, sha1) -> check = if (ok) ExeCheck.OK else ExeCheck.WRONG; checkDetail = sha1 }
            .onFailure { check = ExeCheck.ERROR; checkDetail = it.message ?: it.toString() }
    }
    var goWarning by remember { mutableStateOf<String?>(null) }
    fun launch() = c.startActivity(Intent(c, PcGameActivity::class.java))

    Text(stringResource(R.string.app_title), style = LogoStyle, color = MaterialTheme.colorScheme.primary)
    Hint(stringResource(R.string.game_intro))
    if (!access) {
        Warning(stringResource(R.string.game_need_access))
        Button(onClick = {
            c.startActivity(Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:${c.packageName}")))
        }) { Text(stringResource(R.string.game_grant_access)) }
    }
    Section(stringResource(R.string.game_folder))
    OutlinedTextField(exe, { exe = it.trim(); PcGame.setExePath(c, exe) }, Modifier.fillMaxWidth(),
        label = { Text(stringResource(R.string.game_exe_label)) }, singleLine = true)
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedButton(onClick = { PcGame.findExe()?.let { exe = it; PcGame.setExePath(c, it) } }) { Text(stringResource(R.string.game_search)) }
    }
    Hint(stringResource(R.string.game_search_hint, PcGame.EXE_NAME))

    Section(stringResource(R.string.game_status))
    StatusRow(stringResource(R.string.status_packaged), stringResource(if (packaged) R.string.yes else R.string.status_missing), packaged)
    StatusRow(stringResource(R.string.status_access), stringResource(if (access) R.string.yes else R.string.no), access)
    StatusRow(stringResource(R.string.status_exe), stringResource(if (found) R.string.status_found else R.string.status_not_found), found)
    StatusRow(stringResource(R.string.status_version), when (check) {
        ExeCheck.NONE -> "—"
        ExeCheck.CHECKING -> stringResource(R.string.status_checking)
        ExeCheck.OK -> stringResource(R.string.status_supported)
        ExeCheck.WRONG -> stringResource(R.string.status_unsupported)
        ExeCheck.ERROR -> stringResource(R.string.status_error)
    }, when (check) { ExeCheck.OK -> true; ExeCheck.WRONG, ExeCheck.ERROR -> false; else -> null })
    if (check == ExeCheck.WRONG) Warning(stringResource(R.string.status_unsupported_detail, checkDetail))
    if (check == ExeCheck.ERROR) Warning(checkDetail)

    Spacer(Modifier.height(8.dp))
    Button(
        onClick = {
            scope.launch {
                // As opcoes do jogo sao gravadas outra vez antes de lancar (com verificacao).
                val r = withContext(Dispatchers.IO) { GraphicOptions.write(c) }
                if (r.ok) launch() else goWarning = c.getString(R.string.go_write_failed, r.path, r.error ?: "?")
            }
        },
        enabled = found && access && packaged && check != ExeCheck.WRONG,
        modifier = Modifier.fillMaxWidth().height(64.dp),
    ) { Text(stringResource(R.string.play), style = MaterialTheme.typography.titleLarge) }
    Hint(stringResource(R.string.play_hint))

    goWarning?.let { msg ->
        AlertDialog(
            onDismissRequest = { goWarning = null },
            title = { Text(stringResource(R.string.go_write_failed_title)) },
            text = { Text(msg) },
            confirmButton = { TextButton({ goWarning = null; launch() }) { Text(stringResource(R.string.play_anyway)) } },
            dismissButton = { TextButton({ goWarning = null }) { Text(stringResource(R.string.cancel)) } },
        )
    }
}

// ------------------------------------------------------------------------------------------------ Graficos

@Composable
private fun GraphicsTab() {
    val c = LocalContext.current
    val (nativeW, nativeH) = remember { PcGame.nativeSize(c) }
    var profile by remember { mutableStateOf(PcGame.profile(c).ifEmpty { PcGame.defaultProfile() }) }
    var scale by remember { mutableIntStateOf(PcGame.renderScale(c)) }
    var fps by remember { mutableIntStateOf(PcGame.fpsCap(c)) }
    var fsr by remember { mutableIntStateOf(PcGame.fsrMode(c)) }
    var sharp by remember { mutableFloatStateOf(PcGame.fsrSharpness(c)) }
    var fsrOut by remember { mutableIntStateOf(PcGame.fsrOut(c)) }
    var interp by remember { mutableStateOf(PcGame.frameInterp(c)) }
    var opts by remember { mutableStateOf(GraphicOptions.values(c)) }
    var lastWrite by remember { mutableStateOf(GraphicOptions.lastResult(c)) }
    fun custom() { if (profile != "custom") { profile = "custom"; PcGame.setProfile(c, "custom") } }

    Header(stringResource(R.string.tab_graphics))
    Section(stringResource(R.string.gfx_profile))
    val profiles = PcGame.PROFILES.map { it.id to stringResource(it.label) } + ("custom" to stringResource(R.string.profile_custom))
    Chips(profiles, profile) { id ->
        if (id == "custom") { custom(); return@Chips }
        lastWrite = PcGame.applyProfile(c, id) ?: lastWrite
        profile = id; scale = PcGame.renderScale(c); fps = PcGame.fpsCap(c); interp = PcGame.frameInterp(c)
        opts = GraphicOptions.values(c)
    }
    Hint(stringResource(R.string.gfx_recommended, stringResource(PcGame.PROFILES.first { it.id == PcGame.defaultProfile() }.label)))

    Section(stringResource(R.string.gfx_resolution))
    val size = PcGame.renderSizeFor(nativeW, nativeH, scale).ifEmpty { "${nativeW}x$nativeH" }
    Text(stringResource(R.string.gfx_resolution_value, size, scale), style = MaterialTheme.typography.bodyLarge, fontWeight = FontWeight.Bold)
    Slider(
        value = scale.toFloat(),
        onValueChange = { scale = ((it / PcGame.SCALE_STEP).roundToInt() * PcGame.SCALE_STEP).coerceIn(PcGame.SCALE_MIN, 100) },
        onValueChangeFinished = { PcGame.setRenderScale(c, scale); custom() },
        valueRange = PcGame.SCALE_MIN.toFloat()..100f,
        steps = (100 - PcGame.SCALE_MIN) / PcGame.SCALE_STEP - 1,
    )
    Hint(stringResource(R.string.gfx_resolution_hint, "${nativeW}x$nativeH"))

    Section(stringResource(R.string.gfx_fps))
    Chips(PcGame.FPS_CAPS.map { it to it.toString() }, fps) { v ->
        fps = v; PcGame.setFpsCap(c, v); custom()
        // A geracao de quadros so serve com o jogo a 30 fps.
        if (v != 30 && interp) { interp = false; PcGame.setFrameInterp(c, false) }
    }
    Hint(stringResource(R.string.gfx_fps_hint))

    Section(stringResource(R.string.gfx_upscale))
    Chips(listOf(0 to stringResource(R.string.gfx_upscale_bilinear), 1 to stringResource(R.string.gfx_upscale_fsr1),
        2 to stringResource(R.string.gfx_upscale_cas)), fsr) { v -> fsr = v; PcGame.setFsrMode(c, v); custom() }
    Hint(stringResource(when (fsr) { 1 -> R.string.gfx_upscale_fsr1_sub; 2 -> R.string.gfx_upscale_cas_sub; else -> R.string.gfx_upscale_bilinear_sub }))
    if (fsr != 0) {
        Text(stringResource(R.string.gfx_sharpness, (sharp * 100).roundToInt()), style = MaterialTheme.typography.labelLarge)
        Slider(sharp, { sharp = (it * 20).roundToInt() / 20f }, valueRange = 0f..1f, steps = 19,
            onValueChangeFinished = { PcGame.setFsrSharpness(c, sharp); custom() })
        Text(stringResource(R.string.gfx_upscale_output), style = MaterialTheme.typography.labelLarge)
        Chips(listOf(0 to stringResource(R.string.gfx_output_screen), 720 to stringResource(R.string.gfx_output_720)), fsrOut) { v ->
            fsrOut = v; PcGame.setFsrOut(c, v); custom()
        }
    }

    Section(stringResource(R.string.gfx_framegen))
    SwitchRow(stringResource(R.string.gfx_framegen_switch), null, interp) { on ->
        interp = on; PcGame.setFrameInterp(c, on); custom()
        if (on && fps != 30) { fps = 30; PcGame.setFpsCap(c, 30) }
    }
    Hint(stringResource(R.string.gfx_framegen_sub))
    if (interp) Hint(stringResource(R.string.gfx_framegen_warning)) else Warning(stringResource(R.string.gfx_framegen_warning))

    // Opcoes do proprio jogo (GraphicOption).
    Section(stringResource(R.string.go_title))
    Hint(stringResource(R.string.go_sub))
    fun set(i: Int, x: Int) {
        opts = opts.copyOf().also { it[i] = x }
        lastWrite = GraphicOptions.save(c, opts)
        custom()
    }
    @Composable fun row(label: Int, i: Int, options: List<Pair<Int, String>>) {
        Text(stringResource(label), style = MaterialTheme.typography.labelLarge)
        Chips(options, opts[i]) { set(i, it) }
    }
    val off = stringResource(R.string.off); val on = stringResource(R.string.on)
    val low = stringResource(R.string.go_low); val med = stringResource(R.string.go_medium)
    row(R.string.go_aa, 7, listOf(0 to off, 1 to "MSAA x2"))
    row(R.string.go_filter, 1, listOf(0 to off, 1 to stringResource(R.string.go_aniso, 2), 2 to stringResource(R.string.go_aniso, 4)))
    row(R.string.go_blur, 2, listOf(0 to on, 1 to off))
    row(R.string.go_shadow, 3, listOf(0 to low, 1 to med))
    row(R.string.go_zangeki, 8, listOf(0 to low, 1 to med))
    row(R.string.go_zantime, 9, listOf(0 to low, 1 to med))
    row(R.string.go_window, 6, listOf(0 to stringResource(R.string.go_fullscreen), 1 to stringResource(R.string.go_windowed)))
    row(R.string.go_brightness, 4, (0..10).map { it to "$it" })

    val exe = PcGame.exePath(c)
    Text(stringResource(R.string.go_file, if (exe.isNotEmpty()) GraphicOptions.file(exe).path else "—"),
        style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
    val lw = lastWrite
    when {
        lw == null -> Hint(stringResource(R.string.go_never_written))
        lw.ok -> Text(stringResource(R.string.go_written_at, formatTime(lw.at), lw.via.uppercase()),
            style = MaterialTheme.typography.bodySmall, color = DfColors.Ok)
        else -> Warning(stringResource(R.string.go_write_failed, lw.path, lw.error ?: "?") + "\n" + formatTime(lw.at))
    }
    if (lw != null && !lw.ok) SafAccess()
    Hint(stringResource(R.string.gfx_renderer))
}

/** Acesso a pasta do jogo pelo SAF (alternativa quando a escrita por caminho no cartao SD falha). */
@Composable
private fun SafAccess() {
    val c = LocalContext.current
    var tree by remember { mutableStateOf(GraphicOptions.treeUri(c)) }
    val pick = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri ->
        if (uri != null) {
            runCatching {
                c.contentResolver.takePersistableUriPermission(uri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
            }
            GraphicOptions.setTreeUri(c, uri); tree = uri
        }
    }
    Hint(stringResource(R.string.saf_hint))
    Text(stringResource(R.string.saf_current, tree?.let { GraphicOptions.treePath(it) ?: it.toString() } ?: "—"),
        style = MaterialTheme.typography.bodySmall)
    OutlinedButton(onClick = {
        val start = PcGame.exePath(c).let { File(it).parentFile }
        pick.launch(start?.let { folderUri(it) })
    }) { Text(stringResource(R.string.saf_pick)) }
}

/** URI inicial do seletor de pastas para um caminho /storage/<vol>/... (so uma sugestao ao sistema). */
private fun folderUri(dir: File): Uri? = runCatching {
    val p = dir.absolutePath
    val (vol, rel) = when {
        p.startsWith("/storage/emulated/0/") -> "primary" to p.removePrefix("/storage/emulated/0/")
        p.startsWith("/sdcard/") -> "primary" to p.removePrefix("/sdcard/")
        p.startsWith("/storage/") -> p.removePrefix("/storage/").substringBefore('/') to p.removePrefix("/storage/").substringAfter('/', "")
        else -> return null
    }
    android.provider.DocumentsContract.buildDocumentUri("com.android.externalstorage.documents", "$vol:$rel")
}.getOrNull()

// ------------------------------------------------------------------------------------------------ Controles

@Composable
private fun ControlsTab() {
    val c = LocalContext.current
    val prefs = remember { LauncherPrefs(c) }
    var mode by remember { mutableStateOf(prefs.pcTouchMode) }
    Header(stringResource(R.string.tab_controls))
    Section(stringResource(R.string.ctl_mode))
    Chips(listOf(PcTouchMode.BUTTONS to stringResource(R.string.ctl_mode_buttons),
        PcTouchMode.GESTURES to stringResource(R.string.ctl_mode_gestures),
        PcTouchMode.MOUSE to stringResource(R.string.ctl_mode_mouse)), mode) { mode = it; prefs.pcTouchMode = it }
    Card(Modifier.fillMaxWidth()) {
        Text(stringResource(when (mode) {
            PcTouchMode.BUTTONS -> R.string.ctl_mode_buttons_sub
            PcTouchMode.GESTURES -> R.string.ctl_mode_gestures_sub
            PcTouchMode.MOUSE -> R.string.ctl_mode_mouse_sub
        }), Modifier.padding(12.dp), style = MaterialTheme.typography.bodyMedium)
    }
    Hint(stringResource(R.string.ctl_pad_hint))

    if (mode == PcTouchMode.BUTTONS) {
        Section(stringResource(R.string.ctl_layout))
        Button(onClick = { c.startActivity(Intent(c, ControlsEditorActivity::class.java)) }) { Text(stringResource(R.string.ctl_edit_layout)) }
        Hint(stringResource(R.string.ctl_edit_hint))
        var style by remember { mutableStateOf(prefs.controlStyle) }
        Text(stringResource(R.string.ctl_style), style = MaterialTheme.typography.labelLarge)
        Chips(listOf(ControlStyle.GAME to stringResource(R.string.ctl_style_classic), ControlStyle.SCIFI to "Sci-fi",
            ControlStyle.XBOX to "Xbox", ControlStyle.PLAYSTATION to "PlayStation", ControlStyle.NINTENDO to "Nintendo"), style) {
            style = it; prefs.controlStyle = it
        }
        var dir by remember { mutableStateOf(prefs.directional) }
        Text(stringResource(R.string.ctl_directional), style = MaterialTheme.typography.labelLarge)
        Chips(listOf(Directional.STICK to stringResource(R.string.ctl_stick), Directional.DPAD to stringResource(R.string.ctl_dpad)), dir) {
            dir = it; prefs.directional = it
        }
        var badge by remember { mutableStateOf(prefs.buttonBadge) }
        Text(stringResource(R.string.ctl_badge), style = MaterialTheme.typography.labelLarge)
        Chips(listOf(ButtonBadge.XBOX to "Xbox", ButtonBadge.PLAYSTATION to "PlayStation", ButtonBadge.NINTENDO to "Nintendo",
            ButtonBadge.NONE to stringResource(R.string.ctl_badge_none)), badge) { badge = it; prefs.buttonBadge = it }
        var opacity by remember { mutableFloatStateOf(prefs.touchOpacity) }
        Text(stringResource(R.string.ctl_opacity, (opacity * 100).roundToInt()), style = MaterialTheme.typography.labelLarge)
        Slider(opacity, { opacity = (it * 20).roundToInt() / 20f }, valueRange = 0.1f..1f, onValueChangeFinished = { prefs.touchOpacity = opacity })
        var scaleT by remember { mutableFloatStateOf(prefs.touchScale) }
        Text(stringResource(R.string.ctl_scale, (scaleT * 100).roundToInt()), style = MaterialTheme.typography.labelLarge)
        Slider(scaleT, { scaleT = (it * 20).roundToInt() / 20f }, valueRange = 0.5f..1.6f, onValueChangeFinished = { prefs.touchScale = scaleT })
        var haptics by remember { mutableStateOf(prefs.touchHaptics) }
        SwitchRow(stringResource(R.string.ctl_haptics), null, haptics) { haptics = it; prefs.touchHaptics = it }
        var blade by remember { mutableStateOf(prefs.bladeToggle) }
        SwitchRow(stringResource(R.string.ctl_blade_toggle), stringResource(R.string.ctl_blade_toggle_sub), blade) { blade = it; prefs.bladeToggle = it }
    }
    var pause by remember { mutableStateOf(prefs.pauseOnMenus) }
    SwitchRow(stringResource(R.string.ctl_pause_menu), stringResource(R.string.ctl_pause_menu_sub), pause) { pause = it; prefs.pauseOnMenus = it }
}

// ------------------------------------------------------------------------------------------------ Sistema

@Composable
private fun SystemTab() {
    val c = LocalContext.current
    val scope = rememberCoroutineScope()
    var drivers by remember { mutableStateOf(DriverRepository.installed()) }
    var driver by remember { mutableStateOf(PcGame.driverDir(c)) }
    var busy by remember { mutableStateOf<String?>(null) }
    var progress by remember { mutableFloatStateOf(-1f) }
    var error by remember { mutableStateOf<String?>(null) }
    var showDownload by remember { mutableStateOf(false) }
    var toDelete by remember { mutableStateOf<InstalledDriver?>(null) }
    fun reasonText(e: Throwable): String = if (e is DriverException) c.getString(when (e.reason) {
        DriverException.Reason.NO_LIBRARY -> R.string.drv_err_no_library
        DriverException.Reason.NOT_ZIP -> R.string.drv_err_not_zip
        DriverException.Reason.IO -> R.string.drv_err_io
        DriverException.Reason.NETWORK -> R.string.drv_err_network
        DriverException.Reason.MIN_API -> R.string.drv_err_min_api
        DriverException.Reason.WRONG_GPU -> R.string.drv_err_wrong_gpu
    }) + (e.message?.takeIf { it.isNotBlank() }?.let { " ($it)" } ?: "") else (e.message ?: e.toString())
    fun install(label: String, work: suspend () -> InstalledDriver) {
        busy = label; progress = -1f; error = null
        scope.launch {
            runCatching { withContext(Dispatchers.IO) { work() } }
                .onSuccess { d -> drivers = DriverRepository.installed(); driver = d.dir.absolutePath; PcGame.setDriverDir(c, driver) }
                .onFailure { error = reasonText(it) }
            busy = null
        }
    }
    val import = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            val name = c.contentResolver.query(uri, arrayOf(android.provider.OpenableColumns.DISPLAY_NAME), null, null, null)
                ?.use { if (it.moveToFirst()) it.getString(0) else null } ?: "driver.zip"
            install(name) { DriverRepository.importUri(c, uri, name) }
        }
    }

    Header(stringResource(R.string.tab_system))
    Section(stringResource(R.string.sys_driver))
    Hint(stringResource(R.string.sys_driver_sub, DriverRepository.gpuName().ifEmpty { DriverRepository.socName() }))
    Chips(listOf("" to stringResource(R.string.sys_driver_system)) + drivers.map { it.dir.absolutePath to it.name }, driver) {
        driver = it; PcGame.setDriverDir(c, it)
    }
    drivers.forEach { d ->
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text(d.name, style = MaterialTheme.typography.bodyMedium)
                Hint(listOf(d.version, d.author, d.library.name).filter { it.isNotBlank() }.joinToString(" · "))
            }
            IconButton({ toDelete = d }) { Icon(Icons.Filled.Delete, stringResource(R.string.delete)) }
        }
    }
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedButton(onClick = { import.launch(arrayOf("application/zip", "application/octet-stream", "*/*")) }, enabled = busy == null) {
            Text(stringResource(R.string.sys_driver_import))
        }
        if (DriverRepository.sourcesForDevice().isNotEmpty()) {
            OutlinedButton(onClick = { showDownload = true }, enabled = busy == null) { Text(stringResource(R.string.sys_driver_download)) }
        }
    }
    busy?.let {
        Text(stringResource(R.string.sys_driver_installing, it), style = MaterialTheme.typography.bodySmall)
        if (progress >= 0f) LinearProgressIndicator({ progress }, Modifier.fillMaxWidth()) else LinearProgressIndicator(Modifier.fillMaxWidth())
    }
    error?.let { Warning(it) }
    Hint(stringResource(R.string.sys_driver_hint))

    Section(stringResource(R.string.sys_cpu))
    var mt by remember { mutableStateOf(PcGame.multithread(c)) }
    SwitchRow(stringResource(R.string.sys_mt), stringResource(R.string.sys_mt_sub), mt) { mt = it; PcGame.setMultithread(c, it) }

    Section(stringResource(R.string.sys_language))
    val current = AppCompatDelegate.getApplicationLocales().toLanguageTags().substringBefore('-')
    val lang = AppLanguage.entries.firstOrNull { it.tag.isNotEmpty() && it.tag == current } ?: AppLanguage.SYSTEM
    Chips(listOf(AppLanguage.SYSTEM to stringResource(R.string.sys_language_system), AppLanguage.PORTUGUESE to "Português",
        AppLanguage.ENGLISH to "English", AppLanguage.SPANISH to "Español"), lang) {
        AppCompatDelegate.setApplicationLocales(if (it.tag.isEmpty()) LocaleListCompat.getEmptyLocaleList() else LocaleListCompat.forLanguageTags(it.tag))
    }

    toDelete?.let { d ->
        AlertDialog(
            onDismissRequest = { toDelete = null },
            title = { Text(stringResource(R.string.sys_driver_delete_title)) },
            text = { Text(d.name) },
            confirmButton = {
                TextButton({
                    DriverRepository.delete(d)
                    if (driver == d.dir.absolutePath) { driver = ""; PcGame.setDriverDir(c, "") }
                    drivers = DriverRepository.installed(); toDelete = null
                }) { Text(stringResource(R.string.delete)) }
            },
            dismissButton = { TextButton({ toDelete = null }) { Text(stringResource(R.string.cancel)) } },
        )
    }
    if (showDownload) DownloadDialog(onDismiss = { showDownload = false }) { asset ->
        showDownload = false
        install(asset.name) {
            DriverRepository.download(asset) { done, total -> progress = if (total > 0) done.toFloat() / total else -1f }
        }
    }
}

/** Escolher uma fonte (releases do GitHub), uma release e o anexo a instalar. */
@Composable
private fun DownloadDialog(onDismiss: () -> Unit, onPick: (DriverAsset) -> Unit) {
    var source by remember { mutableStateOf<DriverSource?>(null) }
    var releases by remember { mutableStateOf<List<DriverRelease>?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(source) {
        val s = source ?: return@LaunchedEffect
        releases = null; error = null
        runCatching { withContext(Dispatchers.IO) { DriverRepository.fetchReleases(s) } }
            .onSuccess { releases = it }.onFailure { error = it.message ?: it.toString() }
    }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.sys_driver_download)) },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState()).widthIn(max = 480.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                val s = source
                if (s == null) {
                    DriverRepository.sourcesForDevice().forEach { src ->
                        OutlinedButton({ source = src }, Modifier.fillMaxWidth()) { Text(src.label) }
                    }
                } else {
                    Text(s.label, style = MaterialTheme.typography.titleSmall)
                    when {
                        error != null -> Text(error!!, color = MaterialTheme.colorScheme.error)
                        releases == null -> CircularProgressIndicator()
                        releases!!.isEmpty() -> Text("—")
                        else -> releases!!.take(8).forEach { r ->
                            Text("${r.title} (${r.date})", style = MaterialTheme.typography.labelLarge)
                            r.assets.forEach { a ->
                                TextButton({ onPick(a) }) { Text("${a.name} · ${a.size / 1024 / 1024} MB") }
                            }
                        }
                    }
                }
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton({ if (source != null) source = null else onDismiss() }) {
                Text(stringResource(if (source != null) R.string.back else R.string.cancel))
            }
        },
    )
}

// ------------------------------------------------------------------------------------------------ Sobre

@Composable
private fun AboutTab() {
    Header(stringResource(R.string.tab_about))
    Text(stringResource(R.string.app_name) + " " + BuildConfig.VERSION_NAME, style = MaterialTheme.typography.titleMedium)
    Text(stringResource(R.string.about_text), style = MaterialTheme.typography.bodyMedium)
    Hint(stringResource(R.string.about_separate))
    Hint(stringResource(R.string.about_tech))
    Hint(stringResource(R.string.about_paths, AppPaths.appDir.path))
    Hint(stringResource(R.string.about_legal))
}
