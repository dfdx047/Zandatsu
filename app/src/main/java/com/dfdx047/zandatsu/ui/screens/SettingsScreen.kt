package com.dfdx047.zandatsu.ui.screens

import android.os.Build
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Description
import androidx.compose.material.icons.filled.Download
import androidx.compose.material.icons.filled.Palette
import androidx.compose.material.icons.filled.Save
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.dfdx047.zandatsu.R
import com.dfdx047.zandatsu.data.AppLanguage
import com.dfdx047.zandatsu.data.DarkMode
import com.dfdx047.zandatsu.data.LogExporter
import com.dfdx047.zandatsu.data.ThemeMode
import com.dfdx047.zandatsu.ui.LauncherViewModel
import com.dfdx047.zandatsu.ui.components.SectionCard
import com.dfdx047.zandatsu.ui.components.SettingSegmented
import com.dfdx047.zandatsu.ui.components.SettingSwitch

/** Aba Sistema: o proprio aplicativo (tema, idioma, atualizacoes) e os logs. */
@Composable
fun SettingsScreen(vm: LauncherViewModel) {
    val context = LocalContext.current
    val saveLog = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/zip")) { uri ->
        if (uri != null) vm.saveLogTo(context, uri)
    }

    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        SectionCard(stringResource(R.string.settings_app), Icons.Filled.Palette) {
            SettingSegmented(
                title = stringResource(R.string.settings_theme),
                subtitle = if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) stringResource(R.string.settings_theme_material_unavailable) else null,
                options = listOf(
                    ThemeMode.GAME to "Revengeance",
                    ThemeMode.MATERIAL_YOU to "Material You",
                ),
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
                    AppLanguage.SYSTEM to stringResource(R.string.lang_system),
                    AppLanguage.PORTUGUESE to "PT",
                    AppLanguage.ENGLISH to "EN",
                    AppLanguage.SPANISH to "ES",
                ),
                selected = vm.appLanguage,
                onSelect = vm::setLanguage,
            )
            SettingSwitch(
                stringResource(R.string.settings_check_updates),
                checked = vm.checkUpdatesOnStart,
                onChange = vm::setCheckUpdatesOnStart,
            )
        }

        SectionCard(
            stringResource(R.string.settings_logs),
            Icons.Filled.Description,
            subtitle = stringResource(R.string.settings_logs_sub),
        ) {
            Row {
                FilledTonalButton(onClick = { vm.exportLogs(context) }, enabled = vm.busy == null) {
                    Icon(Icons.Filled.Download, null)
                    Spacer(Modifier.width(6.dp))
                    Text(stringResource(R.string.export_logs))
                }
                Spacer(Modifier.width(8.dp))
                OutlinedButton(onClick = { saveLog.launch(LogExporter.suggestedName()) }, enabled = vm.busy == null) {
                    Icon(Icons.Filled.Save, null)
                    Spacer(Modifier.width(6.dp))
                    Text(stringResource(R.string.err_dialog_save_log))
                }
            }
            Text(
                stringResource(R.string.err_dialog_discord_help),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}
