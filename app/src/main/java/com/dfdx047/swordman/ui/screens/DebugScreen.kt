package com.dfdx047.swordman.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.BlurOn
import androidx.compose.material.icons.filled.BugReport
import androidx.compose.material.icons.filled.CameraAlt
import androidx.compose.material.icons.filled.Description
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material.icons.filled.PhotoSizeSelectLarge
import androidx.compose.material.icons.filled.Speed
import androidx.compose.material.icons.filled.Wallpaper
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.OutlinedButton
import androidx.compose.ui.platform.LocalContext
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.dfdx047.swordman.R
import com.dfdx047.swordman.data.GameConfig
import com.dfdx047.swordman.ui.LauncherViewModel
import com.dfdx047.swordman.ui.components.Level
import com.dfdx047.swordman.ui.components.SectionCard
import com.dfdx047.swordman.ui.components.SettingDropdown
import com.dfdx047.swordman.ui.components.SettingSegmented
import com.dfdx047.swordman.ui.components.SettingSlider
import com.dfdx047.swordman.ui.components.SettingSwitch
import com.dfdx047.swordman.ui.components.StatusLine
import kotlin.math.roundToInt

/**
 * Aba Depuracao: logs, diagnostico de desempenho, capturas de frames e os atalhos do
 * renderizador (para isolar um problema grafico: desligar um a um e comparar).
 */
@Composable
fun DebugScreen(vm: LauncherViewModel) {
    val c = vm.config
    val context = LocalContext.current

    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        StatusLine(stringResource(R.string.debug_note), Level.INFO)

        SectionCard(stringResource(R.string.debug_logs_title), Icons.Filled.Description, subtitle = stringResource(R.string.debug_logs_sub)) {
            SettingSwitch(
                stringResource(R.string.settings_verbose_log),
                stringResource(R.string.settings_verbose_log_sub),
                checked = c.verboseLog,
            ) { on -> vm.updateConfig { it.copy(verboseLog = on) } }
            SettingSwitch(
                stringResource(R.string.settings_perf_log),
                stringResource(R.string.settings_perf_log_sub),
                checked = c.perfLogging,
            ) { on -> vm.updateConfig { it.copy(perfLogging = on) } }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = { vm.exportLogs(context) }, enabled = vm.busy == null) {
                    Text(stringResource(R.string.export_logs))
                }
                OutlinedButton(onClick = { vm.openGpuNotice() }) {
                    Text(stringResource(R.string.about_gpu_notice))
                }
            }
        }

        SectionCard(stringResource(R.string.debug_capture_title), Icons.Filled.CameraAlt) {
            Text(stringResource(R.string.debug_capture_text), style = MaterialTheme.typography.bodySmall)
            Text(stringResource(R.string.debug_selftest_text), style = MaterialTheme.typography.bodySmall)
        }

        // ------------------------------------------------------ otimizacoes da GPU
        SectionCard(
            stringResource(R.string.perf_gpu_title),
            Icons.Filled.BugReport,
            subtitle = stringResource(R.string.perf_gpu_sub),
        ) {
            SettingSwitch(
                stringResource(R.string.settings_safe_gpu),
                stringResource(R.string.settings_safe_gpu_sub),
                checked = c.safeGpu,
            ) { on -> vm.updateConfig { it.copy(safeGpu = on) } }
            if (c.safeGpu) StatusLine(stringResource(R.string.settings_safe_gpu_on), Level.WARN)
            HorizontalDivider()
            SettingSwitch(
                stringResource(R.string.settings_rt_direct),
                stringResource(R.string.settings_rt_direct_sub),
                checked = c.rtDirect,
            ) { on -> vm.updateConfig { it.copy(rtDirect = on) } }
            SettingSwitch(
                stringResource(R.string.settings_spec_shaders),
                stringResource(R.string.settings_spec_shaders_sub),
                checked = c.specShaders,
            ) { on -> vm.updateConfig { it.copy(specShaders = on) } }
            SettingSwitch(
                stringResource(R.string.settings_clear_1x),
                stringResource(R.string.settings_clear_1x_sub),
                checked = c.clearTo1x,
            ) { on -> vm.updateConfig { it.copy(clearTo1x = on) } }
            SettingSwitch(
                stringResource(R.string.settings_into_texture),
                stringResource(R.string.settings_into_texture_sub),
                checked = c.intoTexture,
            ) { on -> vm.updateConfig { it.copy(intoTexture = on) } }
            SettingSwitch(
                stringResource(R.string.settings_post_lite),
                stringResource(R.string.settings_post_lite_sub),
                checked = c.postLite,
            ) { on -> vm.updateConfig { it.copy(postLite = on) } }
            SettingSwitch(
                stringResource(R.string.settings_direct_resolve),
                stringResource(R.string.settings_direct_resolve_sub),
                checked = c.directResolve != 0,
            ) { on -> vm.updateConfig { it.copy(directResolve = if (on) 1 else 0) } }
            if (c.directResolve != 0) {
                SettingSwitch(
                    stringResource(R.string.settings_direct_resolve_fast),
                    stringResource(R.string.settings_direct_resolve_fast_sub),
                    checked = c.directResolveFast,
                ) { on -> vm.updateConfig { it.copy(directResolveFast = on) } }
            }
        }
    }
}
