package com.dfdx047.zandatsu.pc.ui.screens

import android.content.Intent
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AspectRatio
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.Dashboard
import androidx.compose.material.icons.filled.PhotoSizeSelectLarge
import androidx.compose.material.icons.filled.Speed
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.dfdx047.zandatsu.pc.GraphicOptions
import com.dfdx047.zandatsu.pc.PcGame
import com.dfdx047.zandatsu.pc.R
import com.dfdx047.zandatsu.pc.ui.components.ChoiceRow
import com.dfdx047.zandatsu.pc.ui.components.Level
import com.dfdx047.zandatsu.pc.ui.components.SectionCard
import com.dfdx047.zandatsu.pc.ui.components.SettingSwitch
import com.dfdx047.zandatsu.pc.ui.components.StatusLine
import com.dfdx047.zandatsu.pc.ui.theme.DfColors
import java.io.File
import java.text.DateFormat
import java.util.Date
import kotlin.math.roundToInt

private fun formatTime(ms: Long): String = DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.MEDIUM).format(Date(ms))

@Composable
private fun Hint(text: String) =
    Text(text, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)

/**
 * Graficos do jogo de PC (o separador do pcapp, no visual da launcher do 360): perfis, resolucao interna, limite de
 * fps, FSR / CAS, geracao de quadros e as opcoes do proprio jogo (GraphicOption, gravadas e verificadas).
 */
@Composable
fun GraphicsScreen() {
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

    Column(
        Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        SectionCard(stringResource(R.string.gfx_profile), Icons.Filled.Dashboard) {
            val profiles = PcGame.PROFILES.map { it.id to stringResource(it.label) } + ("custom" to stringResource(R.string.profile_custom))
            ChoiceRow(profiles, profile) { id ->
                if (id == "custom") { custom(); return@ChoiceRow }
                lastWrite = PcGame.applyProfile(c, id) ?: lastWrite
                profile = id; scale = PcGame.renderScale(c); fps = PcGame.fpsCap(c); interp = PcGame.frameInterp(c)
                opts = GraphicOptions.values(c)
            }
            Hint(stringResource(R.string.gfx_recommended, stringResource(PcGame.PROFILES.first { it.id == PcGame.defaultProfile() }.label)))
        }

        SectionCard(stringResource(R.string.gfx_resolution), Icons.Filled.PhotoSizeSelectLarge) {
            val size = PcGame.renderSizeFor(nativeW, nativeH, scale).ifEmpty { "${nativeW}x$nativeH" }
            Text(stringResource(R.string.gfx_resolution_value, size, scale), style = MaterialTheme.typography.bodyLarge,
                fontWeight = FontWeight.Bold, color = MaterialTheme.colorScheme.primary)
            Slider(
                value = scale.toFloat(),
                onValueChange = { scale = ((it / PcGame.SCALE_STEP).roundToInt() * PcGame.SCALE_STEP).coerceIn(PcGame.SCALE_MIN, 100) },
                onValueChangeFinished = { PcGame.setRenderScale(c, scale); custom() },
                valueRange = PcGame.SCALE_MIN.toFloat()..100f,
                steps = (100 - PcGame.SCALE_MIN) / PcGame.SCALE_STEP - 1,
            )
            Hint(stringResource(R.string.gfx_resolution_hint, "${nativeW}x$nativeH"))
        }

        SectionCard(stringResource(R.string.gfx_fps), Icons.Filled.Speed) {
            ChoiceRow(PcGame.FPS_CAPS.map { it to it.toString() }, fps) { v ->
                fps = v; PcGame.setFpsCap(c, v); custom()
                // A geracao de quadros so serve com o jogo a 30 fps.
                if (v != 30 && interp) { interp = false; PcGame.setFrameInterp(c, false) }
            }
            Hint(stringResource(R.string.gfx_fps_hint))
        }

        SectionCard(stringResource(R.string.gfx_upscale), Icons.Filled.AspectRatio) {
            ChoiceRow(listOf(0 to stringResource(R.string.gfx_upscale_bilinear), 1 to stringResource(R.string.gfx_upscale_fsr1),
                2 to stringResource(R.string.gfx_upscale_cas)), fsr) { v -> fsr = v; PcGame.setFsrMode(c, v); custom() }
            Hint(stringResource(when (fsr) { 1 -> R.string.gfx_upscale_fsr1_sub; 2 -> R.string.gfx_upscale_cas_sub; else -> R.string.gfx_upscale_bilinear_sub }))
            if (fsr != 0) {
                Text(stringResource(R.string.gfx_sharpness, (sharp * 100).roundToInt()), style = MaterialTheme.typography.labelLarge)
                Slider(sharp, { sharp = (it * 20).roundToInt() / 20f }, valueRange = 0f..1f, steps = 19,
                    onValueChangeFinished = { PcGame.setFsrSharpness(c, sharp); custom() })
                Text(stringResource(R.string.gfx_upscale_output), style = MaterialTheme.typography.labelLarge)
                ChoiceRow(listOf(0 to stringResource(R.string.gfx_output_screen), 720 to stringResource(R.string.gfx_output_720)), fsrOut) { v ->
                    fsrOut = v; PcGame.setFsrOut(c, v); custom()
                }
            }
        }

        SectionCard(stringResource(R.string.gfx_framegen), Icons.Filled.AutoAwesome) {
            SettingSwitch(stringResource(R.string.gfx_framegen_switch), null, interp) { on ->
                interp = on; PcGame.setFrameInterp(c, on); custom()
                if (on && fps != 30) { fps = 30; PcGame.setFpsCap(c, 30) }
            }
            Hint(stringResource(R.string.gfx_framegen_sub))
            StatusLine(stringResource(R.string.gfx_framegen_warning), if (interp) Level.INFO else Level.WARN)
        }

        // Opcoes do proprio jogo (GraphicOption).
        SectionCard(stringResource(R.string.go_title), Icons.Filled.Tune, subtitle = stringResource(R.string.go_sub)) {
            fun set(i: Int, x: Int) {
                opts = opts.copyOf().also { it[i] = x }
                lastWrite = GraphicOptions.save(c, opts)
                custom()
            }
            @Composable fun row(label: Int, i: Int, options: List<Pair<Int, String>>) {
                Text(stringResource(label), style = MaterialTheme.typography.bodyLarge)
                ChoiceRow(options, opts[i]) { set(i, it) }
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
            Text(stringResource(R.string.go_brightness) + ": " + opts[4], style = MaterialTheme.typography.bodyLarge)
            var bright by remember(opts[4]) { mutableFloatStateOf(opts[4].toFloat()) }
            Slider(bright, { bright = it.roundToInt().toFloat() }, valueRange = 0f..10f, steps = 9,
                onValueChangeFinished = { set(4, bright.roundToInt()) })

            val exe = PcGame.exePath(c)
            Hint(stringResource(R.string.go_file, if (exe.isNotEmpty()) GraphicOptions.file(exe).path else "—"))
            val lw = lastWrite
            when {
                lw == null -> Hint(stringResource(R.string.go_never_written))
                lw.ok -> Text(stringResource(R.string.go_written_at, formatTime(lw.at), lw.via.uppercase()),
                    style = MaterialTheme.typography.bodySmall, color = DfColors.Ok)
                else -> StatusLine(stringResource(R.string.go_write_failed, lw.path, lw.error ?: "?") + "\n" + formatTime(lw.at), Level.ERROR)
            }
            if (lw != null && !lw.ok) SafAccess()
        }
        Hint(stringResource(R.string.gfx_renderer))
    }
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
