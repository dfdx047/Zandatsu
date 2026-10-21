package com.dfdx047.swordman.ui.screens

import android.content.Intent
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Build
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.CloudDownload
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.FileOpen
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material.icons.filled.OpenInNew
import androidx.compose.material.icons.filled.PhoneAndroid
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.dfdx047.swordman.R
import com.dfdx047.swordman.drivers.DriverRelease
import com.dfdx047.swordman.drivers.DriverRepository
import com.dfdx047.swordman.drivers.InstalledDriver
import com.dfdx047.swordman.ui.LauncherViewModel
import com.dfdx047.swordman.ui.components.Level
import com.dfdx047.swordman.ui.components.Pill
import com.dfdx047.swordman.ui.components.SectionCard
import com.dfdx047.swordman.ui.components.SettingSwitch
import com.dfdx047.swordman.ui.components.SettingSegmented
import com.dfdx047.swordman.ui.components.StatusLine
import com.dfdx047.swordman.ui.components.formatBytes
import com.dfdx047.swordman.ui.theme.DfColors

@Composable
fun DriversScreen(vm: LauncherViewModel) {
    val context = LocalContext.current
    val pickFile = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri != null) vm.importDriver(uri)
    }
    LaunchedEffect(Unit) {
        vm.refreshDrivers()
        vm.refreshTurnipBisect()
        if (vm.driverReleases.isEmpty() && !vm.driverReleasesLoading) vm.loadDriverReleases()
    }

    LazyColumn(
        Modifier.fillMaxSize(),
        contentPadding = PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        item {
            SectionCard(stringResource(R.string.drivers_device), Icons.Filled.PhoneAndroid) {
                Text(vm.socName, style = MaterialTheme.typography.bodyLarge)
                val g = vm.turnipGpu
                if (g.adreno != 0) {
                    Text(stringResource(R.string.drivers_gpu_line, g.adreno, g.gen), style = MaterialTheme.typography.bodyMedium)
                }
                if (vm.isAdreno) {
                    StatusLine(
                        stringResource(
                            when (g.level) {
                                com.dfdx047.swordman.drivers.TurnipSupport.Level.STABLE -> R.string.drivers_turnip_stable
                                com.dfdx047.swordman.drivers.TurnipSupport.Level.EXPERIMENTAL -> R.string.drivers_turnip_experimental
                                else -> R.string.drivers_turnip_unsupported
                            },
                            g.builds,
                        ),
                        if (g.level == com.dfdx047.swordman.drivers.TurnipSupport.Level.STABLE) Level.INFO else Level.WARN,
                    )
                    if (g.tuDebug.isNotEmpty()) {
                        StatusLine(stringResource(R.string.drivers_turnip_auto, g.tuDebug), Level.INFO)
                    }
                    if (vm.driverBootFailed.isNotEmpty()) {
                        StatusLine(stringResource(R.string.drivers_boot_failed, vm.driverBootFailed.substringBefore(':')), Level.WARN)
                    }
                } else {
                    StatusLine(stringResource(R.string.drivers_not_adreno), Level.WARN)
                }
            }
        }
        item {
            SectionCard(stringResource(R.string.drivers_active_title), Icons.Filled.Memory) {
                val active = vm.activeDriver
                Text(
                    if (active == null) stringResource(R.string.driver_current_system) else stringResource(R.string.driver_current, active),
                    style = MaterialTheme.typography.bodyLarge,
                )
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(onClick = { vm.useSystemDriver() }, enabled = active != null) {
                        Text(stringResource(R.string.drivers_use_system))
                    }
                    FilledTonalButton(onClick = { pickFile.launch(arrayOf("application/zip", "application/octet-stream", "*/*")) }, enabled = vm.busy == null) {
                        Icon(Icons.Filled.FileOpen, null)
                        Spacer(Modifier.width(6.dp))
                        Text(stringResource(R.string.drivers_import))
                    }
                }
                Text(stringResource(R.string.drivers_restart_note), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
        item {
            // Testes A/B dos glitches do Turnip nos Adreno 6xx (RP5): cada opcao liga um
            // conjunto de correcoes no arranque do jogo (GameActivity.applyTurnipCompat).
            SectionCard(stringResource(R.string.turnip_compat_title), Icons.Filled.Build, subtitle = stringResource(R.string.turnip_compat_sub)) {
                listOf(
                    1 to R.string.turnip_compat_single_buffer,
                    8 to R.string.turnip_compat_bc,
                    2 to R.string.turnip_compat_sysmem,
                    4 to R.string.turnip_compat_nolrz,
                    16 to R.string.turnip_compat_paths,
                    32 to R.string.turnip_compat_depth,
                    64 to R.string.turnip_compat_sync,
                    128 to R.string.turnip_compat_ir3_nopreamble,
                    256 to R.string.turnip_compat_ir3_nouboopt,
                    512 to R.string.turnip_compat_ir3_nofp16,
                ).forEach { (bit, label) ->
                    SettingSwitch(
                        stringResource(label),
                        checked = (vm.turnipCompatFlags and bit) != 0,
                    ) { on -> vm.setTurnipCompatFlag(bit, on) }
                }
                val sweepCtx = androidx.compose.ui.platform.LocalContext.current
                androidx.compose.material3.FilledTonalButton(
                    onClick = { vm.startTurnipSweep(sweepCtx) },
                    enabled = vm.busy == null && vm.activeDriver != null,
                ) { Text(stringResource(R.string.turnip_sweep_start, com.dfdx047.swordman.drivers.TurnipSupport.BISECT.size)) }
                androidx.compose.material3.FilledTonalButton(
                    onClick = { vm.startTurnipSweep(sweepCtx, "rtt") },
                    enabled = vm.busy == null && vm.activeDriver != null,
                ) { Text(stringResource(R.string.turnip_sweep_rtt, com.dfdx047.swordman.drivers.TurnipSupport.BISECT_RTT.size)) }
                Text(
                    stringResource(R.string.turnip_sweep_help),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                SettingSwitch(
                    stringResource(R.string.turnip_bisect),
                    checked = vm.turnipBisect,
                ) { on -> vm.changeTurnipBisect(on) }
                if (vm.turnipBisect) {
                    val steps = com.dfdx047.swordman.drivers.TurnipSupport.BISECT
                    val next = Math.floorMod(vm.turnipBisectStep, steps.size)
                    StatusLine(stringResource(R.string.turnip_bisect_next, steps[next][0], steps.size), Level.INFO)
                }
            }
        }
        if (vm.installedDrivers.isNotEmpty()) {
            item { Text(stringResource(R.string.drivers_installed), style = MaterialTheme.typography.titleMedium) }
            items(vm.installedDrivers, key = { it.dir.name }) { d ->
                InstalledDriverCard(
                    d,
                    active = vm.activeDriver == d.dir.name,
                    onActivate = { vm.activateDriver(d) },
                    onDelete = { vm.deleteDriver(d) },
                )
            }
        }
        item {
            SectionCard(stringResource(R.string.drivers_download_title), Icons.Filled.CloudDownload, subtitle = stringResource(R.string.drivers_download_sub)) {
                SettingSegmented(
                    title = stringResource(R.string.drivers_source),
                    options = DriverRepository.SOURCES.map { it to it.owner },
                    selected = vm.driverSource,
                    onSelect = { vm.loadDriverReleases(it) },
                )
                Text(vm.driverSource.label, style = MaterialTheme.typography.bodySmall)
                TextButton(onClick = {
                    context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(vm.driverSource.pageUrl)))
                }) {
                    Icon(Icons.Filled.OpenInNew, null, modifier = Modifier.size(18.dp))
                    Spacer(Modifier.width(4.dp))
                    Text(stringResource(R.string.open_on_github))
                }
                when {
                    vm.driverReleasesLoading -> Row(verticalAlignment = Alignment.CenterVertically) {
                        CircularProgressIndicator(Modifier.size(20.dp), strokeWidth = 2.dp)
                        Spacer(Modifier.width(8.dp))
                        Text(stringResource(R.string.loading))
                    }
                    vm.driverReleasesError -> StatusLine(stringResource(R.string.drivers_load_error), Level.ERROR)
                    vm.driverReleases.isEmpty() -> Text(stringResource(R.string.drivers_none_found))
                }
            }
        }
        items(vm.driverReleases, key = { "${it.source.owner}/${it.tag}" }) { r ->
            ReleaseCard(r, enabled = vm.busy == null) { vm.downloadDriver(it) }
        }
    }
}

@Composable
private fun InstalledDriverCard(d: InstalledDriver, active: Boolean, onActivate: () -> Unit, onDelete: () -> Unit) {
    Card(
        shape = RoundedCornerShape(18.dp),
        colors = CardDefaults.cardColors(
            containerColor = if (active) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceContainerHigh,
        ),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Row(Modifier.padding(14.dp), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                Text(d.name, style = MaterialTheme.typography.titleSmall)
                val meta = listOfNotNull(
                    d.version.takeIf { it.isNotBlank() },
                    d.author.takeIf { it.isNotBlank() },
                    d.library.name,
                ).joinToString(" · ")
                Text(meta, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                if (active) Pill(stringResource(R.string.drivers_in_use), DfColors.Ok)
            }
            if (!active) {
                TextButton(onClick = onActivate) {
                    Icon(Icons.Filled.CheckCircle, null)
                    Spacer(Modifier.width(4.dp))
                    Text(stringResource(R.string.drivers_use))
                }
            }
            IconButton(onClick = onDelete) { Icon(Icons.Filled.Delete, stringResource(R.string.delete)) }
        }
    }
}

@Composable
private fun ReleaseCard(r: DriverRelease, enabled: Boolean, onDownload: (com.dfdx047.swordman.drivers.DriverAsset) -> Unit) {
    Card(
        shape = RoundedCornerShape(18.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(r.title, style = MaterialTheme.typography.titleSmall)
            Text("${r.tag} · ${r.date}", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            r.assets.forEach { a ->
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        "${a.name} (${formatBytes(a.size)})",
                        style = MaterialTheme.typography.bodySmall,
                        modifier = Modifier.weight(1f),
                    )
                    TextButton(onClick = { onDownload(a) }, enabled = enabled) {
                        Icon(Icons.Filled.CloudDownload, null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(4.dp))
                        Text(stringResource(R.string.download))
                    }
                }
            }
        }
    }
}
