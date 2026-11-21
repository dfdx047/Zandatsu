package com.dfdx047.zandatsu.pc.ui.screens

import android.content.Intent
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.Gamepad
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material.icons.filled.OpenWith
import androidx.compose.material.icons.filled.RestartAlt
import androidx.compose.material.icons.filled.SportsEsports
import androidx.compose.material.icons.filled.TouchApp
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringArrayResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.dfdx047.zandatsu.pc.R
import com.dfdx047.zandatsu.pc.controls.ControlLayoutStore
import com.dfdx047.zandatsu.pc.controls.ControlsEditorActivity
import com.dfdx047.zandatsu.pc.controls.PadMapperActivity
import com.dfdx047.zandatsu.pc.data.ButtonBadge
import com.dfdx047.zandatsu.pc.data.ControlStyle
import com.dfdx047.zandatsu.pc.data.Directional
import com.dfdx047.zandatsu.pc.data.LauncherPrefs
import com.dfdx047.zandatsu.pc.data.PcTouchMode
import com.dfdx047.zandatsu.pc.ui.components.ButtonRow
import com.dfdx047.zandatsu.pc.ui.components.ChoiceRow
import com.dfdx047.zandatsu.pc.ui.components.Level
import com.dfdx047.zandatsu.pc.ui.components.SectionCard
import com.dfdx047.zandatsu.pc.ui.components.SettingDropdown
import com.dfdx047.zandatsu.pc.ui.components.SettingSegmented
import com.dfdx047.zandatsu.pc.ui.components.SettingSlider
import com.dfdx047.zandatsu.pc.ui.components.SettingSwitch
import com.dfdx047.zandatsu.pc.ui.components.StatusLine
import kotlin.math.roundToInt

/**
 * Controles (a aba da launcher do 360 com o modo de toque do PC): modo Botoes / Gestos / Mouse com a explicacao
 * animada de cada um, os controles nos menus do jogo, os controles na tela (estilo, layout, perfis) e o comando fisico.
 */
@Composable
fun ControlsScreen() {
    val context = LocalContext.current
    val prefs = remember { LauncherPrefs(context) }
    var mode by remember { mutableStateOf(prefs.pcTouchMode) }
    var confirmReset by remember { mutableStateOf(false) }

    Column(
        Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        // ------------------------------------------------------------------------------------- modo de toque
        SectionCard(stringResource(R.string.ctl_mode), Icons.Filled.TouchApp, subtitle = stringResource(R.string.ctl_mode_card_sub)) {
            ChoiceRow(
                listOf(
                    PcTouchMode.BUTTONS to stringResource(R.string.ctl_mode_buttons),
                    PcTouchMode.GESTURES to stringResource(R.string.ctl_mode_gestures),
                    PcTouchMode.MOUSE to stringResource(R.string.ctl_mode_mouse),
                ),
                mode,
            ) { mode = it; prefs.pcTouchMode = it }
            Text(
                stringResource(when (mode) {
                    PcTouchMode.BUTTONS -> R.string.ctl_mode_buttons_sub
                    PcTouchMode.GESTURES -> R.string.ctl_mode_gestures_sub
                    PcTouchMode.MOUSE -> R.string.ctl_mode_mouse_sub
                }),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            when (mode) {
                PcTouchMode.BUTTONS -> TouchDemo(DemoScene.BUTTONS, TouchDemos.buttons)
                PcTouchMode.GESTURES -> TouchDemo(DemoScene.GESTURES, TouchDemos.gestures)
                PcTouchMode.MOUSE -> TouchDemo(DemoScene.MOUSE, TouchDemos.mouse)
            }
            StatusLine(stringResource(R.string.ctl_pad_hint), Level.INFO)
        }

        // ------------------------------------------------------------------------------ nos menus do jogo
        SectionCard(stringResource(R.string.ctl_menus_title), Icons.Filled.Menu, subtitle = stringResource(R.string.ctl_menus_sub)) {
            TouchDemo(DemoScene.MENU, TouchDemos.menu)
            Text(
                stringResource(when (mode) {
                    PcTouchMode.BUTTONS -> R.string.ctl_menus_buttons_note
                    PcTouchMode.GESTURES -> R.string.ctl_menus_gestures_note
                    PcTouchMode.MOUSE -> R.string.ctl_menus_mouse_note
                }),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }

        // ------------------------------------------------------------------------------ controles na tela
        if (mode == PcTouchMode.BUTTONS) {
            var directional by remember { mutableStateOf(prefs.directional) }
            var style by remember { mutableStateOf(prefs.controlStyle) }
            var badge by remember { mutableStateOf(prefs.buttonBadge) }
            var opacity by remember { mutableStateOf(prefs.touchOpacity) }
            var scale by remember { mutableStateOf(prefs.touchScale) }
            var blade by remember { mutableStateOf(prefs.bladeToggle) }
            var haptics by remember { mutableStateOf(prefs.touchHaptics) }
            SectionCard(stringResource(R.string.ctl_onscreen), Icons.Filled.Gamepad, subtitle = stringResource(R.string.ctl_onscreen_sub)) {
                SettingSegmented(
                    title = stringResource(R.string.ctl_directional),
                    options = listOf(Directional.DPAD to stringResource(R.string.ctl_dpad), Directional.STICK to stringResource(R.string.ctl_stick)),
                    selected = directional,
                ) { directional = it; prefs.directional = it }
                SettingDropdown(
                    title = stringResource(R.string.ctl_style),
                    options = listOf(
                        ControlStyle.GAME to stringResource(R.string.ctl_style_classic),
                        ControlStyle.SCIFI to "Sci-fi",
                        ControlStyle.XBOX to "Xbox",
                        ControlStyle.PLAYSTATION to "PS",
                        ControlStyle.NINTENDO to "Nintendo",
                    ),
                    selected = style,
                ) { style = it; prefs.controlStyle = it }
                if (style.iconic) {
                    SettingSegmented(
                        title = stringResource(R.string.ctl_badge),
                        options = listOf(
                            ButtonBadge.XBOX to "Xbox",
                            ButtonBadge.PLAYSTATION to "PS",
                            ButtonBadge.NINTENDO to "Nintendo",
                            ButtonBadge.NONE to stringResource(R.string.ctl_badge_none),
                        ),
                        selected = badge,
                    ) { badge = it; prefs.buttonBadge = it }
                }
                SettingSlider(stringResource(R.string.ctl_opacity_title), opacity, 0.1f..1f, label = { v -> "${(v * 100).roundToInt()}%" }) { v ->
                    opacity = v; prefs.touchOpacity = v
                }
                SettingSlider(stringResource(R.string.ctl_scale_title), scale, 0.5f..1.6f, label = { v -> "${(v * 100).roundToInt()}%" }) { v ->
                    scale = v; prefs.touchScale = v
                }
                SettingSegmented(
                    title = stringResource(R.string.ctl_blade_mode),
                    subtitle = stringResource(R.string.ctl_blade_toggle_sub),
                    options = listOf(true to stringResource(R.string.ctl_blade_toggle_opt), false to stringResource(R.string.ctl_blade_hold_opt)),
                    selected = blade,
                ) { blade = it; prefs.bladeToggle = it }
                SettingSwitch(stringResource(R.string.ctl_haptics), checked = haptics) { haptics = it; prefs.touchHaptics = it }
                ButtonRow {
                    FilledTonalButton(onClick = { context.startActivity(Intent(context, ControlsEditorActivity::class.java)) }) {
                        Icon(Icons.Filled.Edit, null)
                        Spacer(Modifier.width(6.dp))
                        Text(stringResource(R.string.ctl_edit_layout))
                    }
                    OutlinedButton(onClick = { confirmReset = true }) {
                        Icon(Icons.Filled.RestartAlt, null)
                        Spacer(Modifier.width(6.dp))
                        Text(stringResource(R.string.reset))
                    }
                }
                StatusLine(stringResource(R.string.ctl_edit_hint), Level.INFO)
                LayoutProfiles()
            }

            SectionCard(stringResource(R.string.controls_scheme_title), Icons.Filled.SportsEsports, subtitle = stringResource(R.string.controls_scheme_sub)) {
                ControlsLegend()
            }
        }

        // ---------------------------------------------------------------------------------------- geral
        SectionCard(stringResource(R.string.ctl_general), Icons.Filled.Menu) {
            var pause by remember { mutableStateOf(prefs.pauseOnMenus) }
            SettingSwitch(stringResource(R.string.ctl_pause_menu), stringResource(R.string.ctl_pause_menu_sub), pause) {
                pause = it; prefs.pauseOnMenus = it
            }
        }

        // --------------------------------------------------------------------------------- comando fisico
        SectionCard(stringResource(R.string.settings_physical_pad), Icons.Filled.SportsEsports, subtitle = stringResource(R.string.ctl_physical_sub)) {
            FilledTonalButton(onClick = { context.startActivity(Intent(context, PadMapperActivity::class.java)) }) {
                Icon(Icons.Filled.SportsEsports, null)
                Spacer(Modifier.width(6.dp))
                Text(stringResource(R.string.mapper_open))
            }
            Text(stringResource(R.string.ctl_physical_note), style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant)
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
                    prefs.touchScale = 1f
                    prefs.touchOpacity = 0.75f
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
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
        OutlinedTextField(
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
        }) { Text(stringResource(R.string.save)) }
    }
    for (n in names) {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
            Text(n, modifier = Modifier.weight(1f))
            OutlinedButton(onClick = { ControlLayoutStore.loadProfile(n) }) { Text(stringResource(R.string.profiles_load)) }
            TextButton(onClick = {
                ControlLayoutStore.deleteProfile(n)
                names = ControlLayoutStore.profiles()
            }) { Text(stringResource(R.string.delete)) }
        }
    }
}

/**
 * Legenda dos botoes por toque (copia da app do 360): o icone que aparece no jogo, o botao do comando e o que faz. Os
 * textos vem de R.array.controls_legend ("Nome|descricao"), pela ordem de LEGEND.
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
    val texts = stringArrayResource(R.array.controls_legend)
    BoxWithConstraints(Modifier.fillMaxWidth()) {
        val cols = (maxWidth / 112.dp).toInt().coerceIn(2, 6)
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            LEGEND.indices.chunked(cols).forEach { row ->
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.height(IntrinsicSize.Max)) {
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
    val shape = RoundedCornerShape(12.dp)
    Column(
        modifier.background(scheme.surfaceContainerHigh, shape).padding(horizontal = 6.dp, vertical = 8.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Box(
            Modifier
                .size(34.dp)
                .background(scheme.surfaceContainerHighest, CircleShape)
                .border(1.dp, scheme.outlineVariant, CircleShape),
            contentAlignment = Alignment.Center,
        ) {
            if (icon != null) {
                Icon(painterResource(icon), null, tint = scheme.onSurface, modifier = Modifier.size(19.dp))
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
            modifier = Modifier.background(scheme.surfaceVariant, RoundedCornerShape(6.dp)).padding(horizontal = 6.dp, vertical = 1.dp),
        )
        Spacer(Modifier.height(4.dp))
        Text(name, style = MaterialTheme.typography.labelMedium, textAlign = TextAlign.Center, maxLines = 2)
        if (desc.isNotEmpty()) {
            Text(
                desc,
                style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Normal),
                color = scheme.onSurfaceVariant,
                textAlign = TextAlign.Center,
            )
        }
    }
}
