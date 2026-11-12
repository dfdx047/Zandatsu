package com.dfdx047.zandatsu.ui.screens

import androidx.compose.foundation.layout.fillMaxWidth

import androidx.compose.ui.Alignment

import androidx.compose.material.icons.filled.OpenWith

import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.fillMaxHeight

import androidx.compose.foundation.border

import androidx.compose.foundation.background

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
import com.dfdx047.zandatsu.R
import com.dfdx047.zandatsu.controls.ControlLayoutStore
import com.dfdx047.zandatsu.controls.ControlsEditorActivity
import com.dfdx047.zandatsu.data.ButtonBadge
import com.dfdx047.zandatsu.data.ControlStyle
import com.dfdx047.zandatsu.data.Directional
import com.dfdx047.zandatsu.data.TouchMode
import com.dfdx047.zandatsu.ui.LauncherViewModel
import com.dfdx047.zandatsu.ui.components.Level
import com.dfdx047.zandatsu.ui.components.SectionCard
import com.dfdx047.zandatsu.ui.components.SettingDropdown
import com.dfdx047.zandatsu.ui.components.SettingSegmented
import com.dfdx047.zandatsu.ui.components.SettingSlider
import com.dfdx047.zandatsu.ui.components.SettingSwitch
import com.dfdx047.zandatsu.ui.components.StatusLine
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
            SettingDropdown(
                title = stringResource(R.string.settings_touch_style),
                options = listOf(
                    ControlStyle.GAME to stringResource(R.string.ctl_style_classic),
                    ControlStyle.SCIFI to stringResource(R.string.ctl_style_scifi),
                    ControlStyle.XBOX to "Xbox",
                    ControlStyle.PLAYSTATION to "PS",
                    ControlStyle.NINTENDO to "Nintendo",
                ),
                selected = vm.controlStyle,
                onSelect = vm::setControlStyle,
            )
            if (vm.controlStyle.iconic) {
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
            SettingSlider(
                title = stringResource(R.string.settings_touch_scale),
                value = vm.touchScale,
                range = 0.5f..1.6f,
                label = { v -> "${(v * 100).roundToInt()}%" },
            ) { v -> vm.setTouchScale(v) }
            SettingSwitch(
                stringResource(R.string.settings_auto_switch),
                stringResource(R.string.settings_auto_switch_sub),
                checked = vm.touchAutoSwitch,
                onChange = vm::setTouchAutoSwitch,
            )
            SettingSegmented(
                title = stringResource(R.string.settings_blade_mode),
                subtitle = stringResource(R.string.settings_blade_mode_sub),
                options = listOf(
                    true to stringResource(R.string.blade_toggle),
                    false to stringResource(R.string.blade_hold),
                ),
                selected = vm.bladeToggle,
                onSelect = vm::setBladeToggle,
            )
            SettingSwitch(stringResource(R.string.settings_touch_haptics), checked = vm.touchHaptics, onChange = vm::setTouchHaptics)
            SettingSwitch(
                stringResource(R.string.settings_pause_menus),
                stringResource(R.string.settings_pause_menus_sub),
                checked = vm.pauseOnMenus,
                onChange = vm::setPauseOnMenus,
            )
            com.dfdx047.zandatsu.ui.components.ButtonRow {
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
            LayoutProfiles()
        }

        // ------------------------------------------------- esquema do MGR no toque
        SectionCard(
            stringResource(R.string.controls_scheme_title),
            Icons.Filled.SportsEsports,
            subtitle = stringResource(R.string.controls_scheme_sub),
        ) {
            ControlsLegend()
        }

        // --------------------------------------------------------- comando fisico
        SectionCard(stringResource(R.string.settings_physical_pad), Icons.Filled.SportsEsports) {
            SettingSlider(
                title = stringResource(R.string.settings_deadzone),
                value = c.deadzone.toFloat(),
                range = 0f..0.5f,
                label = { v -> "${(v * 100).roundToInt()}%" },
            ) { v -> vm.updateConfig { it.copy(deadzone = v.toDouble()) } }
            FilledTonalButton(onClick = { context.startActivity(Intent(context, com.dfdx047.zandatsu.controls.PadMapperActivity::class.java)) }) {
                Icon(Icons.Filled.SportsEsports, null)
                Spacer(Modifier.width(6.dp))
                Text(stringResource(R.string.mapper_open))
            }
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
                    vm.setTouchScale(1f)
                    vm.setTouchOpacity(0.7f)
                    confirmReset = false
                }) { Text(stringResource(R.string.reset)) }
            },
            dismissButton = { TextButton(onClick = { confirmReset = false }) { Text(stringResource(R.string.cancel)) } },
        )
    }
}

/** Perfis de layout: guarda os tres layouts atuais com um nome e volta a eles. */
@Composable
private fun LayoutProfiles() {
    var names by remember { mutableStateOf(ControlLayoutStore.profiles()) }
    var name by remember { mutableStateOf("") }
    Text(stringResource(R.string.profiles_title), style = MaterialTheme.typography.titleSmall)
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        androidx.compose.material3.OutlinedTextField(
            value = name,
            onValueChange = { name = it.take(40) },
            singleLine = true,
            label = { Text(stringResource(R.string.profiles_name)) },
            modifier = Modifier.weight(1f),
        )
        FilledTonalButton(enabled = name.isNotBlank(), onClick = {
            ControlLayoutStore.saveProfile(name)
            names = ControlLayoutStore.profiles()
            name = ""
        }) { Text(stringResource(R.string.profiles_save)) }
    }
    for (n in names) {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(n, modifier = Modifier.weight(1f).padding(top = 10.dp))
            OutlinedButton(onClick = { ControlLayoutStore.loadProfile(n) }) { Text(stringResource(R.string.profiles_load)) }
            TextButton(onClick = {
                ControlLayoutStore.deleteProfile(n)
                names = ControlLayoutStore.profiles()
            }) { Text(stringResource(R.string.profiles_delete)) }
        }
    }
}


/**
 * Legenda dos botoes por toque (12/11; antes um paragrafo corrido): o icone que aparece no jogo, o botao do comando e
 * o que faz. Os textos vem de R.array.controls_legend ("Nome|descricao"), pela ordem de LEGEND.
 */
private val LEGEND: List<Pair<Int?, String>> = listOf(
    null to "L",
    R.drawable.ic_btn_camera to "R",
    R.drawable.ic_btn_light to "X",
    R.drawable.ic_btn_heavy to "Y",
    R.drawable.ic_btn_jump to "A",
    R.drawable.ic_btn_action to "B",
    R.drawable.ic_btn_blade to "LT",
    R.drawable.ic_btn_run to "RT",
    R.drawable.ic_btn_lock to "RB",
    R.drawable.ic_btn_subweapon to "LB",
    R.drawable.ic_btn_dodge to "X+A",
    R.drawable.ic_btn_ripper to "L3+R3",
    R.drawable.ic_btn_augment to "↑",
    R.drawable.ic_btn_swap to "← →",
    R.drawable.ic_btn_item to "↓",
    R.drawable.ic_btn_start to "Start",
    R.drawable.ic_btn_codec to "Back",
)

@Composable
private fun ControlsLegend() {
    val texts = androidx.compose.ui.res.stringArrayResource(R.array.controls_legend)
    // 12/11: grade de cartoes lado a lado (icone, botao, nome e descricao curta), ~3 colunas em retrato, 5-6 deitado.
    androidx.compose.foundation.layout.BoxWithConstraints(Modifier.fillMaxWidth()) {
        val cols = (maxWidth / 112.dp).toInt().coerceIn(2, 6)
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            LEGEND.indices.chunked(cols).forEach { row ->
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.height(androidx.compose.foundation.layout.IntrinsicSize.Max)) {
                    row.forEach { i ->
                        val (icon, pad) = LEGEND[i]
                        val parts = texts.getOrElse(i) { "" }.split('|', limit = 2)
                        LegendTile(icon, pad, parts[0], parts.getOrElse(1) { "" }, Modifier.weight(1f).fillMaxHeight())
                    }
                    repeat(cols - row.size) { Spacer(Modifier.weight(1f)) }
                }
            }
        }
    }
}

@Composable
private fun LegendTile(icon: Int?, pad: String, name: String, desc: String, modifier: Modifier) {
    val scheme = MaterialTheme.colorScheme
    val shape = androidx.compose.foundation.shape.RoundedCornerShape(12.dp)
    Column(
        modifier
            .background(scheme.surfaceContainerHigh, shape)
            .padding(horizontal = 6.dp, vertical = 8.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        androidx.compose.foundation.layout.Box(
            Modifier
                .size(34.dp)
                .background(scheme.surfaceContainerHighest, androidx.compose.foundation.shape.CircleShape)
                .border(1.dp, scheme.outlineVariant, androidx.compose.foundation.shape.CircleShape),
            contentAlignment = Alignment.Center,
        ) {
            if (icon != null) {
                Icon(androidx.compose.ui.res.painterResource(icon), null, tint = scheme.onSurface, modifier = Modifier.size(19.dp))
            } else {
                Icon(Icons.Filled.OpenWith, null, tint = scheme.onSurface, modifier = Modifier.size(19.dp))
            }
        }
        Spacer(Modifier.height(5.dp))
        Text(
            pad,
            style = MaterialTheme.typography.labelSmall,
            color = padColor(pad) ?: scheme.onSurfaceVariant,
            softWrap = false,
            modifier = Modifier
                .background(scheme.surfaceVariant, androidx.compose.foundation.shape.RoundedCornerShape(6.dp))
                .padding(horizontal = 6.dp, vertical = 1.dp),
        )
        Spacer(Modifier.height(4.dp))
        Text(
            name,
            style = MaterialTheme.typography.labelMedium,
            textAlign = androidx.compose.ui.text.style.TextAlign.Center,
            maxLines = 2,
        )
        if (desc.isNotEmpty()) {
            Text(
                desc,
                style = MaterialTheme.typography.labelSmall.copy(fontWeight = androidx.compose.ui.text.font.FontWeight.Normal),
                color = scheme.onSurfaceVariant,
                textAlign = androidx.compose.ui.text.style.TextAlign.Center,
            )
        }
    }
}

/** As cores dos botoes do comando de Xbox, como nos botoes por toque. */
private fun padColor(pad: String): androidx.compose.ui.graphics.Color? = when (pad) {
    "A" -> androidx.compose.ui.graphics.Color(0xFF5DBB46)
    "B" -> androidx.compose.ui.graphics.Color(0xFFE5483C)
    "X" -> androidx.compose.ui.graphics.Color(0xFF3B8CE8)
    "Y" -> androidx.compose.ui.graphics.Color(0xFFF2C230)
    else -> null
}
