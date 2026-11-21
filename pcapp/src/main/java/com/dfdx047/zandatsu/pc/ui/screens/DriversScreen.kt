package com.dfdx047.zandatsu.pc.ui.screens

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CloudDownload
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.FileOpen
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.dfdx047.zandatsu.pc.PcGame
import com.dfdx047.zandatsu.pc.R
import com.dfdx047.zandatsu.pc.drivers.DriverAsset
import com.dfdx047.zandatsu.pc.drivers.DriverException
import com.dfdx047.zandatsu.pc.drivers.DriverRelease
import com.dfdx047.zandatsu.pc.drivers.DriverRepository
import com.dfdx047.zandatsu.pc.drivers.DriverSource
import com.dfdx047.zandatsu.pc.drivers.InstalledDriver
import com.dfdx047.zandatsu.pc.ui.components.ButtonRow
import com.dfdx047.zandatsu.pc.ui.components.Level
import com.dfdx047.zandatsu.pc.ui.components.SectionCard
import com.dfdx047.zandatsu.pc.ui.components.StatusLine
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/**
 * Drivers (separador proprio, como na launcher do 360): o driver Vulkan do jogo de PC ("Sistema" ou um instalado nesta
 * app), instalar de um .zip ou das releases do GitHub e apagar. Nunca muda sozinho.
 */
@Composable
fun DriversScreen() {
    val c = LocalContext.current
    val scope = rememberCoroutineScope()
    var drivers by remember { mutableStateOf(DriverRepository.installed()) }
    var driver by remember { mutableStateOf(PcGame.driverDir(c)) }
    var busy by remember { mutableStateOf<String?>(null) }
    var progress by remember { mutableFloatStateOf(-1f) }
    var error by remember { mutableStateOf<String?>(null) }
    var showDownload by remember { mutableStateOf(false) }
    var toDelete by remember { mutableStateOf<InstalledDriver?>(null) }
    fun reasonText(e: Throwable): String = if (e is DriverException) c.getString(when (e.reason) {
        DriverException.Reason.NO_LIBRARY -> R.string.drv_err_no_library
        DriverException.Reason.NOT_ZIP -> R.string.drv_err_not_zip
        DriverException.Reason.IO -> R.string.drv_err_io
        DriverException.Reason.NETWORK -> R.string.drv_err_network
        DriverException.Reason.MIN_API -> R.string.drv_err_min_api
        DriverException.Reason.WRONG_GPU -> R.string.drv_err_wrong_gpu
    }) + (e.message?.takeIf { it.isNotBlank() }?.let { " ($it)" } ?: "") else (e.message ?: e.toString())
    fun install(label: String, work: suspend () -> InstalledDriver) {
        busy = label; progress = -1f; error = null
        scope.launch {
            runCatching { withContext(Dispatchers.IO) { work() } }
                .onSuccess { d -> drivers = DriverRepository.installed(); driver = d.dir.absolutePath; PcGame.setDriverDir(c, driver) }
                .onFailure { error = reasonText(it) }
            busy = null
        }
    }
    val import = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            val name = c.contentResolver.query(uri, arrayOf(android.provider.OpenableColumns.DISPLAY_NAME), null, null, null)
                ?.use { if (it.moveToFirst()) it.getString(0) else null } ?: "driver.zip"
            install(name) { DriverRepository.importUri(c, uri, name) }
        }
    }
    val gpu = remember { DriverRepository.gpuName().ifEmpty { DriverRepository.socName() } }

    Column(
        Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        SectionCard(stringResource(R.string.sys_driver), Icons.Filled.Memory, subtitle = stringResource(R.string.sys_driver_sub, gpu)) {
            DriverRow(stringResource(R.string.sys_driver_system), stringResource(R.string.drv_system_sub), driver.isEmpty(), null) {
                driver = ""; PcGame.setDriverDir(c, "")
            }
            drivers.forEach { d ->
                DriverRow(
                    d.name,
                    listOf(d.version, d.author, d.library.name).filter { it.isNotBlank() }.joinToString(" · "),
                    driver == d.dir.absolutePath,
                    onDelete = { toDelete = d },
                ) { driver = d.dir.absolutePath; PcGame.setDriverDir(c, driver) }
            }
            StatusLine(stringResource(R.string.drv_applies_next), Level.INFO)
        }

        SectionCard(stringResource(R.string.drv_install_title), Icons.Filled.CloudDownload, subtitle = stringResource(R.string.sys_driver_hint)) {
            ButtonRow {
                FilledTonalButton(onClick = { import.launch(arrayOf("application/zip", "application/octet-stream", "*/*")) }, enabled = busy == null) {
                    Icon(Icons.Filled.FileOpen, null)
                    Spacer(Modifier.width(6.dp))
                    Text(stringResource(R.string.sys_driver_import))
                }
                if (DriverRepository.sourcesForDevice().isNotEmpty()) {
                    OutlinedButton(onClick = { showDownload = true }, enabled = busy == null) {
                        Icon(Icons.Filled.CloudDownload, null)
                        Spacer(Modifier.width(6.dp))
                        Text(stringResource(R.string.sys_driver_download))
                    }
                }
            }
            busy?.let {
                Text(stringResource(R.string.sys_driver_installing, it), style = MaterialTheme.typography.bodySmall)
                if (progress >= 0f) LinearProgressIndicator({ progress }, Modifier.fillMaxWidth()) else LinearProgressIndicator(Modifier.fillMaxWidth())
            }
            error?.let { StatusLine(it, Level.ERROR) }
        }
    }

    toDelete?.let { d ->
        AlertDialog(
            onDismissRequest = { toDelete = null },
            title = { Text(stringResource(R.string.sys_driver_delete_title)) },
            text = { Text(d.name) },
            confirmButton = {
                TextButton({
                    DriverRepository.delete(d)
                    if (driver == d.dir.absolutePath) { driver = ""; PcGame.setDriverDir(c, "") }
                    drivers = DriverRepository.installed(); toDelete = null
                }) { Text(stringResource(R.string.delete)) }
            },
            dismissButton = { TextButton({ toDelete = null }) { Text(stringResource(R.string.cancel)) } },
        )
    }
    if (showDownload) DownloadDialog(onDismiss = { showDownload = false }) { asset ->
        showDownload = false
        install(asset.name) {
            DriverRepository.download(asset) { done, total -> progress = if (total > 0) done.toFloat() / total else -1f }
        }
    }
}

@Composable
private fun DriverRow(name: String, detail: String, selected: Boolean, onDelete: (() -> Unit)?, onSelect: () -> Unit) {
    Row(
        Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(10.dp))
            .clickable(onClick = onSelect)
            .padding(vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        RadioButton(selected, onSelect)
        Column(Modifier.weight(1f)) {
            Text(name, style = MaterialTheme.typography.bodyLarge)
            if (detail.isNotBlank()) {
                Text(detail, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
        if (onDelete != null) IconButton(onDelete) { Icon(Icons.Filled.Delete, stringResource(R.string.delete)) }
    }
}

/** Escolher uma fonte (releases do GitHub), uma release e o anexo a instalar. */
@Composable
private fun DownloadDialog(onDismiss: () -> Unit, onPick: (DriverAsset) -> Unit) {
    var source by remember { mutableStateOf<DriverSource?>(null) }
    var releases by remember { mutableStateOf<List<DriverRelease>?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(source) {
        val s = source ?: return@LaunchedEffect
        releases = null; error = null
        runCatching { withContext(Dispatchers.IO) { DriverRepository.fetchReleases(s) } }
            .onSuccess { releases = it }.onFailure { error = it.message ?: it.toString() }
    }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.sys_driver_download)) },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState()).widthIn(max = 480.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                val s = source
                if (s == null) {
                    DriverRepository.sourcesForDevice().forEach { src ->
                        OutlinedButton({ source = src }, Modifier.fillMaxWidth()) { Text(src.label) }
                    }
                } else {
                    Text(s.label, style = MaterialTheme.typography.titleSmall)
                    when {
                        error != null -> Text(error!!, color = MaterialTheme.colorScheme.error)
                        releases == null -> CircularProgressIndicator()
                        releases!!.isEmpty() -> Text("—")
                        else -> releases!!.take(8).forEach { r ->
                            Text("${r.title} (${r.date})", style = MaterialTheme.typography.labelLarge)
                            r.assets.forEach { a ->
                                TextButton({ onPick(a) }) { Text("${a.name} · ${a.size / 1024 / 1024} MB") }
                            }
                        }
                    }
                }
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton({ if (source != null) source = null else onDismiss() }) {
                Text(stringResource(if (source != null) R.string.back else R.string.cancel))
            }
        },
    )
}
