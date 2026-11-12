package com.dfdx047.zandatsu.ui.screens

import android.content.Intent
import android.net.Uri
import android.provider.OpenableColumns
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.material.icons.filled.Tune
import androidx.compose.ui.text.style.TextOverflow
import com.dfdx047.zandatsu.data.FrameMode
import com.dfdx047.zandatsu.data.IsoSource
import com.dfdx047.zandatsu.data.RenutPreset
import com.dfdx047.zandatsu.data.ScreenInfo
import com.dfdx047.zandatsu.data.frameMode
import com.dfdx047.zandatsu.ui.components.ButtonRow
import com.dfdx047.zandatsu.ui.components.SettingSwitch
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AddToHomeScreen
import androidx.compose.material.icons.filled.Album
import androidx.compose.material.icons.filled.Code
import androidx.compose.material.icons.filled.DeleteSweep
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.Forum
import androidx.compose.material.icons.filled.LocalCafe
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.ScreenRotation
import androidx.compose.material.icons.filled.SmartDisplay
import androidx.compose.material.icons.filled.VerifiedUser
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.dfdx047.zandatsu.R
import com.dfdx047.zandatsu.SwordManApp
import com.dfdx047.zandatsu.data.FileStatus
import com.dfdx047.zandatsu.data.GameOrientation
import com.dfdx047.zandatsu.ui.LauncherTab
import com.dfdx047.zandatsu.ui.LauncherViewModel
import com.dfdx047.zandatsu.ui.components.Level
import com.dfdx047.zandatsu.ui.components.Pill
import com.dfdx047.zandatsu.ui.components.SectionCard
import com.dfdx047.zandatsu.ui.components.SettingSegmented
import com.dfdx047.zandatsu.ui.components.StatusLine
import com.dfdx047.zandatsu.ui.components.SwordStage
import com.dfdx047.zandatsu.ui.components.color
import com.dfdx047.zandatsu.ui.theme.DfColors
import com.dfdx047.zandatsu.ui.theme.LogoStyle

/**
 * Inicio (12/11, refeito): o palco com o estado e JOGAR, um resumo da configuracao (cada item leva a sua pagina), os
 * arquivos do jogo (compactos depois de importar) e as opcoes de arranque (com o "Pular as logos", antes numa aba so
 * para ele). Em paisagem larga, duas colunas.
 */
@Composable
fun HomeScreen(vm: LauncherViewModel, goTo: (LauncherTab) -> Unit) {
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val wide = maxWidth >= 700.dp
        Column(
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp),
        ) {
            // 26/10: Adreno com o driver do sistema = modo emulado; aviso grande antes do JOGAR.
            com.dfdx047.zandatsu.ui.components.TurnipWarning(vm) { goTo(LauncherTab.DRIVERS) }
            if (wide) {
                Row(horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                    Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                        Hero(vm, wide = true)
                        SetupSummary(vm, goTo)
                    }
                    Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                        GameFilesCard(vm)
                        LaunchOptionsCard(vm)
                    }
                }
            } else {
                Hero(vm, wide = false)
                if (vm.install?.playable == true) SetupSummary(vm, goTo)
                GameFilesCard(vm)
                LaunchOptionsCard(vm)
            }
            CommunityLinks()
        }
    }
}

/** O palco: a lamina a cortar a pedra, o titulo, o estado (por baixo do titulo, inteiro) e JOGAR. */
@Composable
private fun Hero(vm: LauncherViewModel, wide: Boolean) {
    val context = LocalContext.current
    val install = vm.install
    val (statusText, level) = when {
        install == null -> stringResource(R.string.status_checking) to Level.INFO
        install.xex?.status == FileStatus.DIFFERENT -> stringResource(R.string.status_wrong_region) to Level.ERROR
        install.playable -> stringResource(R.string.status_ready) to Level.OK
        install.installed -> stringResource(R.string.status_incomplete) to Level.WARN
        else -> stringResource(R.string.status_not_installed) to Level.WARN
    }
    val heroShape = RoundedCornerShape(16.dp)
    SwordStage(
        Modifier
            .fillMaxWidth()
            .height(if (wide) 236.dp else 250.dp)
            .clip(heroShape)
            .border(1.dp, DfColors.Royal, heroShape),
    ) {
        Column(Modifier.align(Alignment.TopStart).padding(20.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(
                stringResource(R.string.game_title_small),
                style = MaterialTheme.typography.labelMedium.copy(letterSpacing = 3.sp),
                color = DfColors.Steel,
            )
            Text("REVENGEANCE", style = LogoStyle.copy(fontSize = 30.sp), color = DfColors.Cream, maxLines = 1, softWrap = false)
            Box(Modifier.width(56.dp).height(2.dp).background(DfColors.Gold))
            Spacer(Modifier.height(6.dp))
            Pill(statusText, if (level == Level.OK) DfColors.Lavender else level.color())
        }
        Box(
            Modifier
                .align(Alignment.BottomStart)
                .fillMaxWidth()
                .background(Brush.verticalGradient(listOf(Color.Transparent, DfColors.Night.copy(alpha = 0.9f))))
                .padding(horizontal = 16.dp, vertical = 14.dp),
            contentAlignment = Alignment.CenterEnd,
        ) {
            Button(
                onClick = { vm.play(context) },
                enabled = install?.playable == true && vm.busy == null,
                modifier = Modifier.height(54.dp).then(if (wide) Modifier.widthIn(min = 200.dp) else Modifier.fillMaxWidth()),
                colors = ButtonDefaults.buttonColors(containerColor = DfColors.Gold, contentColor = Color.White),
                shape = RoundedCornerShape(10.dp),
            ) {
                Icon(Icons.Filled.PlayArrow, null)
                Spacer(Modifier.width(8.dp))
                Text(stringResource(R.string.play), style = MaterialTheme.typography.titleMedium.copy(letterSpacing = 2.sp))
            }
        }
    }
}

/** Resumo do que o jogo vai usar: perfil, renderizador, resolucao, FPS, imagem e driver. Cada item abre a sua pagina. */
@Composable
private fun SetupSummary(vm: LauncherViewModel, goTo: (LauncherTab) -> Unit) {
    val c = vm.config
    val profile = when (c.renutPreset) {
        RenutPreset.MINIMUM.name -> stringResource(R.string.preset_minimum)
        RenutPreset.LOW.name -> stringResource(R.string.renut_preset_low)
        RenutPreset.MEDIUM.name -> stringResource(R.string.renut_preset_medium)
        RenutPreset.HIGH.name -> stringResource(R.string.renut_preset_high)
        RenutPreset.ULTRA.name -> stringResource(R.string.renut_preset_ultra)
        else -> stringResource(R.string.sum_custom)
    }
    val resolution = if (c.resolutionScreen) {
        stringResource(R.string.sum_screen_res, ScreenInfo.matchedResolution().label.substringBefore(' '))
    } else {
        c.resolution.label.substringBefore(' ')
    }
    val fps = when (c.frameMode) {
        FrameMode.FPS30 -> "30"
        FrameMode.FPS60 -> "60"
        FrameMode.FPS120_INTERP -> "120"
    }
    val ratio = ScreenInfo.ratioLabel()
    val image = when (c.effectiveAspect) {
        "widescreen" -> stringResource(R.string.aspect_widescreen, ratio).substringBefore(' ')  // "Ultrawide": cabe no item
        "stretch" -> stringResource(R.string.aspect_stretch)
        else -> stringResource(R.string.aspect_original)
    }
    val renderer = if (c.useRenut) "Renut" else "Xenia"
    val driver = vm.activeDriver ?: stringResource(R.string.sum_system)
    val items = listOf(
        Triple(R.string.sum_profile, profile, LauncherTab.PERFORMANCE),
        Triple(R.string.sum_renderer, renderer, LauncherTab.PERFORMANCE),
        Triple(R.string.sum_resolution, resolution, LauncherTab.PERFORMANCE),
        Triple(R.string.sum_fps, fps, LauncherTab.PERFORMANCE),
        Triple(R.string.sum_image, image, LauncherTab.PERFORMANCE),
        Triple(R.string.sum_driver, driver, LauncherTab.DRIVERS),
    )
    SectionCard(stringResource(R.string.home_setup_title), Icons.Filled.Tune, subtitle = stringResource(R.string.home_setup_sub)) {
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            val cols = if (maxWidth < 330.dp) 2 else 3
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                items.chunked(cols).forEach { row ->
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        row.forEach { (label, value, dest) ->
                            SummaryTile(stringResource(label), value, Modifier.weight(1f)) { goTo(dest) }
                        }
                        repeat(cols - row.size) { Spacer(Modifier.weight(1f)) }
                    }
                }
            }
        }
    }
}

@Composable
private fun SummaryTile(label: String, value: String, modifier: Modifier, onClick: () -> Unit) {
    Column(
        modifier
            .clip(RoundedCornerShape(10.dp))
            .background(MaterialTheme.colorScheme.surfaceContainerHigh)
            .clickable(onClick = onClick)
            .padding(horizontal = 10.dp, vertical = 8.dp),
    ) {
        Text(label, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1)
        Text(
            value,
            style = MaterialTheme.typography.titleSmall,
            color = MaterialTheme.colorScheme.onSurface,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis,
        )
    }
}

/**
 * Arquivos do jogo. Sem jogo: a explicacao e os dois botoes de importar. Com o jogo: de onde ele e lido, e verificar /
 * trocar numa linha que passa para baixo quando nao cabe.
 */
@Composable
private fun GameFilesCard(vm: LauncherViewModel) {
    val context = LocalContext.current
    val pickFolder = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri: Uri? ->
        if (uri != null) vm.importGame(uri)
    }
    val pickIso = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri != null) vm.importIso(uri)
    }
    val install = vm.install
    val installed = install?.installed == true
    var showFiles by remember { mutableStateOf(false) }
    val isoName = remember(install) {
        IsoSource.current()?.first?.let { uri ->
            runCatching {
                context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
                    if (c.moveToFirst()) c.getString(0) else null
                }
            }.getOrNull() ?: Uri.decode(uri.lastPathSegment ?: "").substringAfterLast('/')
        }
    }
    val isoMimes = arrayOf("application/x-iso9660-image", "application/octet-stream", "*/*")
    SectionCard(
        stringResource(R.string.game_files_title),
        Icons.Filled.FolderOpen,
        subtitle = stringResource(R.string.game_files_subtitle),
    ) {
        if (!installed) {
            Text(stringResource(R.string.game_files_help), style = MaterialTheme.typography.bodySmall)
        } else {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(
                    if (isoName != null) Icons.Filled.Album else Icons.Filled.FolderOpen,
                    null,
                    tint = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.size(18.dp),
                )
                Spacer(Modifier.width(8.dp))
                Text(
                    if (isoName != null) stringResource(R.string.home_iso_in_use, isoName) else stringResource(R.string.home_folder_in_use),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        vm.importIssue?.let { (res, detail) ->
            StatusLine(stringResource(res, detail), Level.ERROR)
        }
        ButtonRow {
            if (!installed) {
                FilledTonalButton(onClick = { pickIso.launch(isoMimes) }, enabled = vm.busy == null) {
                    Icon(Icons.Filled.Album, null)
                    Spacer(Modifier.width(6.dp))
                    Text(stringResource(R.string.import_iso), maxLines = 1, softWrap = false)
                }
            } else {
                FilledTonalButton(onClick = { vm.refreshInstall(true) }, enabled = vm.busy == null) {
                    Icon(Icons.Filled.VerifiedUser, null)
                    Spacer(Modifier.width(6.dp))
                    Text(stringResource(R.string.verify), maxLines = 1, softWrap = false)
                }
                OutlinedButton(onClick = { pickIso.launch(isoMimes) }, enabled = vm.busy == null) {
                    Icon(Icons.Filled.Album, null)
                    Spacer(Modifier.width(6.dp))
                    Text(stringResource(R.string.home_change_iso), maxLines = 1, softWrap = false)
                }
            }
            OutlinedButton(onClick = { pickFolder.launch(null) }, enabled = vm.busy == null) {
                Icon(Icons.Filled.FolderOpen, null)
                Spacer(Modifier.width(6.dp))
                Text(stringResource(R.string.import_folder), maxLines = 1, softWrap = false)
            }
        }
        // ISO ativa e sobras de uma extracao antiga: o jogo ja nao as le.
        if (vm.staleBytes > 0) {
            OutlinedButton(onClick = { vm.deleteStaleFiles() }, enabled = vm.busy == null) {
                Icon(Icons.Filled.DeleteSweep, null)
                Spacer(Modifier.width(6.dp))
                Text(stringResource(R.string.delete_old_extracted, "%.1f GB".format(vm.staleBytes / 1e9)))
            }
        }
        if (install != null && installed) {
            TextButton(onClick = { showFiles = !showFiles }) {
                Text(stringResource(if (showFiles) R.string.hide_files else R.string.show_files))
            }
            if (showFiles) {
                install.checks.forEach { c ->
                    val (label, lvl) = when (c.status) {
                        FileStatus.OK -> stringResource(R.string.file_ok) to Level.OK
                        FileStatus.MISSING -> stringResource(if (c.required) R.string.file_missing_required else R.string.file_missing_optional) to
                            (if (c.required) Level.ERROR else Level.INFO)
                        FileStatus.DIFFERENT -> stringResource(R.string.file_wrong_region) to Level.ERROR
                        FileStatus.WRONG_SIZE -> stringResource(R.string.file_wrong_size) to Level.ERROR
                    }
                    StatusLine("${c.name} — $label", lvl)
                }
            }
        }
    }
}

/** Como o jogo abre: orientacao, pular as logos (antes na aba Cheats) e o atalho na tela inicial. */
@Composable
private fun LaunchOptionsCard(vm: LauncherViewModel) {
    val context = LocalContext.current
    SectionCard(stringResource(R.string.launch_options_title), Icons.Filled.ScreenRotation) {
        SettingSegmented(
            title = stringResource(R.string.orientation),
            options = listOf(
                GameOrientation.LANDSCAPE to stringResource(R.string.orientation_landscape),
                GameOrientation.PORTRAIT to stringResource(R.string.orientation_portrait),
                GameOrientation.AUTO to stringResource(R.string.orientation_auto),
            ),
            selected = vm.gameOrientation,
            onSelect = vm::setOrientation,
        )
        SettingSwitch(
            title = stringResource(R.string.cheats_skip_logos),
            subtitle = stringResource(R.string.cheats_skip_logos_sub),
            checked = vm.skipLogos,
        ) { vm.changeSkipLogos(it) }
        OutlinedButton(onClick = { vm.createShortcut(context) }, modifier = Modifier.fillMaxWidth()) {
            Icon(Icons.Filled.AddToHomeScreen, null)
            Spacer(Modifier.width(6.dp))
            Text(stringResource(R.string.create_shortcut))
        }
        Text(
            stringResource(R.string.create_shortcut_help),
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

/** Redes no fundo da pagina: so os icones e o nome, discretos. */
@Composable
private fun CommunityLinks() {
    val context = LocalContext.current
    fun open(url: String) = context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)
        Text(
            stringResource(R.string.community_sub),
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.align(Alignment.CenterHorizontally),
        )
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
            LinkButton("GitHub", Icons.Filled.Code, DfColors.Cream, Modifier.weight(1f)) { open(SwordManApp.GITHUB_URL) }
            LinkButton("YouTube", Icons.Filled.SmartDisplay, Color(0xFFE62117), Modifier.weight(1f)) { open(SwordManApp.YOUTUBE_URL) }
            LinkButton("Ko-fi", Icons.Filled.LocalCafe, Color(0xFF13A8D8), Modifier.weight(1f)) { open(SwordManApp.KOFI_URL) }
            LinkButton("Discord", Icons.Filled.Forum, Color(0xFF5865F2), Modifier.weight(1f)) { open(SwordManApp.DISCORD_URL) }
        }
    }
}

/** Botao de um link da comunidade: icone na cor da marca, nome por baixo. */
@Composable
private fun LinkButton(label: String, icon: ImageVector, accent: Color, modifier: Modifier = Modifier, onClick: () -> Unit) {
    OutlinedButton(
        onClick = onClick,
        modifier = modifier.height(60.dp),
        shape = RoundedCornerShape(10.dp),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant),
        contentPadding = androidx.compose.foundation.layout.PaddingValues(4.dp),
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Icon(icon, null, tint = accent, modifier = Modifier.size(22.dp))
            Text(label, maxLines = 1, style = MaterialTheme.typography.labelSmall)
        }
    }
}
