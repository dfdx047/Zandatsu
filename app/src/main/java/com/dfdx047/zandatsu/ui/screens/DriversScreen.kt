package com.dfdx047.zandatsu.ui.screens

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
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.dfdx047.zandatsu.R
import com.dfdx047.zandatsu.drivers.DriverRelease
import com.dfdx047.zandatsu.drivers.DriverRepository
import com.dfdx047.zandatsu.drivers.InstalledDriver
import com.dfdx047.zandatsu.ui.LauncherViewModel
import com.dfdx047.zandatsu.ui.components.Level
import com.dfdx047.zandatsu.ui.components.Pill
import com.dfdx047.zandatsu.ui.components.SectionCard
import com.dfdx047.zandatsu.ui.components.SettingSwitch
import com.dfdx047.zandatsu.ui.components.SettingSegmented
import com.dfdx047.zandatsu.ui.components.StatusLine
import com.dfdx047.zandatsu.ui.components.formatBytes
import com.dfdx047.zandatsu.ui.theme.DfColors

@Composable
fun DriversScreen(vm: LauncherViewModel) {
    val context = LocalContext.current
    val pickFile = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri != null) vm.importDriver(uri)
    }
    LaunchedEffect(Unit) {
        vm.refreshDrivers()
        vm.refreshTurnipBisect()
    }
    // As fontes desta GPU so se sabem depois do refreshDrivers (Turnip no Adreno, RADV na Xclipse).
    LaunchedEffect(vm.driverSources) {
        if (vm.driverSources.isNotEmpty() && vm.driverReleases.isEmpty() && !vm.driverReleasesLoading) {
            vm.loadDriverReleases(vm.driverSource)
        }
    }
    val radvOldAndroid = vm.isXclipse && android.os.Build.VERSION.SDK_INT < DriverRepository.RADV_MIN_API

    LazyColumn(
        Modifier.fillMaxSize(),
        contentPadding = PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        item {
            SectionCard(stringResource(R.string.drivers_device), Icons.Filled.PhoneAndroid) {
                Text(vm.socName, style = MaterialTheme.typography.bodyLarge)
                if (vm.gpuName.isNotEmpty()) {
                    Text(vm.gpuName, style = MaterialTheme.typography.bodyMedium)
                }
                val g = vm.turnipGpu
                if (g.adreno != 0) {
                    Text(stringResource(R.string.drivers_gpu_line, g.adreno, g.gen), style = MaterialTheme.typography.bodyMedium)
                }
                if (vm.isAdreno) {
                    StatusLine(
                        stringResource(
                            when (g.level) {
                                com.dfdx047.zandatsu.drivers.TurnipSupport.Level.STABLE -> R.string.drivers_turnip_stable
                                com.dfdx047.zandatsu.drivers.TurnipSupport.Level.EXPERIMENTAL -> R.string.drivers_turnip_experimental
                                else -> R.string.drivers_turnip_unsupported
                            },
                            g.builds,
                        ),
                        if (g.level == com.dfdx047.zandatsu.drivers.TurnipSupport.Level.STABLE) Level.INFO else Level.WARN,
                    )
                    if (g.tuDebug.isNotEmpty()) {
                        StatusLine(stringResource(R.string.drivers_turnip_auto, g.tuDebug), Level.INFO)
                    }
                    // Renut: no Adreno 6xx o driver do sistema nao serve; outro driver pode ter falhado a verificacao.
                    val renutMissing = remember(vm.activeDriver) { com.dfdx047.zandatsu.data.RenutSupport.missing() }
                    if (vm.activeDriver == null && g.gen == "a6xx") {
                        StatusLine(stringResource(R.string.drivers_renut_system_6xx), Level.ERROR)
                    } else if (renutMissing != null) {
                        StatusLine(stringResource(R.string.drivers_renut_missing, renutMissing), Level.ERROR)
                    }
                    if (vm.driverBootFailed.isNotEmpty()) {
                        StatusLine(stringResource(R.string.drivers_boot_failed, vm.driverBootFailed.substringBefore(':')), Level.WARN)
                    }
                } else if (vm.isXclipse) {
                    // 26/10: RADV (Mesa) nas Xclipse, experimental e so por escolha; o driver da Samsung fica.
                    StatusLine(stringResource(R.string.drivers_xclipse_warning), Level.WARN)
                    if (radvOldAndroid) {
                        StatusLine(stringResource(R.string.drivers_radv_needs_android14, android.os.Build.VERSION.RELEASE), Level.WARN)
                    }
                    val renutMissing = remember(vm.activeDriver) { com.dfdx047.zandatsu.data.RenutSupport.missing() }
                    if (renutMissing != null) {
                        StatusLine(stringResource(R.string.renderer_renut_missing_gpu, renutMissing), Level.ERROR)
                    }
                    if (vm.driverBootFailed.isNotEmpty()) {
                        StatusLine(stringResource(R.string.drivers_boot_failed, vm.driverBootFailed.substringBefore(':')), Level.WARN)
                    }
                } else {
                    StatusLine(stringResource(R.string.drivers_not_adreno), Level.WARN)
                }
            }
        }
        // Fora do Adreno e da Xclipse (Mali): so o driver do sistema, sem escolha nem downloads.
        if (!vm.isAdreno && !vm.isXclipse) return@LazyColumn
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
        // Xclipse num Android antes do 14: o RADV nao corre, sem lista (a explicacao esta no cartao do aparelho).
        if (radvOldAndroid || vm.driverSources.isEmpty()) return@LazyColumn
        item {
            SectionCard(stringResource(R.string.drivers_download_title), Icons.Filled.CloudDownload, subtitle = stringResource(R.string.drivers_download_sub)) {
                SettingSegmented(
                    title = stringResource(R.string.drivers_source),
                    options = vm.driverSources.map { it to it.short },
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
private fun ReleaseCard(r: DriverRelease, enabled: Boolean, onDownload: (com.dfdx047.zandatsu.drivers.DriverAsset) -> Unit) {
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
