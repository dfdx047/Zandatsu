package com.dfdx047.zandatsu.controls

import android.os.Bundle
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.dfdx047.zandatsu.R
import com.dfdx047.zandatsu.data.AppPaths
import com.dfdx047.zandatsu.data.LauncherPrefs
import com.dfdx047.zandatsu.ui.theme.SwordManTheme
import kotlin.math.abs

/** Tela de teste do comando (cada botao e analogico ao vivo), remapeamento e mapeamento automatico. */
class PadMapperActivity : AppCompatActivity() {
    private val pads = mutableStateListOf<InputDevice>()
    private val selected = mutableStateOf<InputDevice?>(null)
    private val keysDown = mutableStateMapOf<Int, Boolean>()
    private val axisValues = mutableStateMapOf<Int, Float>()
    private val draft = mutableStateMapOf<PadTarget, PadSource>()
    private val listening = mutableStateOf<PadTarget?>(null)
    private val status = mutableStateOf("")
    /** Repouso de cada eixo quando a escuta comecou (ha gatilhos que descansam a -1). */
    private val baseline = HashMap<Int, Float>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        AppPaths.init(this)
        val prefs = LauncherPrefs(this)
        refresh()
        setContent {
            SwordManTheme(themeMode = prefs.themeMode, darkMode = com.dfdx047.zandatsu.data.DarkMode.DARK) {
                Surface(Modifier.fillMaxSize()) { Screen() }
            }
        }
    }

    private fun refresh() {
        pads.clear()
        pads.addAll(PadMapper.pads())
        val current = selected.value?.id
        select(pads.firstOrNull { it.id == current } ?: pads.firstOrNull())
    }

    private fun select(d: InputDevice?) {
        selected.value = d
        draft.clear()
        d?.let { draft.putAll(PadMapper.load(it) ?: PadMapper.auto(this, it).first) }
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val d = event.device
        if (d == null || !PadMapper.isPad(d)) return super.dispatchKeyEvent(event)
        // Back sem estar a escutar: sai da tela, mesmo pelo comando.
        if (event.keyCode == KeyEvent.KEYCODE_BACK && listening.value == null) return super.dispatchKeyEvent(event)
        if (selected.value?.id != d.id) select(d)
        keysDown[event.keyCode] = event.action == KeyEvent.ACTION_DOWN
        val target = listening.value
        if (target != null && event.action == KeyEvent.ACTION_DOWN) {
            assign(target, PadSource(key = event.keyCode))
            listening.value = null
        }
        return true
    }

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        if (event.source and InputDevice.SOURCE_JOYSTICK != InputDevice.SOURCE_JOYSTICK) return super.dispatchGenericMotionEvent(event)
        val d = event.device ?: return true
        if (selected.value?.id != d.id) select(d)
        for (r in d.motionRanges) {
            if (r.source and InputDevice.SOURCE_JOYSTICK == 0) continue
            val v = event.getAxisValue(r.axis)
            axisValues[r.axis] = v
            val target = listening.value ?: continue
            val base = baseline.getOrPut(r.axis) { if (r.min < -0.5f && r.axis !in TRIGGERS) 0f else r.min }
            if (abs(v - base) > 0.6f) {
                // D-pad e botoes num eixo guardam o lado; analogicos e gatilhos o eixo inteiro.
                val sign = if (target.isStick || target.isTrigger) 0 else if (v > base) 1 else -1
                assign(target, PadSource(axis = r.axis, sign = sign))
                listening.value = null
                baseline.clear()
            }
        }
        return true
    }

    /** Botao ja usado por outra acao: as duas trocam de lugar, nenhuma fica sem botao. */
    private fun assign(target: PadTarget, source: PadSource) {
        val previous = draft[target]
        val other = draft.entries.firstOrNull { it.key != target && it.value == source }?.key
        draft[target] = source
        if (other != null) {
            if (previous != null) draft[other] = previous else draft.remove(other)
        }
    }

    private fun sourceActive(s: PadSource?): Boolean {
        if (s == null) return false
        if (s.axis < 0) return keysDown[s.key] == true
        val v = axisValues[s.axis] ?: return false
        return when (s.sign) {
            0 -> abs(v) > 0.3f
            else -> v * s.sign > 0.5f
        }
    }

    @Composable
    private fun Screen() {
        Box(Modifier.fillMaxSize()) { Content() }
    }

    @OptIn(ExperimentalLayoutApi::class)
    @Composable
    private fun androidx.compose.foundation.layout.BoxScope.Content() {
        val d = selected.value
        Column(
            Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Text(stringResource(R.string.mapper_title), style = MaterialTheme.typography.titleLarge)
            if (d == null) {
                Text(stringResource(R.string.mapper_none))
                OutlinedButton(onClick = { refresh() }) { Text(stringResource(R.string.saves_refresh)) }
                return@Column
            }
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                for (p in pads) {
                    val on = p.id == d.id
                    if (on) FilledTonalButton(onClick = {}) { Text(p.name) } else OutlinedButton(onClick = { select(p) }) { Text(p.name) }
                }
            }
            Text(
                "%04x:%04x · ".format(d.vendorId, d.productId) +
                    stringResource(if (PadMapper.load(d) != null) R.string.mapper_active else R.string.mapper_default) +
                    " · " + stringResource(if (PadMapper.dbEntry(this@PadMapperActivity, d) != null) R.string.mapper_source_db else R.string.mapper_source_android),
                style = MaterialTheme.typography.bodySmall,
            )
            // Ao vivo: dois analogicos e os gatilhos, pelo mapeamento em edicao.
            Row(horizontalArrangement = Arrangement.spacedBy(24.dp), verticalAlignment = Alignment.CenterVertically) {
                Stick(axisValues[draft[PadTarget.LX]?.axis] ?: 0f, axisValues[draft[PadTarget.LY]?.axis] ?: 0f)
                Stick(axisValues[draft[PadTarget.RX]?.axis] ?: 0f, axisValues[draft[PadTarget.RY]?.axis] ?: 0f)
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    d.motionRanges.filter { it.source and InputDevice.SOURCE_JOYSTICK != 0 }.forEach { r ->
                        Text(
                            MotionEvent.axisToString(r.axis).removePrefix("AXIS_") + " " + "%.2f".format(axisValues[r.axis] ?: 0f),
                            style = MaterialTheme.typography.labelSmall,
                        )
                    }
                }
            }
            FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                for (t in PadTarget.values()) {
                    val src = draft[t]
                    val active = sourceActive(src)
                    Box(
                        Modifier
                            .background(
                                if (active) Color(0xFFE3263A) else MaterialTheme.colorScheme.surfaceVariant,
                                RoundedCornerShape(8.dp),
                            )
                            .clickable { baseline.clear(); listening.value = t }
                            .padding(horizontal = 10.dp, vertical = 6.dp),
                    ) {
                        Column {
                            Text(t.label, style = MaterialTheme.typography.labelLarge)
                            Text(src?.label ?: "—", style = MaterialTheme.typography.labelSmall)
                        }
                    }
                }
            }
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilledTonalButton(onClick = {
                    val (m, _) = PadMapper.auto(this@PadMapperActivity, d)
                    draft.clear()
                    draft.putAll(m)
                }) { Text(stringResource(R.string.mapper_auto)) }
                FilledTonalButton(onClick = {
                    PadMapper.save(d, draft.toMap())
                    status.value = getString(R.string.mapper_saved)
                }) { Text(stringResource(R.string.mapper_save)) }
                OutlinedButton(onClick = {
                    PadMapper.clear(d)
                    select(d)
                    status.value = ""
                }) { Text(stringResource(R.string.mapper_clear)) }
            }
            if (status.value.isNotEmpty()) Text(status.value, style = MaterialTheme.typography.bodySmall)
        }
        // Na mesma janela: um AlertDialog abre outra e as teclas do comando deixam de chegar a esta atividade.
        listening.value?.let { t ->
            Box(Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.7f)), contentAlignment = Alignment.Center) {
                Surface(shape = RoundedCornerShape(16.dp), tonalElevation = 6.dp) {
                    Column(Modifier.padding(24.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                        Text(stringResource(R.string.mapper_press, t.label))
                        TextButton(onClick = { listening.value = null }) { Text(stringResource(R.string.mapper_cancel)) }
                    }
                }
            }
        }
    }

    @Composable
    private fun Stick(x: Float, y: Float) {
        Box(Modifier.size(84.dp).background(MaterialTheme.colorScheme.surfaceVariant, CircleShape)) {
            Box(
                Modifier
                    .padding(start = (32f + x * 32f).dp, top = (32f + y * 32f).dp)
                    .size(20.dp)
                    .background(Color(0xFFE3263A), CircleShape),
            )
        }
    }

    private companion object {
        val TRIGGERS = setOf(MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_RTRIGGER, MotionEvent.AXIS_GAS, MotionEvent.AXIS_BRAKE, MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ)
    }
}
