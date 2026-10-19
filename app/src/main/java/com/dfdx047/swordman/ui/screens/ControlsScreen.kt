package com.dfdx047.swordman.ui.screens

import android.content.Intent
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
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.Gamepad
import androidx.compose.material.icons.filled.RestartAlt
import androidx.compose.material.icons.filled.SportsEsports
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.dfdx047.swordman.R
import com.dfdx047.swordman.controls.ControlLayoutStore
import com.dfdx047.swordman.controls.ControlsEditorActivity
import com.dfdx047.swordman.data.ButtonBadge
import com.dfdx047.swordman.data.ControlStyle
import com.dfdx047.swordman.data.Directional
import com.dfdx047.swordman.data.TouchMode
import com.dfdx047.swordman.ui.LauncherViewModel
import com.dfdx047.swordman.ui.components.Level
import com.dfdx047.swordman.ui.components.SectionCard
import com.dfdx047.swordman.ui.components.SettingSegmented
import com.dfdx047.swordman.ui.components.SettingSlider
import com.dfdx047.swordman.ui.components.SettingSwitch
import com.dfdx047.swordman.ui.components.StatusLine
import kotlin.math.roundToInt

/** Aba Controles: controles na tela (estilo, layout, vibracao) e comando fisico. */
@Composable
fun ControlsScreen(vm: LauncherViewModel) {
    val c = vm.config
    val context = LocalContext.current
    var confirmReset by remember { mutableStateOf(false) }

    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        // ------------------------------------------------------ controles na tela
        SectionCard(stringResource(R.string.settings_controls), Icons.Filled.Gamepad, subtitle = stringResource(R.string.settings_touch_sub)) {
            SettingSegmented(
                title = stringResource(R.string.settings_touch),
                subtitle = stringResource(R.string.settings_touch_mode_sub),
                options = listOf(
                    TouchMode.AUTO to stringResource(R.string.touch_mode_auto),
                    TouchMode.ON to stringResource(R.string.touch_mode_on),
                    TouchMode.OFF to stringResource(R.string.touch_mode_off),
                ),
                selected = vm.touchMode,
                onSelect = vm::setTouchMode,
            )
            SettingSegmented(
                title = stringResource(R.string.ingame_directional),
                subtitle = stringResource(R.string.settings_directional_sub),
                options = listOf(
                    Directional.DPAD to stringResource(R.string.directional_dpad),
                    Directional.STICK to stringResource(R.string.directional_stick),
                ),
                selected = vm.directional,
                onSelect = vm::setDirectional,
            )
            SettingSegmented(
                title = stringResource(R.string.settings_touch_style),
                options = listOf(
                    ControlStyle.GAME to "MGR",
                    ControlStyle.XBOX to "Xbox",
                    ControlStyle.PLAYSTATION to "PS",
                    ControlStyle.NINTENDO to "Nintendo",
                ),
                selected = vm.controlStyle,
                onSelect = vm::setControlStyle,
            )
            if (vm.controlStyle == ControlStyle.GAME) {
                SettingSegmented(
                    title = stringResource(R.string.ingame_badge),
                    options = listOf(
                        ButtonBadge.XBOX to "Xbox",
                        ButtonBadge.PLAYSTATION to "PS",
                        ButtonBadge.NINTENDO to "Nintendo",
                        ButtonBadge.NONE to stringResource(R.string.badge_none),
                    ),
                    selected = vm.buttonBadge,
                    onSelect = vm::setButtonBadge,
                )
            }
            SettingSlider(
                title = stringResource(R.string.settings_touch_opacity),
                value = vm.touchOpacity,
                range = 0.1f..1f,
                label = { v -> "${(v * 100).roundToInt()}%" },
            ) { v -> vm.setTouchOpacity(v) }
            SettingSwitch(stringResource(R.string.settings_touch_haptics), checked = vm.touchHaptics, onChange = vm::setTouchHaptics)
            SettingSwitch(
                stringResource(R.string.settings_pause_menus),
                stringResource(R.string.settings_pause_menus_sub),
                checked = vm.pauseOnMenus,
                onChange = vm::setPauseOnMenus,
            )
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilledTonalButton(onClick = { context.startActivity(Intent(context, ControlsEditorActivity::class.java)) }) {
                    Icon(Icons.Filled.Edit, null)
                    Spacer(Modifier.width(6.dp))
                    Text(stringResource(R.string.settings_touch_edit))
                }
                OutlinedButton(onClick = { confirmReset = true }) {
                    Icon(Icons.Filled.RestartAlt, null)
                    Spacer(Modifier.width(6.dp))
                    Text(stringResource(R.string.reset))
                }
            }
            StatusLine(stringResource(R.string.settings_touch_helpers_note), Level.INFO)
        }

        // ------------------------------------------------- esquema do MGR no toque
        SectionCard(
            stringResource(R.string.controls_scheme_title),
            Icons.Filled.SportsEsports,
            subtitle = stringResource(R.string.controls_scheme_sub),
        ) {
            Text(stringResource(R.string.controls_scheme_text), style = MaterialTheme.typography.bodySmall)
        }

        // --------------------------------------------------------- comando fisico
        SectionCard(stringResource(R.string.settings_physical_pad), Icons.Filled.SportsEsports) {
            SettingSlider(
                title = stringResource(R.string.settings_deadzone),
                value = c.deadzone.toFloat(),
                range = 0f..0.5f,
                label = { v -> "${(v * 100).roundToInt()}%" },
            ) { v -> vm.updateConfig { it.copy(deadzone = v.toDouble()) } }
            SettingSwitch(stringResource(R.string.settings_rumble), checked = c.vibration) { on -> vm.updateConfig { it.copy(vibration = on) } }
        }
    }

    if (confirmReset) {
        AlertDialog(
            onDismissRequest = { confirmReset = false },
            title = { Text(stringResource(R.string.settings_touch_reset_title)) },
            text = { Text(stringResource(R.string.settings_touch_reset_text)) },
            confirmButton = {
                TextButton(onClick = {
                    ControlLayoutStore.reset()
                    confirmReset = false
                }) { Text(stringResource(R.string.reset)) }
            },
            dismissButton = { TextButton(onClick = { confirmReset = false }) { Text(stringResource(R.string.cancel)) } },
        )
    }
}
