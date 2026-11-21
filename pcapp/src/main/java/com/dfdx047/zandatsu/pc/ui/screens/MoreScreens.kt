package com.dfdx047.zandatsu.pc.ui.screens

import android.content.Intent
import android.net.Uri
import android.os.Build
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material.icons.filled.Code
import androidx.compose.material.icons.filled.Description
import androidx.compose.material.icons.filled.DeveloperBoard
import androidx.compose.material.icons.filled.Favorite
import androidx.compose.material.icons.filled.Palette
import androidx.compose.material.icons.filled.Save
import androidx.compose.material.icons.filled.SystemUpdate
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.scale
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.dfdx047.zandatsu.pc.BuildConfig
import com.dfdx047.zandatsu.pc.Links
import com.dfdx047.zandatsu.pc.PcGame
import com.dfdx047.zandatsu.pc.R
import com.dfdx047.zandatsu.pc.data.AppLanguage
import com.dfdx047.zandatsu.pc.data.AppPaths
import com.dfdx047.zandatsu.pc.data.DarkMode
import com.dfdx047.zandatsu.pc.data.LogExporter
import com.dfdx047.zandatsu.pc.data.ThemeMode
import com.dfdx047.zandatsu.pc.ui.LauncherTab
import com.dfdx047.zandatsu.pc.ui.LauncherViewModel
import com.dfdx047.zandatsu.pc.ui.UpdateState
import com.dfdx047.zandatsu.pc.ui.components.ButtonRow
import com.dfdx047.zandatsu.pc.ui.components.SectionCard
import com.dfdx047.zandatsu.pc.ui.components.SettingSegmented
import com.dfdx047.zandatsu.pc.ui.components.SettingSwitch
import com.dfdx047.zandatsu.pc.ui.theme.LogoStyle
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

// ------------------------------------------------------------------------------------------------------- Mais

/** "Mais" (como na app do 360): as paginas que se abrem pouco, cada uma com uma linha a dizer o que tem. */
@Composable
fun MoreScreen(goTo: (LauncherTab) -> Unit) {
    Column(
        Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        MoreEntry(LauncherTab.SETTINGS, stringResource(R.string.more_settings_sub), goTo)
        MoreEntry(LauncherTab.ABOUT, stringResource(R.string.more_about_sub), goTo)
    }
}

@Composable
private fun MoreEntry(tab: LauncherTab, subtitle: String, goTo: (LauncherTab) -> Unit) {
    Card(
        onClick = { goTo(tab) },
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(14.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant),
    ) {
        Row(Modifier.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
            Box(
                Modifier.size(40.dp).background(MaterialTheme.colorScheme.surfaceVariant, RoundedCornerShape(10.dp)),
                contentAlignment = Alignment.Center,
            ) { Icon(tab.icon, null, tint = MaterialTheme.colorScheme.primary) }
            Spacer(Modifier.width(14.dp))
            Column(Modifier.weight(1f)) {
                Text(stringResource(tab.title), style = MaterialTheme.typography.titleMedium)
                Text(subtitle, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Icon(Icons.AutoMirrored.Filled.KeyboardArrowRight, null, tint = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

// ---------------------------------------------------------------------------------------------------- Sistema

/** Sistema: a propria app (tema, idioma, atualizacoes), o processador do jogo e os logs. */
@Composable
fun SettingsScreen(vm: LauncherViewModel) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var logStatus by remember { mutableStateOf<String?>(null) }
    val saveLog = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/zip")) { uri ->
        if (uri != null) scope.launch {
            val r = withContext(Dispatchers.IO) { runCatching { LogExporter.writeTo(context, uri) } }
            logStatus = if (r.isSuccess) context.getString(R.string.logs_saved) else (r.exceptionOrNull()?.message ?: "?")
        }
    }
    Column(
        Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        SectionCard(stringResource(R.string.settings_app), Icons.Filled.Palette) {
            SettingSegmented(
                title = stringResource(R.string.settings_theme),
                subtitle = if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) stringResource(R.string.settings_theme_material_unavailable) else null,
                options = listOf(ThemeMode.GAME to "Revengeance", ThemeMode.MATERIAL_YOU to "Material You"),
                selected = vm.themeMode,
                onSelect = vm::setTheme,
            )
            SettingSegmented(
                title = stringResource(R.string.settings_dark),
                options = listOf(
                    DarkMode.SYSTEM to stringResource(R.string.dark_system),
                    DarkMode.LIGHT to stringResource(R.string.dark_light),
                    DarkMode.DARK to stringResource(R.string.dark_dark),
                ),
                selected = vm.darkMode,
                onSelect = vm::setDark,
            )
            SettingSegmented(
                title = stringResource(R.string.settings_language),
                subtitle = stringResource(R.string.settings_language_sub),
                options = listOf(
                    AppLanguage.SYSTEM to stringResource(R.string.sys_language_system),
                    AppLanguage.PORTUGUESE to "PT",
                    AppLanguage.ENGLISH to "EN",
                    AppLanguage.SPANISH to "ES",
                ),
                selected = vm.appLanguage,
                onSelect = vm::setLanguage,
            )
            SettingSwitch(stringResource(R.string.settings_check_updates), checked = vm.checkUpdatesOnStart,
                onChange = vm::changeCheckUpdatesOnStart)
        }

        SectionCard(stringResource(R.string.sys_cpu), Icons.Filled.DeveloperBoard) {
            var mt by remember { mutableStateOf(PcGame.multithread(context)) }
            SettingSwitch(stringResource(R.string.sys_mt), stringResource(R.string.sys_mt_sub), mt) {
                mt = it; PcGame.setMultithread(context, it)
            }
        }

        SectionCard(stringResource(R.string.settings_logs), Icons.Filled.Description, subtitle = stringResource(R.string.settings_logs_sub)) {
            FilledTonalButton(onClick = { saveLog.launch(LogExporter.suggestedName()) }) {
                Icon(Icons.Filled.Save, null)
                Spacer(Modifier.width(6.dp))
                Text(stringResource(R.string.err_dialog_save_log))
            }
            logStatus?.let { Text(it, style = MaterialTheme.typography.bodySmall) }
            Text(stringResource(R.string.about_paths, AppPaths.appDir.path), style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(stringResource(R.string.logs_discord_help), style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

// ------------------------------------------------------------------------------------------------------ Sobre

@Composable
fun AboutScreen(vm: LauncherViewModel) {
    val context = LocalContext.current
    fun open(url: String) = runCatching { context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url))) }

    Column(
        Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        // O icone adaptativo nao se desenha no Compose: o fundo e a camada da frente (miolo de 72 dp dos 108 dp).
        Box(
            Modifier
                .size(128.dp)
                .clip(RoundedCornerShape(28.dp))
                .background(colorResource(R.color.pc_icon_background)),
        ) {
            Icon(
                painterResource(R.drawable.ic_launcher_foreground),
                null,
                tint = Color.Unspecified,
                modifier = Modifier.fillMaxSize().scale(1.5f),
            )
        }
        Text("ZANDATSU PC", style = LogoStyle.copy(fontSize = 30.sp), color = MaterialTheme.colorScheme.onBackground)
        Text(stringResource(R.string.game_title), style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(stringResource(R.string.about_version, BuildConfig.VERSION_NAME, BuildConfig.VERSION_CODE), style = MaterialTheme.typography.bodyMedium)
        Text(stringResource(R.string.about_developer, Links.DEVELOPER), style = MaterialTheme.typography.titleMedium)

        ButtonRow(center = true) {
            Button(onClick = { open(Links.GITHUB_URL) }) {
                Icon(Icons.Filled.Code, null)
                Spacer(Modifier.width(6.dp))
                Text("GitHub")
            }
            OutlinedButton(onClick = { vm.checkUpdates(manual = true) }, enabled = vm.updateState != UpdateState.CHECKING) {
                if (vm.updateState == UpdateState.CHECKING) {
                    CircularProgressIndicator(Modifier.size(18.dp), strokeWidth = 2.dp)
                } else {
                    Icon(Icons.Filled.SystemUpdate, null)
                }
                Spacer(Modifier.width(6.dp))
                Text(stringResource(R.string.about_check_updates))
            }
        }
        ButtonRow(center = true) {
            OutlinedButton(onClick = { open(Links.DISCORD_URL) }) { Text("Discord") }
            OutlinedButton(onClick = { open(Links.YOUTUBE_URL) }) { Text("YouTube") }
            OutlinedButton(onClick = { open(Links.KOFI_URL) }) { Text("Ko-fi") }
        }
        if (vm.updateState == UpdateState.AVAILABLE) {
            TextButton(onClick = { vm.showUpdateDialog = true }) {
                Text(stringResource(R.string.about_update_available, vm.updateInfo?.version ?: ""))
            }
        }

        SectionCard(stringResource(R.string.about_project_title), Icons.Filled.Favorite, modifier = Modifier.fillMaxWidth()) {
            Text(stringResource(R.string.about_text), style = MaterialTheme.typography.bodySmall)
            Text(stringResource(R.string.about_separate), style = MaterialTheme.typography.bodySmall)
            Text(stringResource(R.string.about_tech), style = MaterialTheme.typography.bodySmall)
            Text(stringResource(R.string.about_credits), style = MaterialTheme.typography.bodySmall)
            Text(stringResource(R.string.about_legal), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}
