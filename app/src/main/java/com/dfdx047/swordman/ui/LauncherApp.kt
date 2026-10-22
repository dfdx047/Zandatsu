@file:OptIn(ExperimentalMaterial3Api::class)

package com.dfdx047.swordman.ui

import androidx.annotation.StringRes
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.BugReport
import androidx.compose.material.icons.filled.ErrorOutline
import androidx.compose.material.icons.filled.Save
import androidx.compose.material.icons.filled.FastForward
import androidx.compose.material.icons.filled.Gamepad
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Speed
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.ui.text.font.FontFamily
import com.dfdx047.swordman.SwordManApp
import com.dfdx047.swordman.data.CrashReport
import com.dfdx047.swordman.data.ErrorKind
import com.dfdx047.swordman.data.LogExporter
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.dfdx047.swordman.R
import com.dfdx047.swordman.ui.screens.AboutScreen
import com.dfdx047.swordman.ui.screens.CheatsScreen
import com.dfdx047.swordman.ui.screens.ControlsScreen
import com.dfdx047.swordman.ui.screens.DebugScreen
import com.dfdx047.swordman.ui.screens.DriversScreen
import com.dfdx047.swordman.ui.screens.HomeScreen
import com.dfdx047.swordman.ui.screens.PerformanceScreen
import com.dfdx047.swordman.ui.screens.SettingsScreen
import com.dfdx047.swordman.ui.theme.DfColors

enum class LauncherTab(@StringRes val title: Int, val icon: ImageVector) {
    HOME(R.string.tab_home, Icons.Filled.Home),
    PERFORMANCE(R.string.tab_performance, Icons.Filled.Speed),
    CONTROLS(R.string.tab_controls, Icons.Filled.Gamepad),
    CHEATS(R.string.tab_cheats, Icons.Filled.FastForward),
    DRIVERS(R.string.tab_drivers, Icons.Filled.Memory),
    DEBUG(R.string.tab_debug, Icons.Filled.BugReport),
    SETTINGS(R.string.tab_settings, Icons.Filled.Settings),
    ABOUT(R.string.tab_about, Icons.Filled.Info),
}

/**
 * O launcher: o conteudo da aba por cima e a barra de navegacao em baixo (sete abas, so o
 * icone e o nome da aba selecionada para caberem nas telas pequenas dos portateis).
 */
@Composable
fun LauncherApp(vm: LauncherViewModel) {
    var tab by rememberSaveable { mutableStateOfTab() }
    val snackbar = remember { SnackbarHostState() }
    val context = LocalContext.current

    LaunchedEffect(vm.message) {
        val m = vm.message ?: return@LaunchedEffect
        snackbar.showSnackbar(context.getString(m.res, *m.args.toTypedArray()))
        vm.message = null
    }

    Scaffold(
        snackbarHost = { SnackbarHost(snackbar) },
        contentWindowInsets = WindowInsets.safeDrawing,
        containerColor = MaterialTheme.colorScheme.background,
        bottomBar = {
            NavigationBar(
                containerColor = MaterialTheme.colorScheme.surfaceContainer,
                tonalElevation = 0.dp,
            ) {
                LauncherTab.entries.forEach { t ->
                    NavigationBarItem(
                        selected = t == tab,
                        onClick = { tab = t },
                        icon = { Icon(t.icon, null) },
                        label = { Text(stringResource(t.title), maxLines = 1, overflow = TextOverflow.Ellipsis) },
                        alwaysShowLabel = false,
                        colors = NavigationBarItemDefaults.colors(
                            selectedIconColor = MaterialTheme.colorScheme.primary,
                            selectedTextColor = MaterialTheme.colorScheme.primary,
                            indicatorColor = MaterialTheme.colorScheme.surfaceVariant,
                        ),
                    )
                }
            }
        },
    ) { inner ->
        Column(
            Modifier
                .fillMaxSize()
                .padding(inner)
                .background(MaterialTheme.colorScheme.background),
        ) {
            BusyBar(vm.busy)
            TabContent(tab, vm) { tab = it }
        }
    }

    if (vm.showGpuNotice) GpuNotice(vm, openTab = { tab = it })
    if (vm.showUpdateDialog) UpdateDialog(vm)
    vm.errorReport?.let { ErrorDialog(vm, it, openTab = { t -> tab = t }) }
}

private fun mutableStateOfTab() = androidx.compose.runtime.mutableStateOf(LauncherTab.HOME)

@Composable
private fun BusyBar(busy: Busy?) {
    if (busy == null) return
    Column(
        Modifier
            .fillMaxWidth()
            .background(MaterialTheme.colorScheme.secondaryContainer)
            .padding(horizontal = 16.dp, vertical = 8.dp),
    ) {
        Text(
            stringResource(busy.label) + if (busy.detail.isNotEmpty()) " · ${busy.detail}" else "",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSecondaryContainer,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
        Spacer(Modifier.heightIn(min = 4.dp))
        if (busy.progress != null) {
            LinearProgressIndicator(progress = { busy.progress.coerceIn(0f, 1f) }, modifier = Modifier.fillMaxWidth())
        } else {
            LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
        }
    }
}

@Composable
private fun TabContent(tab: LauncherTab, vm: LauncherViewModel, goTo: (LauncherTab) -> Unit) {
    Box(Modifier.fillMaxSize()) {
        when (tab) {
            LauncherTab.HOME -> HomeScreen(vm, goTo)
            LauncherTab.PERFORMANCE -> PerformanceScreen(vm)
            LauncherTab.CONTROLS -> ControlsScreen(vm)
            LauncherTab.CHEATS -> CheatsScreen(vm)
            LauncherTab.DRIVERS -> DriversScreen(vm)
            LauncherTab.DEBUG -> DebugScreen(vm)
            LauncherTab.SETTINGS -> SettingsScreen(vm)
            LauncherTab.ABOUT -> AboutScreen(vm)
        }
    }
}

/**
 * Aviso de primeiro arranque (e pelo Sobre): em que GPUs/drivers o jogo foi otimizado
 * e o que fazer nas Mali. Fechar marca-o como visto.
 */
@Composable
private fun GpuNotice(vm: LauncherViewModel, openTab: (LauncherTab) -> Unit) {
    AlertDialog(
        onDismissRequest = { vm.dismissGpuNotice() },
        icon = { Icon(Icons.Filled.Memory, null) },
        title = { Text(stringResource(R.string.gpu_notice_title)) },
        text = {
            Text(
                stringResource(R.string.gpu_notice_text),
                modifier = Modifier
                    .heightIn(max = 360.dp)
                    .verticalScroll(rememberScrollState()),
            )
        },
        confirmButton = {
            TextButton(onClick = { vm.dismissGpuNotice() }) { Text(stringResource(R.string.gpu_notice_ok)) }
        },
        dismissButton = {
            Row {
                TextButton(onClick = {
                    vm.dismissGpuNotice()
                    openTab(LauncherTab.DRIVERS)
                }) { Text(stringResource(R.string.tab_drivers)) }
                TextButton(onClick = {
                    vm.dismissGpuNotice()
                    openTab(LauncherTab.PERFORMANCE)
                }) { Text(stringResource(R.string.tab_performance)) }
            }
        },
    )
}

@Composable
private fun UpdateDialog(vm: LauncherViewModel) {
    val info = vm.updateInfo ?: return
    val context = LocalContext.current
    val language = context.resources.configuration.locales[0].language
    AlertDialog(
        onDismissRequest = { vm.showUpdateDialog = false },
        title = { Text(stringResource(R.string.update_title, info.version)) },
        text = {
            Column(
                Modifier
                    .heightIn(max = 360.dp)
                    .verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Text(
                    stringResource(R.string.update_current, com.dfdx047.swordman.BuildConfig.VERSION_NAME),
                    style = MaterialTheme.typography.bodySmall,
                )
                Text(stringResource(R.string.update_changelog), style = MaterialTheme.typography.titleSmall)
                Surface(
                    shape = RoundedCornerShape(12.dp),
                    color = MaterialTheme.colorScheme.surfaceContainerHigh,
                ) {
                    Text(
                        info.changelog(language).ifBlank { stringResource(R.string.update_no_changelog) },
                        style = MaterialTheme.typography.bodyMedium,
                        modifier = Modifier.padding(12.dp),
                    )
                }
            }
        },
        confirmButton = {
            TextButton(onClick = { vm.downloadUpdate(context) }) {
                Text(stringResource(if (info.apkUrl != null) R.string.update_install else R.string.update_open_page))
            }
        },
        dismissButton = {
            Row {
                TextButton(onClick = { vm.skipUpdate() }) { Text(stringResource(R.string.update_skip)) }
                TextButton(onClick = { vm.showUpdateDialog = false }) { Text(stringResource(R.string.later)) }
            }
        },
    )
}

/**
 * Aviso de erro (crash, jogo nao abriu, driver, ISO corrompida, importacao): o que aconteceu,
 * o que tentar, as ultimas linhas do log e como pedir ajuda no Discord (canal de logs) com o
 * zip dos logs, que o botao "Guardar log" grava onde o jogador quiser.
 */
@Composable
private fun ErrorDialog(vm: LauncherViewModel, report: CrashReport, openTab: (LauncherTab) -> Unit) {
    val context = LocalContext.current
    val saveLog = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/zip")) { uri ->
        if (uri != null) vm.saveLogTo(context, uri)
    }
    val (title, text) = when (report.kind) {
        ErrorKind.CRASH -> R.string.err_dialog_crash_title to R.string.err_dialog_crash_text
        ErrorKind.NO_BOOT -> R.string.err_dialog_noboot_title to R.string.err_dialog_noboot_text
        ErrorKind.DRIVER_BOOT -> R.string.err_dialog_driver_boot_title to R.string.err_dialog_driver_boot_text
        ErrorKind.DRIVER_INSTALL -> R.string.err_dialog_driver_title to R.string.err_dialog_driver_text
        ErrorKind.BAD_ISO -> R.string.err_dialog_iso_title to R.string.err_dialog_iso_text
        ErrorKind.IMPORT -> R.string.err_dialog_import_title to R.string.err_dialog_import_text
    }
    AlertDialog(
        onDismissRequest = { vm.dismissError() },
        icon = { Icon(Icons.Filled.ErrorOutline, null, tint = MaterialTheme.colorScheme.error) },
        title = { Text(stringResource(title)) },
        text = {
            Column(
                Modifier
                    .heightIn(max = 420.dp)
                    .verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                Text(stringResource(text), style = MaterialTheme.typography.bodyMedium)
                Text(report.detail, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                if (report.tail.isNotBlank()) {
                    Surface(shape = RoundedCornerShape(8.dp), color = MaterialTheme.colorScheme.surfaceContainerHigh) {
                        Text(
                            report.tail,
                            style = MaterialTheme.typography.bodySmall.copy(fontFamily = FontFamily.Monospace),
                            modifier = Modifier.padding(10.dp),
                        )
                    }
                }
                if (report.kind == ErrorKind.DRIVER_BOOT) {
                    androidx.compose.material3.Button(onClick = {
                        vm.useSystemDriver()
                        vm.dismissError()
                    }) { Text(stringResource(R.string.err_dialog_use_system_driver)) }
                }
                Text(stringResource(R.string.err_dialog_discord_help), style = MaterialTheme.typography.bodySmall)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    androidx.compose.material3.FilledTonalButton(onClick = { saveLog.launch(LogExporter.suggestedName()) }) {
                        Icon(Icons.Filled.Save, null)
                        Spacer(Modifier.width(6.dp))
                        Text(stringResource(R.string.err_dialog_save_log))
                    }
                    androidx.compose.material3.OutlinedButton(onClick = {
                        context.startActivity(
                            android.content.Intent(android.content.Intent.ACTION_VIEW, android.net.Uri.parse(SwordManApp.DISCORD_URL)),
                        )
                    }) { Text("Discord") }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = { vm.dismissError() }) { Text(stringResource(R.string.close)) }
        },
        dismissButton = {
            when (report.kind) {
                ErrorKind.DRIVER_BOOT, ErrorKind.DRIVER_INSTALL -> TextButton(onClick = {
                    vm.dismissError()
                    openTab(LauncherTab.DRIVERS)
                }) { Text(stringResource(R.string.tab_drivers)) }
                ErrorKind.CRASH, ErrorKind.NO_BOOT -> TextButton(onClick = {
                    vm.dismissError()
                    openTab(LauncherTab.PERFORMANCE)
                }) { Text(stringResource(R.string.tab_performance)) }
                else -> {}
            }
        },
    )
}
