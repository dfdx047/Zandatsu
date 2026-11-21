@file:OptIn(ExperimentalMaterial3Api::class)

package com.dfdx047.zandatsu.pc.ui

import androidx.activity.compose.BackHandler
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
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Gamepad
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material.icons.filled.MoreHoriz
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.NavigationRail
import androidx.compose.material3.NavigationRailItem
import androidx.compose.material3.NavigationRailItemDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.dfdx047.zandatsu.pc.BuildConfig
import com.dfdx047.zandatsu.pc.R
import com.dfdx047.zandatsu.pc.ui.screens.AboutScreen
import com.dfdx047.zandatsu.pc.ui.screens.ControlsScreen
import com.dfdx047.zandatsu.pc.ui.screens.DriversScreen
import com.dfdx047.zandatsu.pc.ui.screens.GraphicsScreen
import com.dfdx047.zandatsu.pc.ui.screens.HomeScreen
import com.dfdx047.zandatsu.pc.ui.screens.MoreScreen
import com.dfdx047.zandatsu.pc.ui.screens.SettingsScreen

/**
 * A mesma navegacao da launcher do 360: cinco destinos na barra (Inicio, Graficos, Controles, Drivers, Mais); Sistema
 * e Sobre ficam dentro de "Mais" ([main] = false).
 */
enum class LauncherTab(@StringRes val title: Int, val icon: ImageVector, val main: Boolean = true) {
    HOME(R.string.tab_home, Icons.Filled.Home),
    GRAPHICS(R.string.tab_graphics, Icons.Filled.Tune),
    CONTROLS(R.string.tab_controls, Icons.Filled.Gamepad),
    DRIVERS(R.string.tab_drivers, Icons.Filled.Memory),
    MORE(R.string.tab_more, Icons.Filled.MoreHoriz),
    SETTINGS(R.string.tab_system, Icons.Filled.Settings, main = false),
    ABOUT(R.string.tab_about, Icons.Filled.Info, main = false),
}

/**
 * A launcher. Retrato: o conteudo e a barra de navegacao em baixo. Paisagem larga: um trilho a esquerda e o conteudo
 * centrado (copia do LauncherApp da app do 360).
 */
@Composable
fun LauncherApp(vm: LauncherViewModel) {
    var tab by rememberSaveable { mutableStateOf(LauncherTab.HOME) }
    val snackbar = remember { SnackbarHostState() }
    val context = LocalContext.current

    LaunchedEffect(vm.message) {
        val m = vm.message ?: return@LaunchedEffect
        snackbar.showSnackbar(context.getString(m.res, *m.args.toTypedArray()))
        vm.message = null
    }
    BackHandler(enabled = tab != LauncherTab.HOME) {
        tab = if (!tab.main) LauncherTab.MORE else LauncherTab.HOME
    }

    val config = LocalConfiguration.current
    val rail = config.screenWidthDp > config.screenHeightDp && config.screenWidthDp >= 600
    val current = if (tab.main) tab else LauncherTab.MORE
    val mains = LauncherTab.entries.filter { it.main }

    Scaffold(
        snackbarHost = { SnackbarHost(snackbar) },
        contentWindowInsets = WindowInsets.safeDrawing,
        containerColor = MaterialTheme.colorScheme.background,
        bottomBar = {
            if (!rail) {
                NavigationBar(containerColor = MaterialTheme.colorScheme.surfaceContainer, tonalElevation = 0.dp) {
                    mains.forEach { t ->
                        NavigationBarItem(
                            selected = t == current,
                            onClick = { tab = t },
                            icon = { Icon(t.icon, null) },
                            label = { Text(stringResource(t.title), maxLines = 1, softWrap = false) },
                            alwaysShowLabel = true,
                            colors = NavigationBarItemDefaults.colors(
                                selectedIconColor = MaterialTheme.colorScheme.primary,
                                selectedTextColor = MaterialTheme.colorScheme.primary,
                                indicatorColor = MaterialTheme.colorScheme.surfaceVariant,
                            ),
                        )
                    }
                }
            }
        },
    ) { inner ->
        Row(
            Modifier
                .fillMaxSize()
                .padding(inner)
                .background(MaterialTheme.colorScheme.background),
        ) {
            if (rail) {
                NavigationRail(containerColor = MaterialTheme.colorScheme.surfaceContainer, windowInsets = WindowInsets(0)) {
                    Spacer(Modifier.weight(1f))
                    mains.forEach { t ->
                        NavigationRailItem(
                            selected = t == current,
                            onClick = { tab = t },
                            icon = { Icon(t.icon, null) },
                            label = { Text(stringResource(t.title), maxLines = 1, softWrap = false) },
                            alwaysShowLabel = true,
                            colors = NavigationRailItemDefaults.colors(
                                selectedIconColor = MaterialTheme.colorScheme.primary,
                                selectedTextColor = MaterialTheme.colorScheme.primary,
                                indicatorColor = MaterialTheme.colorScheme.surfaceVariant,
                            ),
                        )
                    }
                    Spacer(Modifier.weight(1f))
                }
            }
            Column(Modifier.weight(1f).fillMaxHeight()) {
                if (!tab.main) SubScreenHeader(tab) { tab = LauncherTab.MORE }
                // Volta do jogo, do editor ou do mapeador: o Inicio e os Controles releem as preferencias (o menu do jogo
                // muda-as). Os outros nao: um resultado do SAF (driver, log) chega antes do onResume e o reset cancelava-o.
                val refresh = if (tab == LauncherTab.HOME || tab == LauncherTab.CONTROLS) vm.resumes else 0
                key(refresh) { TabContent(tab, vm) { tab = it } }
            }
        }
    }

    if (vm.showUpdateDialog) UpdateDialog(vm)
    vm.goWarning?.let { msg ->
        AlertDialog(
            onDismissRequest = { vm.goWarning = null },
            title = { Text(stringResource(R.string.go_write_failed_title)) },
            text = { Text(msg) },
            confirmButton = { TextButton({ vm.goWarning = null; vm.launch(context) }) { Text(stringResource(R.string.play_anyway)) } },
            dismissButton = { TextButton({ vm.goWarning = null }) { Text(stringResource(R.string.cancel)) } },
        )
    }
}

/** Titulo de Sistema / Sobre, com a seta para voltar a "Mais". */
@Composable
private fun SubScreenHeader(tab: LauncherTab, onBack: () -> Unit) {
    Row(
        Modifier.fillMaxWidth().padding(start = 4.dp, end = 16.dp, top = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        IconButton(onClick = onBack) { Icon(Icons.AutoMirrored.Filled.ArrowBack, stringResource(R.string.back)) }
        Icon(tab.icon, null, tint = MaterialTheme.colorScheme.primary, modifier = Modifier.size(20.dp))
        Spacer(Modifier.width(10.dp))
        Text(stringResource(tab.title), style = MaterialTheme.typography.titleLarge)
    }
}

@Composable
private fun TabContent(tab: LauncherTab, vm: LauncherViewModel, goTo: (LauncherTab) -> Unit) {
    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.TopCenter) {
        Box(Modifier.fillMaxHeight().widthIn(max = 920.dp)) {
            when (tab) {
                LauncherTab.HOME -> HomeScreen(vm, goTo)
                LauncherTab.GRAPHICS -> GraphicsScreen()
                LauncherTab.CONTROLS -> ControlsScreen()
                LauncherTab.DRIVERS -> DriversScreen()
                LauncherTab.MORE -> MoreScreen(goTo)
                LauncherTab.SETTINGS -> SettingsScreen(vm)
                LauncherTab.ABOUT -> AboutScreen(vm)
            }
        }
    }
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
                Modifier.heightIn(max = 360.dp).verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Text(stringResource(R.string.update_current, BuildConfig.VERSION_NAME), style = MaterialTheme.typography.bodySmall)
                Text(stringResource(R.string.update_changelog), style = MaterialTheme.typography.titleSmall)
                Surface(shape = RoundedCornerShape(12.dp), color = MaterialTheme.colorScheme.surfaceContainerHigh) {
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
                Text(stringResource(if (info.apkUrl != null) R.string.update_download else R.string.update_open_page))
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
