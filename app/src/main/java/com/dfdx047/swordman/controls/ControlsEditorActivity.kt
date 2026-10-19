@file:OptIn(ExperimentalLayoutApi::class, ExperimentalMaterial3Api::class)

package com.dfdx047.swordman.controls

import android.os.Bundle
import android.view.WindowManager
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.RestartAlt
import androidx.compose.material.icons.filled.Save
import androidx.compose.material.icons.filled.VisibilityOff
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.FilterChip
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import com.dfdx047.swordman.R
import com.dfdx047.swordman.data.AppPaths
import com.dfdx047.swordman.data.LauncherPrefs
import com.dfdx047.swordman.game.TouchControlsView
import com.dfdx047.swordman.ui.theme.SwordManTheme
import kotlin.math.roundToInt

/**
 * Editor do layout dos controles por toque: arrastar botoes, mudar tamanho e
 * opacidade, esconder, adicionar botoes de ajuda, botoes do comando e macros
 * (varios botoes ao mesmo tempo ou uma sequencia).
 */
class ControlsEditorActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        AppPaths.init(this)
        window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN)
        val prefs = LauncherPrefs(this)
        setContent {
            SwordManTheme(themeMode = prefs.themeMode, darkMode = com.dfdx047.swordman.data.DarkMode.DARK) {
                ControlsEditor(prefs) { finish() }
            }
        }
    }
}

/**
 * Editor do layout. Usado pela ControlsEditorActivity (launcher) e por cima do
 * jogo pausado ([overGame] = fundo transparente, ve-se o jogo atras).
 */
@Composable
fun ControlsEditor(
    prefs: LauncherPrefs,
    overGame: Boolean = false,
    initialKind: LayoutKind = LayoutKind.GAMEPLAY,
    close: () -> Unit,
) {
    var kind by remember { mutableStateOf(initialKind) }
    var layout by remember { mutableStateOf(ControlLayoutStore.load(initialKind)) }
    var selected by remember { mutableStateOf<String?>(null) }
    var addMenu by remember { mutableStateOf(false) }
    var dirty by remember { mutableStateOf(false) }

    fun update(id: String, f: (ControlElement) -> ControlElement) {
        layout = layout.copy(elements = layout.elements.map { if (it.id == id) f(it) else it })
        dirty = true
    }

    fun add(e: ControlElement) {
        layout = layout.copy(elements = layout.elements + e)
        selected = e.id
        dirty = true
    }

    Box(Modifier.fillMaxSize().background(if (overGame) Color.Black.copy(alpha = 0.35f) else Color(0xFF0A0B0D))) {
        Text(
            stringResource(R.string.editor_hint),
            color = Color.White.copy(alpha = 0.35f),
            style = MaterialTheme.typography.titleMedium,
            modifier = Modifier.align(Alignment.Center),
        )
        AndroidView(
            factory = { ctx ->
                TouchControlsView(ctx).apply {
                    editMode = true
                    style = prefs.controlStyle
                    directional = prefs.directional
                    badge = prefs.buttonBadge
                    editListener = object : TouchControlsView.EditListener {
                        override fun onSelected(id: String?) {
                            selected = id
                        }

                        override fun onMoved(moved: ControlLayout) {
                            // O View ja tem a posicao nova; guarda-a no estado do editor.
                            layout = moved
                            dirty = true
                        }
                    }
                }
            },
            update = { v ->
                v.layout = layout
                v.selectedId = selected
                v.style = prefs.controlStyle
                v.directional = prefs.directional
                v.badge = prefs.buttonBadge
            },
            modifier = Modifier.fillMaxSize(),
        )
        // Barra de cima
        Surface(
            color = Color.Black.copy(alpha = 0.55f),
            shape = RoundedCornerShape(bottomStart = 16.dp, bottomEnd = 16.dp),
            modifier = Modifier.align(Alignment.TopCenter),
        ) {
            Row(Modifier.padding(horizontal = 8.dp, vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                IconButton(onClick = close) { Icon(Icons.Filled.Close, stringResource(R.string.cancel), tint = Color.White) }
                Text(stringResource(R.string.editor_title), color = Color.White, style = MaterialTheme.typography.titleSmall)
                Spacer(Modifier.width(8.dp))
                // Layout de combate, dos menus ou o comando completo.
                listOf(
                    LayoutKind.GAMEPLAY to stringResource(R.string.settings_touch_layout_battle),
                    LayoutKind.MENU to stringResource(R.string.settings_touch_layout_menu),
                    LayoutKind.FULL to stringResource(R.string.settings_touch_layout_map),
                ).forEach { (k, label) ->
                    FilterChip(
                        selected = kind == k,
                        onClick = {
                            if (kind != k) {
                                if (dirty) ControlLayoutStore.save(layout)
                                kind = k
                                layout = ControlLayoutStore.load(k)
                                selected = null
                                dirty = false
                            }
                        },
                        label = { Text(label) },
                        modifier = Modifier.padding(end = 4.dp),
                    )
                }
                Spacer(Modifier.width(4.dp))
                Box {
                    TextButton(onClick = { addMenu = true }) {
                        Icon(Icons.Filled.Add, null)
                        Text(stringResource(R.string.editor_add))
                    }
                    AddMenu(addMenu, layout, onDismiss = { addMenu = false }) { e ->
                        addMenu = false
                        val existing = layout.elements.firstOrNull { it.id == e.id }
                        if (existing != null) update(e.id) { it.copy(visible = true) } else add(e)
                    }
                }
                TextButton(onClick = {
                    layout = GameActions.defaultLayout(kind)
                    selected = null
                    dirty = true
                }) {
                    Icon(Icons.Filled.RestartAlt, null)
                    Text(stringResource(R.string.reset))
                }
                TextButton(onClick = {
                    ControlLayoutStore.save(layout)
                    dirty = false
                    close()
                }) {
                    Icon(Icons.Filled.Save, null)
                    Text(stringResource(R.string.save) + if (dirty) " •" else "")
                }
            }
        }

        // Painel do botao selecionado
        val sel = layout.elements.firstOrNull { it.id == selected }
        if (sel != null) {
            Surface(
                color = MaterialTheme.colorScheme.surfaceContainerHigh.copy(alpha = 0.95f),
                shape = RoundedCornerShape(18.dp),
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .padding(8.dp)
                    .widthIn(max = 560.dp)
                    .heightIn(max = 300.dp),
            ) {
                ElementPanel(
                    sel,
                    onChange = { f -> update(sel.id, f) },
                    onHide = {
                        update(sel.id) { it.copy(visible = false) }
                        selected = null
                    },
                    onDelete = {
                        layout = layout.copy(elements = layout.elements.filter { it.id != sel.id })
                        selected = null
                        dirty = true
                    },
                )
            }
        }
    }
}

@Composable
private fun AddMenu(open: Boolean, layout: ControlLayout, onDismiss: () -> Unit, onAdd: (ControlElement) -> Unit) {
    val present = layout.elements.filter { it.visible }.map { it.id }.toSet()
    val helperNames = mapOf(
        "dodge" to R.string.helper_dodge,
        "ripper" to R.string.helper_ripper,
        "augment" to R.string.helper_augment,
        "item" to R.string.helper_item,
        "weapon_l" to R.string.helper_weapon_l,
        "weapon_r" to R.string.helper_weapon_r,
        "subweapon" to R.string.helper_subweapon,
        "center_cam" to R.string.helper_center_cam,
        "menu" to R.string.helper_menu,
    )
    DropdownMenu(expanded = open, onDismissRequest = onDismiss) {
        Text(stringResource(R.string.editor_helpers), style = MaterialTheme.typography.labelMedium, modifier = Modifier.padding(horizontal = 12.dp))
        GameActions.HELPERS.filter { it.first !in present }.forEach { (id, action, icon) ->
            DropdownMenuItem(
                text = { Text(stringResource(helperNames[id] ?: R.string.editor_macro)) },
                onClick = { onAdd(ControlElement(id, ElementKind.BUTTON, action, icon, id.uppercase(), 0.5f, 0.5f, 52f)) },
            )
        }
        // Elementos escondidos voltam a aparecer.
        layout.elements.filter { !it.visible && GameActions.HELPERS.none { h -> h.first == it.id } }.forEach { e ->
            DropdownMenuItem(
                text = { Text(stringResource(R.string.editor_show, e.label.ifBlank { e.id })) },
                onClick = { onAdd(e.copy(visible = true)) },
            )
        }
        HorizontalDivider()
        Text(stringResource(R.string.editor_pad_buttons), style = MaterialTheme.typography.labelMedium, modifier = Modifier.padding(horizontal = 12.dp))
        listOf(PadInput.A, PadInput.B, PadInput.X, PadInput.Y, PadInput.LB, PadInput.RB, PadInput.LT, PadInput.RT,
            PadInput.BACK, PadInput.START, PadInput.L3, PadInput.R3).forEach { input ->
            DropdownMenuItem(
                text = { Text(GameActions.classicLabel(input, false)) },
                onClick = {
                    val id = "pad_${input.name.lowercase()}_${System.currentTimeMillis() % 100000}"
                    onAdd(ControlElement(id, ElementKind.BUTTON, ControlAction.Press(setOf(input)), ControlIcon.MACRO,
                        GameActions.classicLabel(input, false), 0.5f, 0.5f, 52f))
                },
            )
        }
        HorizontalDivider()
        DropdownMenuItem(
            text = { Text(stringResource(R.string.editor_new_macro)) },
            onClick = {
                val id = "macro_${System.currentTimeMillis() % 100000}"
                onAdd(ControlElement(id, ElementKind.BUTTON, ControlAction.Press(setOf(PadInput.X, PadInput.Y)), ControlIcon.MACRO,
                    "M", 0.5f, 0.5f, 52f))
            },
        )
    }
}

@Composable
private fun ElementPanel(
    e: ControlElement,
    onChange: ((ControlElement) -> ControlElement) -> Unit,
    onHide: () -> Unit,
    onDelete: () -> Unit,
) {
    Column(
        Modifier
            .padding(12.dp)
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(e.label.ifBlank { e.id }, style = MaterialTheme.typography.titleSmall, modifier = Modifier.weight(1f))
            IconButton(onClick = onHide) { Icon(Icons.Filled.VisibilityOff, stringResource(R.string.editor_hide)) }
            val custom = e.id.startsWith("macro_") || e.id.startsWith("pad_") || GameActions.HELPERS.any { it.first == e.id }
            if (custom) IconButton(onClick = onDelete) { Icon(Icons.Filled.Delete, stringResource(R.string.delete)) }
        }
        val sizeRange = if (e.kind == ElementKind.LOOK) 120f..600f else 28f..220f
        LabeledSlider(stringResource(R.string.editor_size), e.sizeDp.coerceIn(sizeRange), sizeRange, "${e.sizeDp.roundToInt()} dp") { v ->
            onChange { it.copy(sizeDp = v) }
        }
        LabeledSlider(stringResource(R.string.editor_opacity), e.opacity, 0.1f..1f, "${(e.opacity * 100).roundToInt()}%") { v ->
            onChange { it.copy(opacity = v) }
        }
        if (e.id.startsWith("macro_")) MacroEditor(e, onChange)
    }
}

@Composable
private fun LabeledSlider(title: String, value: Float, range: ClosedFloatingPointRange<Float>, label: String, onChange: (Float) -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Text(title, style = MaterialTheme.typography.bodyMedium, modifier = Modifier.width(90.dp))
        Slider(value = value, onValueChange = onChange, valueRange = range, modifier = Modifier.weight(1f))
        Text(label, style = MaterialTheme.typography.labelMedium, modifier = Modifier.width(56.dp).padding(start = 6.dp))
    }
}

private val MACRO_INPUTS = listOf(
    PadInput.X, PadInput.Y, PadInput.A, PadInput.B, PadInput.LB, PadInput.RB, PadInput.LT, PadInput.RT,
    PadInput.UP, PadInput.DOWN, PadInput.LEFT, PadInput.RIGHT, PadInput.L3, PadInput.R3,
)

@Composable
private fun InputChips(selected: Set<PadInput>, onToggle: (PadInput) -> Unit) {
    FlowRow(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        MACRO_INPUTS.forEach { i ->
            FilterChip(
                selected = i in selected,
                onClick = { onToggle(i) },
                label = { Text(inputName(i)) },
            )
        }
    }
}

@Composable
private fun inputName(i: PadInput): String = when (i) {
    PadInput.X -> stringResource(R.string.input_light)
    PadInput.Y -> stringResource(R.string.input_heavy)
    PadInput.A -> stringResource(R.string.input_jump)
    PadInput.B -> stringResource(R.string.input_action)
    else -> GameActions.classicLabel(i, false)
}

/** Macro: combinacao (tudo ao mesmo tempo) ou sequencia de passos. */
@Composable
private fun MacroEditor(e: ControlElement, onChange: ((ControlElement) -> ControlElement) -> Unit) {
    val action = e.action
    val isSequence = action is ControlAction.Sequence
    var labelText by remember(e.id) { mutableStateOf(e.label) }
    Row(verticalAlignment = Alignment.CenterVertically) {
        Text(stringResource(R.string.editor_label), modifier = Modifier.width(90.dp))
        listOf("M", "M1", "M2", "SP", "EX", "Z").forEach { l ->
            FilterChip(selected = labelText == l, onClick = {
                labelText = l
                onChange { it.copy(label = l) }
            }, label = { Text(l) }, modifier = Modifier.padding(end = 4.dp))
        }
    }
    SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
        SegmentedButton(
            selected = !isSequence,
            onClick = {
                if (isSequence) {
                    val all = (action as ControlAction.Sequence).steps.flatMap { it.inputs }.toSet()
                    onChange { it.copy(action = ControlAction.Press(all)) }
                }
            },
            shape = SegmentedButtonDefaults.itemShape(0, 2),
        ) { Text(stringResource(R.string.editor_combo)) }
        SegmentedButton(
            selected = isSequence,
            onClick = {
                if (!isSequence) {
                    val inputs = (action as ControlAction.Press).inputs
                    onChange { it.copy(action = ControlAction.Sequence(listOf(MacroStep(inputs, 3)), holdLast = false)) }
                }
            },
            shape = SegmentedButtonDefaults.itemShape(1, 2),
        ) { Text(stringResource(R.string.editor_sequence)) }
    }
    when (action) {
        is ControlAction.Press -> {
            Text(stringResource(R.string.editor_combo_help), style = MaterialTheme.typography.bodySmall)
            InputChips(action.inputs) { i ->
                val next = if (i in action.inputs) action.inputs - i else action.inputs + i
                onChange { it.copy(action = ControlAction.Press(next)) }
            }
        }
        is ControlAction.Sequence -> {
            Text(stringResource(R.string.editor_sequence_help), style = MaterialTheme.typography.bodySmall)
            action.steps.forEachIndexed { index, step ->
                Surface(shape = RoundedCornerShape(12.dp), color = MaterialTheme.colorScheme.surfaceContainer) {
                    Column(Modifier.padding(8.dp)) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text(stringResource(R.string.editor_step, index + 1), style = MaterialTheme.typography.labelLarge, modifier = Modifier.weight(1f))
                            IconButton(onClick = {
                                val steps = action.steps.filterIndexed { j, _ -> j != index }
                                onChange { it.copy(action = action.copy(steps = steps.ifEmpty { listOf(MacroStep(emptySet(), 3)) })) }
                            }) { Icon(Icons.Filled.Delete, stringResource(R.string.delete)) }
                        }
                        InputChips(step.inputs) { i ->
                            val inputs = if (i in step.inputs) step.inputs - i else step.inputs + i
                            val steps = action.steps.mapIndexed { j, s -> if (j == index) s.copy(inputs = inputs) else s }
                            onChange { it.copy(action = action.copy(steps = steps)) }
                        }
                        LabeledSlider(stringResource(R.string.editor_frames), step.frames.toFloat(), 1f..60f, "${step.frames}f") { v ->
                            val steps = action.steps.mapIndexed { j, s -> if (j == index) s.copy(frames = v.roundToInt()) else s }
                            onChange { it.copy(action = action.copy(steps = steps)) }
                        }
                    }
                }
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                OutlinedButton(onClick = {
                    onChange { it.copy(action = action.copy(steps = action.steps + MacroStep(emptySet(), 3))) }
                }, enabled = action.steps.size < 16) {
                    Icon(Icons.Filled.Add, null)
                    Text(stringResource(R.string.editor_add_step))
                }
                Spacer(Modifier.weight(1f))
                Text(stringResource(R.string.editor_hold_last), style = MaterialTheme.typography.bodySmall)
                Switch(checked = action.holdLast, onCheckedChange = { h -> onChange { it.copy(action = action.copy(holdLast = h)) } })
            }
        }
    }
    Text(
        stringResource(R.string.editor_macro_note),
        style = MaterialTheme.typography.bodySmall,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
}
