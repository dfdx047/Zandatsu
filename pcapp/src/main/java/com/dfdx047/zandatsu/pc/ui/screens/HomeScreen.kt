package com.dfdx047.zandatsu.pc.ui.screens

import android.content.Intent
import android.net.Uri
import android.provider.Settings
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
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
import androidx.compose.material.icons.filled.Code
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.Forum
import androidx.compose.material.icons.filled.LocalCafe
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.ScreenRotation
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.SmartDisplay
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material.icons.filled.VerifiedUser
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
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
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.dfdx047.zandatsu.pc.Links
import com.dfdx047.zandatsu.pc.PcGame
import com.dfdx047.zandatsu.pc.R
import com.dfdx047.zandatsu.pc.data.GameOrientation
import com.dfdx047.zandatsu.pc.data.LauncherPrefs
import com.dfdx047.zandatsu.pc.data.PcTouchMode
import com.dfdx047.zandatsu.pc.drivers.DriverRepository
import com.dfdx047.zandatsu.pc.ui.ExeCheck
import com.dfdx047.zandatsu.pc.ui.LauncherTab
import com.dfdx047.zandatsu.pc.ui.LauncherViewModel
import com.dfdx047.zandatsu.pc.ui.components.ButtonRow
import com.dfdx047.zandatsu.pc.ui.components.Level
import com.dfdx047.zandatsu.pc.ui.components.Pill
import com.dfdx047.zandatsu.pc.ui.components.SectionCard
import com.dfdx047.zandatsu.pc.ui.components.SettingSegmented
import com.dfdx047.zandatsu.pc.ui.components.StatusLine
import com.dfdx047.zandatsu.pc.ui.components.SwordStage
import com.dfdx047.zandatsu.pc.ui.components.color
import com.dfdx047.zandatsu.pc.ui.theme.DfColors
import com.dfdx047.zandatsu.pc.ui.theme.LogoStyle

/**
 * Inicio (copia do da app do 360): o palco animado com o estado e JOGAR no topo, o resumo da configuracao (cada item
 * leva a sua pagina), a pasta do jogo de PC, as opcoes de arranque (orientacao, atalho) e as redes. Paisagem larga:
 * duas colunas.
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
            if (wide) {
                Row(horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                    Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                        Hero(vm, wide = true)
                        SetupSummary(goTo)
                    }
                    Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                        GameFilesCard(vm)
                        LaunchOptionsCard(vm)
                    }
                }
            } else {
                Hero(vm, wide = false)
                if (vm.canPlay) SetupSummary(goTo)
                GameFilesCard(vm)
                LaunchOptionsCard(vm)
            }
            CommunityLinks()
        }
    }
}

/** O palco: a lamina a cortar a pedra, o titulo, o estado e JOGAR. */
@Composable
private fun Hero(vm: LauncherViewModel, wide: Boolean) {
    val context = LocalContext.current
    val (statusText, level) = when {
        !vm.packaged -> stringResource(R.string.home_status_no_code) to Level.ERROR
        !vm.access -> stringResource(R.string.home_status_no_access) to Level.WARN
        !vm.exeFound -> stringResource(R.string.home_status_no_exe) to Level.WARN
        vm.check == ExeCheck.WRONG -> stringResource(R.string.home_status_wrong_exe) to Level.ERROR
        vm.check == ExeCheck.CHECKING -> stringResource(R.string.status_checking) to Level.INFO
        else -> stringResource(R.string.status_ready) to Level.OK
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
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.width(56.dp).height(2.dp).background(DfColors.Gold))
                Spacer(Modifier.width(8.dp))
                Text("PC", style = MaterialTheme.typography.labelMedium.copy(letterSpacing = 3.sp), color = DfColors.Gold)
            }
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
                enabled = vm.canPlay,
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

/** Resumo do que o jogo vai usar: perfil, resolucao, FPS, escala, driver e toque. Cada item abre a sua pagina. */
@Composable
private fun SetupSummary(goTo: (LauncherTab) -> Unit) {
    val c = LocalContext.current
    val profileId = PcGame.profile(c)
    val profile = PcGame.PROFILES.firstOrNull { it.id == profileId }?.let { stringResource(it.label) }
        ?: stringResource(R.string.profile_custom)
    val (w, h) = remember { PcGame.nativeSize(c) }
    val resolution = PcGame.renderSizeFor(w, h, PcGame.renderScale(c)).ifEmpty { "${w}x$h" }
    val fps = PcGame.fpsCap(c).toString() + if (PcGame.frameInterp(c)) " → 60" else ""
    val upscale = when (PcGame.fsrMode(c)) {
        1 -> stringResource(R.string.gfx_upscale_fsr1)
        2 -> stringResource(R.string.gfx_upscale_cas)
        else -> stringResource(R.string.gfx_upscale_bilinear)
    }
    val drv = PcGame.driverDir(c)
    val driver = remember(drv) { DriverRepository.installed().firstOrNull { it.dir.absolutePath == drv }?.name }
        ?: stringResource(R.string.sys_driver_system)
    val touch = stringResource(
        when (LauncherPrefs(c).pcTouchMode) {
            PcTouchMode.BUTTONS -> R.string.ctl_mode_buttons
            PcTouchMode.GESTURES -> R.string.ctl_mode_gestures
            PcTouchMode.MOUSE -> R.string.ctl_mode_mouse
        },
    )
    val items = listOf(
        Triple(R.string.gfx_profile, profile, LauncherTab.GRAPHICS),
        Triple(R.string.sum_resolution, resolution, LauncherTab.GRAPHICS),
        Triple(R.string.sum_fps, fps, LauncherTab.GRAPHICS),
        Triple(R.string.gfx_upscale, upscale, LauncherTab.GRAPHICS),
        Triple(R.string.sum_driver, driver, LauncherTab.DRIVERS),
        Triple(R.string.ctl_mode, touch, LauncherTab.CONTROLS),
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
        Text(value, style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.onSurface, maxLines = 2, overflow = TextOverflow.Ellipsis)
    }
}

/** A pasta do jogo de PC: acesso aos ficheiros, o executavel e a verificacao da versao. */
@Composable
private fun GameFilesCard(vm: LauncherViewModel) {
    val c = LocalContext.current
    var showStatus by remember { mutableStateOf(false) }
    SectionCard(stringResource(R.string.game_folder), Icons.Filled.FolderOpen, subtitle = stringResource(R.string.game_intro_short)) {
        if (!vm.access) {
            StatusLine(stringResource(R.string.game_need_access), Level.WARN)
            FilledTonalButton(onClick = {
                c.startActivity(Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:${c.packageName}")))
            }) {
                Icon(Icons.Filled.VerifiedUser, null)
                Spacer(Modifier.width(6.dp))
                Text(stringResource(R.string.game_grant_access))
            }
        }
        if (!vm.packaged) StatusLine(stringResource(R.string.home_status_no_code_detail), Level.ERROR)
        OutlinedTextField(
            vm.exe, { vm.setExePath(it) }, Modifier.fillMaxWidth(),
            label = { Text(stringResource(R.string.game_exe_label)) }, singleLine = true,
        )
        ButtonRow {
            OutlinedButton(onClick = { vm.searchExe() }) {
                Icon(Icons.Filled.Search, null)
                Spacer(Modifier.width(6.dp))
                Text(stringResource(R.string.game_search), maxLines = 1, softWrap = false)
            }
        }
        Text(
            stringResource(R.string.game_search_hint, PcGame.EXE_NAME),
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        when (vm.check) {
            ExeCheck.WRONG -> StatusLine(stringResource(R.string.status_unsupported_detail, vm.checkDetail), Level.ERROR)
            ExeCheck.ERROR -> StatusLine(vm.checkDetail, Level.ERROR)
            else -> {}
        }
        androidx.compose.material3.TextButton(onClick = { showStatus = !showStatus }) {
            Text(stringResource(if (showStatus) R.string.hide_details else R.string.show_details))
        }
        if (showStatus) {
            StatusLine(stringResource(R.string.status_packaged) + ": " + stringResource(if (vm.packaged) R.string.yes else R.string.status_missing),
                if (vm.packaged) Level.OK else Level.ERROR)
            StatusLine(stringResource(R.string.status_access) + ": " + stringResource(if (vm.access) R.string.yes else R.string.no),
                if (vm.access) Level.OK else Level.WARN)
            StatusLine(stringResource(R.string.status_exe) + ": " + stringResource(if (vm.exeFound) R.string.status_found else R.string.status_not_found),
                if (vm.exeFound) Level.OK else Level.WARN)
            StatusLine(
                stringResource(R.string.status_version) + ": " + when (vm.check) {
                    ExeCheck.NONE -> "—"
                    ExeCheck.CHECKING -> stringResource(R.string.status_checking)
                    ExeCheck.OK -> stringResource(R.string.status_supported)
                    ExeCheck.WRONG -> stringResource(R.string.status_unsupported)
                    ExeCheck.ERROR -> stringResource(R.string.status_error)
                },
                when (vm.check) { ExeCheck.OK -> Level.OK; ExeCheck.WRONG, ExeCheck.ERROR -> Level.ERROR; else -> Level.INFO },
            )
            Text(stringResource(R.string.play_hint), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

/** Como o jogo abre: orientacao e o atalho na tela inicial. */
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
        if (vm.gameOrientation != GameOrientation.LANDSCAPE) {
            Text(stringResource(R.string.orientation_portrait_note), style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        OutlinedButton(onClick = { vm.createShortcut(context) }, modifier = Modifier.fillMaxWidth()) {
            Icon(Icons.Filled.AddToHomeScreen, null)
            Spacer(Modifier.width(6.dp))
            Text(stringResource(R.string.create_shortcut))
        }
        Text(stringResource(R.string.create_shortcut_help), style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}

/** Redes no fundo da pagina: so os icones e o nome, discretos (as mesmas da app do 360). */
@Composable
private fun CommunityLinks() {
    val context = LocalContext.current
    fun open(url: String) = runCatching { context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url))) }
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)
        Text(
            stringResource(R.string.community_sub),
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.align(Alignment.CenterHorizontally),
        )
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
            LinkButton("GitHub", Icons.Filled.Code, DfColors.Cream, Modifier.weight(1f)) { open(Links.GITHUB_URL) }
            LinkButton("YouTube", Icons.Filled.SmartDisplay, Color(0xFFE62117), Modifier.weight(1f)) { open(Links.YOUTUBE_URL) }
            LinkButton("Ko-fi", Icons.Filled.LocalCafe, Color(0xFF13A8D8), Modifier.weight(1f)) { open(Links.KOFI_URL) }
            LinkButton("Discord", Icons.Filled.Forum, Color(0xFF5865F2), Modifier.weight(1f)) { open(Links.DISCORD_URL) }
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
        contentPadding = PaddingValues(4.dp),
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Icon(icon, null, tint = accent, modifier = Modifier.size(22.dp))
            Text(label, maxLines = 1, style = MaterialTheme.typography.labelSmall)
        }
    }
}
