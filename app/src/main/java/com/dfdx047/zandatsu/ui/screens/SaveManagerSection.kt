package com.dfdx047.zandatsu.ui.screens

import android.content.Intent
import android.net.Uri
import android.text.format.DateFormat
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Save
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.dfdx047.zandatsu.R
import com.dfdx047.zandatsu.data.SaveManager
import com.dfdx047.zandatsu.ui.components.Level
import com.dfdx047.zandatsu.ui.components.SectionCard
import com.dfdx047.zandatsu.ui.components.StatusLine
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** Aba Sistema: exportar e restaurar os saves. */
@Composable
fun SaveManagerSection() {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val prefs = remember { context.getSharedPreferences("save_manager", 0) }
    var tree by remember { mutableStateOf(prefs.getString("tree", null)?.let(Uri::parse)) }
    var backups by remember { mutableStateOf(emptyList<SaveManager.Backup>()) }
    var message by remember { mutableStateOf<Pair<String, Level>?>(null) }
    var confirm by remember { mutableStateOf<SaveManager.Backup?>(null) }
    var busy by remember { mutableStateOf(false) }

    fun refresh() {
        scope.launch { backups = withContext(Dispatchers.IO) { SaveManager.list(context, tree) } }
    }
    LaunchedEffect(tree) { refresh() }

    val pick = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri ->
        if (uri != null) {
            context.contentResolver.takePersistableUriPermission(
                uri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
            )
            prefs.edit().putString("tree", uri.toString()).apply()
            tree = uri
        }
    }

    SectionCard(stringResource(R.string.saves_title), Icons.Filled.Save, subtitle = stringResource(R.string.saves_sub)) {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = { pick.launch(tree) }) { Text(stringResource(R.string.saves_folder)) }
            FilledTonalButton(enabled = tree != null && !busy, onClick = {
                busy = true
                scope.launch {
                    message = withContext(Dispatchers.IO) {
                        runCatching {
                            if (!SaveManager.hasSaves()) {
                                context.getString(R.string.saves_no_data) to Level.WARN
                            } else {
                                context.getString(R.string.saves_ok_export, SaveManager.exportTo(context, tree!!)) to Level.OK
                            }
                        }.getOrElse { context.getString(R.string.saves_fail, it.message ?: "?") to Level.ERROR }
                    }
                    busy = false
                    refresh()
                }
            }) { Text(stringResource(R.string.saves_export)) }
        }
        if (tree == null) StatusLine(stringResource(R.string.saves_folder_none), Level.INFO)
        message?.let { (m, l) -> StatusLine(m, l) }
        if (backups.isEmpty()) Text(stringResource(R.string.saves_empty), style = MaterialTheme.typography.bodySmall)
        for (b in backups) {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Column(Modifier.weight(1f)) {
                    Text(if (b.file != null) stringResource(R.string.saves_auto) else b.name, style = MaterialTheme.typography.bodyMedium)
                    Text(
                        DateFormat.format("dd/MM/yyyy HH:mm", b.time).toString() + " · " + (b.size / 1024) + " KB",
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
                TextButton(enabled = !busy, onClick = { confirm = b }) { Text(stringResource(R.string.saves_restore)) }
            }
        }
    }

    confirm?.let { b ->
        AlertDialog(
            onDismissRequest = { confirm = null },
            title = { Text(stringResource(R.string.saves_restore_title)) },
            text = { Text(stringResource(R.string.saves_restore_text, b.name, stringResource(R.string.saves_auto))) },
            confirmButton = {
                TextButton(onClick = {
                    confirm = null
                    busy = true
                    scope.launch {
                        message = withContext(Dispatchers.IO) {
                            if (SaveManager.gameRunning(context)) {
                                context.getString(R.string.saves_game_running) to Level.WARN
                            } else {
                                runCatching { context.getString(R.string.saves_ok_restore, SaveManager.restore(context, b)) to Level.OK }
                                    .getOrElse { context.getString(R.string.saves_fail, it.message ?: "?") to Level.ERROR }
                            }
                        }
                        busy = false
                        refresh()
                    }
                }) { Text(stringResource(R.string.saves_restore)) }
            },
            dismissButton = { TextButton(onClick = { confirm = null }) { Text(stringResource(R.string.cancel)) } },
        )
    }
}
