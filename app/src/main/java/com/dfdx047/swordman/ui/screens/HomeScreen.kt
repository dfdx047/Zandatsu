package com.dfdx047.swordman.ui.screens

import android.content.Intent
import android.net.Uri
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
import com.dfdx047.swordman.R
import com.dfdx047.swordman.SwordManApp
import com.dfdx047.swordman.data.FileStatus
import com.dfdx047.swordman.data.GameOrientation
import com.dfdx047.swordman.ui.LauncherTab
import com.dfdx047.swordman.ui.LauncherViewModel
import com.dfdx047.swordman.ui.components.Level
import com.dfdx047.swordman.ui.components.Pill
import com.dfdx047.swordman.ui.components.SectionCard
import com.dfdx047.swordman.ui.components.SettingSegmented
import com.dfdx047.swordman.ui.components.StatusLine
import com.dfdx047.swordman.ui.components.SwordStage
import com.dfdx047.swordman.ui.components.color
import com.dfdx047.swordman.ui.theme.DfColors
import com.dfdx047.swordman.ui.theme.LogoStyle

@Composable
fun HomeScreen(vm: LauncherViewModel, goTo: (LauncherTab) -> Unit) {
    val context = LocalContext.current
    val pickFolder = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri: Uri? ->
        if (uri != null) vm.importGame(uri)
    }
    val pickIso = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri != null) vm.importIso(uri)
    }
    val install = vm.install
    var showFiles by remember { mutableStateOf(false) }

    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        // Palco: a lamina a cortar a pedra, o titulo, o estado e JOGAR.
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
                .height(260.dp)
                .clip(heroShape)
                .border(1.dp, DfColors.Royal, heroShape),
        ) {
            Column(Modifier.align(Alignment.TopStart).padding(20.dp)) {
                Text(
                    stringResource(R.string.game_title_small),
                    style = MaterialTheme.typography.labelMedium.copy(letterSpacing = 3.sp),
                    color = DfColors.Steel,
                )
                Text("REVENGEANCE", style = LogoStyle.copy(fontSize = 30.sp), color = DfColors.Cream)
                Box(Modifier.padding(top = 4.dp).width(56.dp).height(2.dp).background(DfColors.Gold))
            }
            Row(
                Modifier
                    .align(Alignment.BottomStart)
                    .fillMaxWidth()
                    .background(Brush.verticalGradient(listOf(Color.Transparent, DfColors.Night.copy(alpha = 0.9f))))
                    .padding(horizontal = 16.dp, vertical = 14.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                Box(Modifier.weight(1f)) {
                    Pill(statusText, if (level == Level.OK) DfColors.Lavender else level.color())
                }
                Button(
                    onClick = { vm.play(context) },
                    enabled = install?.playable == true && vm.busy == null,
                    modifier = Modifier.height(52.dp).widthIn(min = 168.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = DfColors.Gold, contentColor = Color.White),
                    shape = RoundedCornerShape(8.dp),
                ) {
                    Icon(Icons.Filled.PlayArrow, null)
                    Spacer(Modifier.width(8.dp))
                    Text(stringResource(R.string.play), style = MaterialTheme.typography.titleMedium.copy(letterSpacing = 2.sp))
                }
            }
        }

        SectionCard(
            stringResource(R.string.game_files_title),
            Icons.Filled.FolderOpen,
            subtitle = stringResource(R.string.game_files_subtitle),
        ) {
            Text(stringResource(R.string.game_files_help), style = MaterialTheme.typography.bodySmall)
            vm.importIssue?.let { (res, detail) ->
                StatusLine(stringResource(res, detail), Level.ERROR)
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilledTonalButton(
                    onClick = { pickIso.launch(arrayOf("application/x-iso9660-image", "application/octet-stream", "*/*")) },
                    enabled = vm.busy == null,
                ) {
                    Icon(Icons.Filled.Album, null)
                    Spacer(Modifier.width(6.dp))
                    Text(stringResource(R.string.import_iso))
                }
                OutlinedButton(onClick = { pickFolder.launch(null) }, enabled = vm.busy == null) {
                    Icon(Icons.Filled.FolderOpen, null)
                    Spacer(Modifier.width(6.dp))
                    Text(stringResource(R.string.import_folder))
                }
            }
            OutlinedButton(onClick = { vm.refreshInstall(true) }, enabled = install?.installed == true && vm.busy == null) {
                Icon(Icons.Filled.VerifiedUser, null)
                Spacer(Modifier.width(6.dp))
                Text(stringResource(R.string.verify))
            }
            if (install != null && install.installed) {
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
            val active = vm.activeDriver
            HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    if (active == null) stringResource(R.string.driver_current_system) else stringResource(R.string.driver_current, active),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.weight(1f),
                )
                TextButton(onClick = { goTo(LauncherTab.DRIVERS) }) { Text(stringResource(R.string.tab_drivers)) }
            }
        }

        // Redes no fundo da pagina: so os icones e o nome, discretos.
        Spacer(Modifier.height(4.dp))
        HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)
        Text(
            stringResource(R.string.community_sub),
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.align(Alignment.CenterHorizontally),
        )
        fun open(url: String) = context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
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
