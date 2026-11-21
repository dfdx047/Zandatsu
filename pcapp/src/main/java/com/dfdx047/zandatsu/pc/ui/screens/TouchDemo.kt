package com.dfdx047.zandatsu.pc.ui.screens

import androidx.annotation.StringRes
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.TextMeasurer
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.dfdx047.zandatsu.pc.R
import com.dfdx047.zandatsu.pc.ui.theme.DfColors
import kotlin.math.PI
import kotlin.math.sin

/** O que o dedo faz num passo da demonstracao. */
enum class DemoKind { TAP, LONG, DRAG, HOLD }

/** Ecra esquematico de cada explicacao. */
enum class DemoScene { BUTTONS, GESTURES, MOUSE, MENU }

/**
 * Um passo: legenda, o botao do comando que sai (badge), o gesto, onde os dedos poisam (0..1 do ecra), o fim do
 * arrasto, a metade do ecra que conta (1 esquerda, 2 direita) e, no ecra dos botoes, o botao tocado.
 */
class DemoStep(
    @StringRes val caption: Int,
    val pad: String,
    val kind: DemoKind,
    val fingers: List<Offset>,
    val to: Offset? = null,
    val region: Int = 0,
    val target: String? = null,
)

private fun o(x: Float, y: Float) = Offset(x, y)

/** Passos de cada modo de toque (os gestos sao os do nativo, pc/android/mgr_android_main.cpp). */
object TouchDemos {
    val buttons = listOf(
        DemoStep(R.string.demo_btn_stick, "L", DemoKind.DRAG, listOf(o(0.17f, 0.66f)), to = o(0.24f, 0.55f), target = "stick"),
        DemoStep(R.string.demo_btn_x, "X", DemoKind.TAP, listOf(o(0.77f, 0.62f)), target = "X"),
        DemoStep(R.string.demo_btn_y, "Y", DemoKind.TAP, listOf(o(0.85f, 0.46f)), target = "Y"),
        DemoStep(R.string.demo_btn_a, "A", DemoKind.TAP, listOf(o(0.85f, 0.78f)), target = "A"),
        DemoStep(R.string.demo_btn_lt, "LT", DemoKind.HOLD, listOf(o(0.09f, 0.24f)), target = "LT"),
        DemoStep(R.string.demo_btn_camera, "R", DemoKind.DRAG, listOf(o(0.52f, 0.42f)), to = o(0.64f, 0.34f)),
        DemoStep(R.string.demo_btn_click, "◉", DemoKind.TAP, listOf(o(0.48f, 0.2f))),
    )
    val gestures = listOf(
        DemoStep(R.string.demo_gest_move, "L", DemoKind.DRAG, listOf(o(0.2f, 0.64f)), to = o(0.3f, 0.48f), region = 1),
        DemoStep(R.string.demo_gest_light, "X", DemoKind.TAP, listOf(o(0.74f, 0.56f)), region = 2),
        DemoStep(R.string.demo_gest_strong, "Y", DemoKind.LONG, listOf(o(0.74f, 0.56f)), region = 2),
        DemoStep(R.string.demo_gest_camera, "R", DemoKind.DRAG, listOf(o(0.64f, 0.56f)), to = o(0.84f, 0.4f), region = 2),
        DemoStep(R.string.demo_gest_blade, "LT", DemoKind.HOLD, listOf(o(0.62f, 0.46f), o(0.76f, 0.62f))),
        DemoStep(R.string.demo_gest_back, "B", DemoKind.TAP, listOf(o(0.58f, 0.5f), o(0.7f, 0.56f))),
        DemoStep(R.string.demo_gest_start, "START", DemoKind.TAP, listOf(o(0.42f, 0.5f), o(0.54f, 0.44f), o(0.66f, 0.52f))),
        DemoStep(R.string.demo_gest_jump, "A", DemoKind.TAP, listOf(o(0.25f, 0.5f)), region = 1),
    )
    val mouse = listOf(
        DemoStep(R.string.demo_mouse_drag, "◉", DemoKind.DRAG, listOf(o(0.36f, 0.62f)), to = o(0.62f, 0.38f)),
        DemoStep(R.string.demo_mouse_tap, "◉", DemoKind.TAP, listOf(o(0.62f, 0.38f))),
        DemoStep(R.string.demo_mouse_two, "Y", DemoKind.TAP, listOf(o(0.5f, 0.5f), o(0.62f, 0.56f))),
    )
    val menu = listOf(
        DemoStep(R.string.demo_menu_swipe, "↕", DemoKind.DRAG, listOf(o(0.7f, 0.32f)), to = o(0.7f, 0.62f)),
        DemoStep(R.string.demo_menu_tap, "A", DemoKind.TAP, listOf(o(0.7f, 0.5f))),
        DemoStep(R.string.demo_menu_back, "B", DemoKind.TAP, listOf(o(0.64f, 0.5f), o(0.76f, 0.56f))),
    )
}

private const val STEP_MS = 2300

/**
 * Explicacao visual animada: um ecra esquematico com o dedo a fazer cada gesto, a legenda do passo por baixo e a lista
 * de todos os passos (tocar num passo mostra-o).
 */
@Composable
fun TouchDemo(scene: DemoScene, steps: List<DemoStep>, modifier: Modifier = Modifier) {
    var step by remember(scene) { mutableIntStateOf(0) }
    val p = remember(scene) { Animatable(0f) }
    LaunchedEffect(scene, step) {
        p.snapTo(0f)
        p.animateTo(1f, tween(STEP_MS, easing = LinearEasing))
        step = (step + 1) % steps.size
    }
    val measurer = rememberTextMeasurer()
    val current = steps[step.coerceIn(0, steps.size - 1)]
    Column(modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.Center) {
            Canvas(
                Modifier
                    .widthIn(max = 460.dp)
                    .fillMaxWidth()
                    .aspectRatio(16f / 9f),
            ) { drawDemo(scene, step, current, p.value, measurer) }
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            PadBadge(current.pad, true)
            Spacer(Modifier.width(10.dp))
            Text(stringResource(current.caption), style = MaterialTheme.typography.titleSmall)
        }
        Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
            steps.forEachIndexed { i, s ->
                val on = i == step
                Row(
                    Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(8.dp))
                        .background(if (on) MaterialTheme.colorScheme.secondaryContainer else Color.Transparent)
                        .clickable { step = i }
                        .padding(horizontal = 8.dp, vertical = 4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    PadBadge(s.pad, on)
                    Spacer(Modifier.width(10.dp))
                    Text(
                        stringResource(s.caption),
                        style = MaterialTheme.typography.bodySmall,
                        color = if (on) MaterialTheme.colorScheme.onSecondaryContainer else MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }
    }
}

@Composable
private fun PadBadge(text: String, on: Boolean) {
    val color = padColor(text) ?: if (on) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant
    Box(
        Modifier
            .widthIn(min = 34.dp)
            .background(color.copy(alpha = if (on) 0.22f else 0.12f), RoundedCornerShape(6.dp))
            .padding(horizontal = 6.dp, vertical = 2.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(text, style = MaterialTheme.typography.labelMedium, color = color, maxLines = 1, softWrap = false)
    }
}

/** As cores dos botoes do comando de Xbox. */
fun padColor(pad: String): Color? = when (pad) {
    "A" -> Color(0xFF5DBB46)
    "B" -> Color(0xFFE5483C)
    "X" -> Color(0xFF3B8CE8)
    "Y" -> Color(0xFFF2C230)
    else -> null
}

// ------------------------------------------------------------------------------------------------------ desenho

private fun phase(t: Float, start: Float, len: Float) = ((t - start) / len).coerceIn(0f, 1f)
private fun ease(x: Float): Float { val v = x.coerceIn(0f, 1f); return v * v * (3f - 2f * v) }

/** Quando o dedo levanta, por tipo de gesto. */
private fun upAt(kind: DemoKind) = when (kind) {
    DemoKind.TAP -> 0.34f
    DemoKind.LONG -> 0.8f
    DemoKind.DRAG -> 0.8f
    DemoKind.HOLD -> 0.86f
}

private fun DemoStep.fingerAt(i: Int, p: Float): Offset {
    val start = fingers[i]
    val end = to ?: return start
    val d = (end - start) * ease(phase(p, 0.22f, 0.5f))
    return start + d
}

private fun DrawScope.drawDemo(scene: DemoScene, index: Int, step: DemoStep, p: Float, measurer: TextMeasurer) {
    val w = size.width
    val h = size.height
    fun px(o: Offset) = Offset(o.x * w, o.y * h)
    val down = p >= 0.15f && p < upAt(step.kind)
    val up = upAt(step.kind)

    // O ecra do aparelho: grafite com bordo.
    drawRoundRect(Color(0xFF14171C), cornerRadius = CornerRadius(18f * density))
    drawRoundRect(DfColors.Steel.copy(alpha = 0.45f), cornerRadius = CornerRadius(18f * density), style = Stroke(1.5f * density))
    // Um "jogo" de fundo: chao e horizonte, para o ecra nao parecer vazio.
    drawLine(DfColors.Steel.copy(alpha = 0.12f), Offset(0f, h * 0.82f), Offset(w, h * 0.82f), 1f * density)

    when (scene) {
        DemoScene.GESTURES -> {
            if (step.region != 0 && down) {
                val left = if (step.region == 1) 0f else w / 2f
                drawRect(DfColors.Lavender.copy(alpha = 0.10f), Offset(left, 0f), Size(w / 2f, h))
            }
            drawLine(
                DfColors.Steel.copy(alpha = 0.35f), Offset(w / 2f, h * 0.06f), Offset(w / 2f, h * 0.94f), 1.2f * density,
                pathEffect = PathEffect.dashPathEffect(floatArrayOf(8f * density, 6f * density)),
            )
            label(measurer, "◀ ▶ ▲ ▼", Offset(w * 0.25f, h * 0.1f), DfColors.Steel.copy(alpha = 0.6f), 10f)
            label(measurer, "X  Y  R", Offset(w * 0.75f, h * 0.1f), DfColors.Steel.copy(alpha = 0.6f), 10f)
        }
        DemoScene.BUTTONS -> drawButtons(step, p, down, measurer)
        DemoScene.MOUSE -> {
            val cursor = if (step.kind == DemoKind.DRAG) step.fingerAt(0, p) else Offset(0.62f, 0.38f)
            if (index == 1 && p > up) {
                val r = phase(p, up, 0.3f)
                drawCircle(DfColors.Gold.copy(alpha = 0.7f * (1f - r)), radius = h * (0.03f + 0.08f * r), center = px(cursor), style = Stroke(2f * density))
            }
            drawCursor(px(cursor) + Offset(h * 0.05f, -h * 0.07f))
        }
        DemoScene.MENU -> drawMenu(index, p, up, measurer)
    }

    // Dedos.
    val appear = phase(p, 0.04f, 0.1f)
    val gone = 1f - phase(p, up, 0.12f)
    val r = h * 0.055f
    for (i in step.fingers.indices) {
        val c = px(step.fingerAt(i, p))
        // Rasto do arrasto.
        if (step.kind == DemoKind.DRAG && p >= 0.22f) {
            val s = px(step.fingers[i])
            drawLine(DfColors.Lavender.copy(alpha = 0.45f * gone), s, c, 4f * density, StrokeCap.Round)
        }
        val alpha = if (down) 1f else if (p < 0.15f) 0.35f * appear else 0.35f * gone
        val pressScale = if (down) 1f else 1.25f
        drawCircle(Color.White.copy(alpha = 0.22f * alpha), radius = r * pressScale * 1.35f, center = c)
        drawCircle(Color.White.copy(alpha = 0.85f * alpha), radius = r * pressScale * 0.62f, center = c)
        drawCircle(Color.White.copy(alpha = alpha), radius = r * pressScale * 0.62f, center = c, style = Stroke(1.5f * density))
        when (step.kind) {
            DemoKind.LONG -> if (down) {
                val sweep = 360f * phase(p, 0.15f, 0.55f)
                drawArc(DfColors.Gold, -90f, sweep, false, topLeft = c - Offset(r * 1.3f, r * 1.3f),
                    size = Size(r * 2.6f, r * 2.6f), style = Stroke(3f * density, cap = StrokeCap.Round))
            }
            DemoKind.HOLD -> if (down) {
                val pulse = (sin(p * 2f * PI.toFloat() * 3f) + 1f) / 2f
                drawCircle(DfColors.Lavender.copy(alpha = 0.25f + 0.25f * pulse), radius = r * (1.5f + 0.3f * pulse), center = c,
                    style = Stroke(2.5f * density))
            }
            else -> {}
        }
        // Onda depois de levantar (toque e toque longo).
        if ((step.kind == DemoKind.TAP || step.kind == DemoKind.LONG) && p >= up) {
            val t = phase(p, up, 0.35f)
            drawCircle((padColor(step.pad) ?: DfColors.Gold).copy(alpha = 0.8f * (1f - t)), radius = r * (0.8f + 1.8f * t), center = c,
                style = Stroke(2.5f * density))
        }
    }
    // Numero de dedos, quando ha mais de um.
    if (step.fingers.size > 1 && p >= 0.1f && p < up + 0.12f) {
        label(measurer, "×${step.fingers.size}", px(step.fingers.last()) + Offset(r * 1.8f, -r * 1.6f), Color.White, 13f)
    }
}

private fun DrawScope.label(measurer: TextMeasurer, text: String, center: Offset, color: Color, sp: Float, bold: Boolean = true) {
    val layout = measurer.measure(text, TextStyle(color = color, fontSize = sp.sp, fontWeight = if (bold) FontWeight.Bold else FontWeight.Normal))
    drawText(layout, topLeft = center - Offset(layout.size.width / 2f, layout.size.height / 2f))
}

/** Ecra do modo Botoes: analogico a esquerda, ABXY a direita, LT/RT em cima. */
private fun DrawScope.drawButtons(step: DemoStep, p: Float, down: Boolean, measurer: TextMeasurer) {
    val w = size.width
    val h = size.height
    val base = Offset(0.17f * w, 0.66f * h)
    val baseR = h * 0.17f
    drawCircle(Color.White.copy(alpha = 0.06f), radius = baseR, center = base)
    drawCircle(Color.White.copy(alpha = 0.35f), radius = baseR, center = base, style = Stroke(1.5f * density))
    val knob = if (step.target == "stick" && down) {
        val f = Offset(step.fingerAt(0, p).x * w, step.fingerAt(0, p).y * h)
        val d = f - base
        val len = d.getDistance()
        if (len > baseR * 0.7f) base + d * (baseR * 0.7f / len) else f
    } else base
    drawCircle(Color.White.copy(alpha = 0.45f), radius = baseR * 0.42f, center = knob)
    val buttons = listOf(
        Triple("X", Offset(0.77f, 0.62f), padColor("X")!!),
        Triple("Y", Offset(0.85f, 0.46f), padColor("Y")!!),
        Triple("A", Offset(0.85f, 0.78f), padColor("A")!!),
        Triple("B", Offset(0.93f, 0.62f), padColor("B")!!),
        Triple("LT", Offset(0.09f, 0.24f), DfColors.Steel),
        Triple("RT", Offset(0.91f, 0.24f), DfColors.Steel),
    )
    val br = h * 0.075f
    for ((name, at, color) in buttons) {
        val c = Offset(at.x * w, at.y * h)
        val pressed = down && step.target == name
        drawCircle(color.copy(alpha = if (pressed) 0.75f else 0.18f), radius = br, center = c)
        drawCircle(color.copy(alpha = 0.8f), radius = br, center = c, style = Stroke(1.5f * density))
        label(measurer, name, c, if (pressed) Color.White else color, if (name.length > 1) 9f else 11f)
    }
    // Botao de menu no canto.
    drawCircle(Color.White.copy(alpha = 0.25f), radius = h * 0.04f, center = Offset(0.035f * w + h * 0.04f, 0.06f * h + h * 0.04f), style = Stroke(1.2f * density))
}

/** Ecra dos menus: uma lista com a linha escolhida; o deslize desce uma linha, o toque confirma, dois dedos fecha. */
private fun DrawScope.drawMenu(index: Int, p: Float, up: Float, measurer: TextMeasurer) {
    val w = size.width
    val h = size.height
    val panelAlpha = if (index == 2) 1f - phase(p, up, 0.25f) else 1f
    val row = when (index) {
        0 -> if (phase(p, 0.22f, 0.5f) > 0.55f) 1 else 0
        else -> 1
    }
    val left = w * 0.08f
    val width = w * 0.38f
    val top = h * 0.16f
    val rowH = h * 0.15f
    drawRoundRect(Color.White.copy(alpha = 0.05f * panelAlpha), Offset(left - 8f, top - 10f), Size(width + 16f, rowH * 4 + 20f),
        cornerRadius = CornerRadius(10f * density))
    val names = listOf("STORY", "OPTIONS", "EXTRAS", "EXIT")
    for (i in 0 until 4) {
        val y = top + i * rowH
        val sel = i == row
        if (sel) {
            val flash = if (index == 1 && p >= up) 1f - phase(p, up, 0.4f) else 0f
            drawRoundRect(DfColors.Gold.copy(alpha = (0.35f + 0.5f * flash) * panelAlpha), Offset(left, y), Size(width, rowH * 0.8f),
                cornerRadius = CornerRadius(6f * density))
        }
        label(measurer, names[i], Offset(left + width / 2f, y + rowH * 0.4f),
            (if (sel) Color.White else DfColors.Steel).copy(alpha = panelAlpha), 10f)
    }
}

/** Seta do rato. */
private fun DrawScope.drawCursor(tip: Offset) {
    val s = size.height * 0.11f
    val path = Path().apply {
        moveTo(tip.x, tip.y)
        lineTo(tip.x, tip.y + s)
        lineTo(tip.x + s * 0.28f, tip.y + s * 0.75f)
        lineTo(tip.x + s * 0.5f, tip.y + s * 1.08f)
        lineTo(tip.x + s * 0.62f, tip.y + s * 1.0f)
        lineTo(tip.x + s * 0.42f, tip.y + s * 0.68f)
        lineTo(tip.x + s * 0.75f, tip.y + s * 0.68f)
        close()
    }
    drawPath(path, Color.White)
    drawPath(path, Color.Black, style = Stroke(1.2f * density))
}
