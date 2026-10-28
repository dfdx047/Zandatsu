package com.dfdx047.zandatsu.ui.screens

import android.content.Intent
import android.net.Uri
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Code
import androidx.compose.material.icons.filled.Favorite
import androidx.compose.material.icons.filled.SystemUpdate
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.dfdx047.zandatsu.BuildConfig
import com.dfdx047.zandatsu.SwordManApp
import com.dfdx047.zandatsu.R
import com.dfdx047.zandatsu.ui.LauncherViewModel
import com.dfdx047.zandatsu.ui.UpdateState
import com.dfdx047.zandatsu.ui.components.SectionCard
import com.dfdx047.zandatsu.ui.theme.LogoStyle
import androidx.compose.ui.draw.clip

@Composable
fun AboutScreen(vm: LauncherViewModel) {
    val context = LocalContext.current
    fun open(url: String) = context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))

    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        androidx.compose.foundation.layout.Box(
            Modifier
                .size(128.dp)
                .iconClip(),
        ) {
            Icon(
                painterResource(R.drawable.ic_launcher_background),
                null,
                tint = Color.Unspecified,
                modifier = Modifier.fillMaxSize(),
            )
            Icon(
                painterResource(R.drawable.ic_launcher_foreground),
                null,
                tint = Color.Unspecified,
                modifier = Modifier.fillMaxSize(),
            )
        }
        Text("PROJECT SWORDMAN", style = LogoStyle, color = MaterialTheme.colorScheme.onBackground)
        Text(stringResource(R.string.game_title), style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(stringResource(R.string.about_version, BuildConfig.VERSION_NAME, BuildConfig.VERSION_CODE), style = MaterialTheme.typography.bodyMedium)
        Text(stringResource(R.string.about_developer, SwordManApp.DEVELOPER), style = MaterialTheme.typography.titleMedium)

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { open(SwordManApp.GITHUB_URL) }) {
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
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = { open(SwordManApp.DISCORD_URL) }) { Text("Discord") }
            OutlinedButton(onClick = { open(SwordManApp.YOUTUBE_URL) }) { Text("YouTube") }
            OutlinedButton(onClick = { open(SwordManApp.KOFI_URL) }) { Text("Ko-fi") }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = { vm.openWhatsNew() }) {
                Text(stringResource(R.string.about_whats_new))
            }
            OutlinedButton(onClick = { vm.openGpuNotice() }) {
                Text(stringResource(R.string.about_gpu_notice))
            }
            OutlinedButton(onClick = { vm.exportLogs(context) }, enabled = vm.busy == null) {
                Text(stringResource(R.string.export_logs))
            }
        }
        if (vm.updateState == UpdateState.AVAILABLE) {
            TextButton(onClick = { vm.showUpdateDialog = true }) {
                Text(stringResource(R.string.about_update_available, vm.updateInfo?.version ?: ""))
            }
        }

        SectionCard(stringResource(R.string.about_project_title), Icons.Filled.Favorite, modifier = Modifier.fillMaxWidth()) {
            Text(stringResource(R.string.about_project_text), style = MaterialTheme.typography.bodySmall)
            TextButton(onClick = { open(SwordManApp.ORIGINAL_PROJECT_URL) }) {
                Text("ReXGlue SDK")
            }
            Text(stringResource(R.string.about_credits), style = MaterialTheme.typography.bodySmall)
            Text(stringResource(R.string.about_legal), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

/** Cantos do icone como no lancador do Android (squircle aproximado). */
private fun Modifier.iconClip(): Modifier =
    this.clip(androidx.compose.foundation.shape.RoundedCornerShape(28.dp))
