package com.dfdx047.swordman.ui.components

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.BlendMode
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipPath
import androidx.compose.ui.graphics.drawscope.rotate
import androidx.compose.ui.graphics.drawscope.translate
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.lerp
import com.dfdx047.swordman.ui.theme.DfColors
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.sin
import kotlin.random.Random

/**
 * Palco animado do Inicio: uma lamina corta uma pedra em dois (o Blade Mode), com o rasto do
 * corte em ciano frio, faiscas e as metades a deslizar pelo plano do corte. Ciclo de 3,6 s,
 * desenhado num Canvas (sem imagens). [content] fica por cima (logotipo, estado, Jogar).
 */
@Composable
fun SwordStage(modifier: Modifier = Modifier, content: @Composable BoxScope.() -> Unit) {
    val transition = rememberInfiniteTransition(label = "sword")
    val t by transition.animateFloat(
        initialValue = 0f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(tween(3600, easing = LinearEasing)),
        label = "cycle",
    )
    val sparks = remember { List(30) { Spark(Random(it * 7919 + 13)) } }
    Box(modifier) {
        Canvas(Modifier.matchParentSize()) { drawStage(t, sparks) }
        content()
    }
}

private class Spark(r: Random) {
    /** 0..1: direcao no semicirculo de cima (para tras do golpe). */
    val dir = r.nextFloat()
    val speed = 0.5f + r.nextFloat() * 1.0f
    val delay = r.nextFloat() * 0.10f
    val life = 0.16f + r.nextFloat() * 0.22f
    /** Onde nasce ao longo do corte (0..1). */
    val along = r.nextFloat()
}

private val RockLit = Color(0xFF4A505A)
private val RockMid = Color(0xFF2A2E35)
private val RockDark = Color(0xFF121418)

/** Pedra irregular (raios fixos: a mesma pedra em todos os ciclos). */
private val STONE_RADII = floatArrayOf(1.0f, 0.86f, 0.97f, 0.78f, 0.92f, 1.05f, 0.83f, 0.95f, 0.88f)

private fun stonePath(c: Offset, r: Float): Path = Path().apply {
    val n = STONE_RADII.size
    for (i in 0 until n) {
        val a = (i.toFloat() / n) * 2f * PI.toFloat() - PI.toFloat() / 2f + 0.2f
        val rr = r * STONE_RADII[i]
        // Pedra mais larga que alta, assente no chao.
        val p = Offset(c.x + cos(a) * rr * 1.18f, c.y + sin(a) * rr * 0.86f)
        if (i == 0) moveTo(p.x, p.y) else lineTo(p.x, p.y)
    }
    close()
}

/** Semiplano (um quadrado enorme) de um lado da linha que passa em [c] com direcao [dir]. */
private fun halfPlane(c: Offset, dir: Offset, normal: Offset, side: Float, big: Float): Path = Path().apply {
    val n = normal * side
    val a = c - dir * big
    val b = c + dir * big
    moveTo(a.x, a.y)
    lineTo(b.x, b.y)
    lineTo(b.x + n.x * big, b.y + n.y * big)
    lineTo(a.x + n.x * big, a.y + n.y * big)
    close()
}

private fun ease(x: Float): Float {
    val v = x.coerceIn(0f, 1f)
    return 1f - (1f - v) * (1f - v) * (1f - v)
}

private fun phase(t: Float, start: Float, len: Float) = ((t - start) / len).coerceIn(0f, 1f)

private fun DrawScope.drawStone(path: Path, c: Offset, r: Float, alpha: Float) {
    if (alpha <= 0f) return
    drawPath(
        path,
        Brush.linearGradient(
            0f to RockLit, 0.45f to RockMid, 1f to RockDark,
            start = Offset(c.x - r, c.y - r),
            end = Offset(c.x + r, c.y + r),
        ),
        alpha = alpha,
    )
    drawPath(path, DfColors.Steel, alpha = 0.35f * alpha, style = Stroke(width = 1.2f * density))
    // Duas arestas (facetas) para a pedra nao ser so uma mancha.
    drawLine(
        Color.White.copy(alpha = 0.07f * alpha),
        Offset(c.x - r * 0.7f, c.y - r * 0.2f), Offset(c.x + r * 0.1f, c.y - r * 0.55f),
        strokeWidth = 1.5f * density,
    )
    drawLine(
        Color.Black.copy(alpha = 0.25f * alpha),
        Offset(c.x - r * 0.2f, c.y + r * 0.6f), Offset(c.x + r * 0.8f, c.y + r * 0.1f),
        strokeWidth = 1.5f * density,
    )
}

private fun DrawScope.drawStage(t: Float, sparks: List<Spark>) {
    val w = size.width
    val h = size.height
    // Fundo: grafite com o brilho vermelho do horizonte e a linha do chao.
    drawRect(Brush.verticalGradient(listOf(Color(0xFF111317), DfColors.Night)))
    drawRect(
        Brush.radialGradient(
            listOf(DfColors.Gold.copy(alpha = 0.16f), Color.Transparent),
            center = Offset(w * 0.66f, h * 0.84f),
            radius = w * 0.6f,
        ),
    )
    val ground = h * 0.84f
    drawLine(DfColors.Steel.copy(alpha = 0.22f), Offset(0f, ground), Offset(w, ground), 1f * density)

    val r = min(w * 0.5f, h) * 0.26f
    val c = Offset(w * 0.68f, ground - r * 0.82f)
    val stone = stonePath(c, r)
    val ang = (-24.0 * PI / 180.0)
    val dir = Offset(cos(ang).toFloat(), sin(ang).toFloat())
    val normal = Offset(-dir.y, dir.x)
    val reach = r * 1.9f
    val cutStart = c - dir * reach
    val cutEnd = c + dir * reach

    val slash = phase(t, 0.16f, 0.12f)          // o golpe atravessa
    val split = ease(phase(t, 0.27f, 0.45f))     // as metades separam-se
    val fadeOut = 1f - phase(t, 0.80f, 0.10f)
    val fadeIn = phase(t, 0.90f, 0.10f)
    val heat = (1f - phase(t, 0.27f, 0.5f)) * fadeOut

    // Pedra: inteira, cortada a separar-se, ou a nova a aparecer.
    when {
        t < 0.27f -> drawStone(stone, c, r, 1f)
        t < 0.90f -> {
            val big = w + h
            val upper = halfPlane(c, dir, normal, -1f, big)
            val lower = halfPlane(c, dir, normal, 1f, big)
            clipPath(lower) { drawStone(stone, c, r, fadeOut) }
            val slide = dir * (split * r * 0.42f) - normal * (split * r * 0.14f)
            translate(slide.x, slide.y) {
                rotate(-9f * split, pivot = c) {
                    clipPath(upper) { drawStone(stone, c, r, fadeOut) }
                    // Face do corte da metade de cima, ainda quente.
                    clipPath(stone) {
                        drawLine(DfColors.Ember.copy(alpha = 0.8f * heat), cutStart, cutEnd, 2.5f * density)
                    }
                }
            }
            clipPath(stone) {
                drawLine(DfColors.Ember.copy(alpha = 0.9f * heat), cutStart, cutEnd, 3f * density)
                drawLine(Color.White.copy(alpha = 0.7f * heat), cutStart, cutEnd, 1f * density)
            }
        }
        else -> drawStone(stone, c, r, fadeIn)
    }

    // Rasto do golpe: ciano frio com nucleo branco, a correr ao longo do corte.
    val trailFade = if (t < 0.28f) 1f else 1f - phase(t, 0.28f, 0.22f)
    if (slash > 0f && trailFade > 0f) {
        val head = cutStart + (cutEnd - cutStart) * slash
        val tail = cutStart + (cutEnd - cutStart) * (slash - 0.55f).coerceAtLeast(0f)
        drawLine(DfColors.Lavender.copy(alpha = 0.22f * trailFade), tail, head, 12f * density, StrokeCap.Round)
        drawLine(DfColors.Lavender.copy(alpha = 0.55f * trailFade), tail, head, 4f * density, StrokeCap.Round)
        drawLine(Color.White.copy(alpha = 0.95f * trailFade), tail, head, 1.4f * density, StrokeCap.Round)
        // A lamina, perpendicular ao corte, so durante o golpe.
        if (slash < 1f) {
            val bladeA = head - normal * (r * 1.25f)
            val bladeB = head + normal * (r * 0.35f)
            drawLine(Color(0xFF0E0F12), bladeA, bladeB, 5f * density, StrokeCap.Round)
            drawLine(DfColors.Gold.copy(alpha = 0.9f), bladeA, bladeB, 1.3f * density, StrokeCap.Round)
        }
    }

    // Faiscas: nascem no corte e caem com a gravidade.
    for (s in sparks) {
        val st = (t - 0.19f - s.delay) / s.life
        if (st <= 0f || st >= 1f) continue
        val origin = c + dir * (r * (s.along * 1.7f - 0.85f))
        val a = -PI.toFloat() * (0.08f + 0.84f * s.dir)
        val vel = Offset(cos(a), sin(a)) * (s.speed * r * 1.4f)
        val pos = origin + vel * st + Offset(0f, r * 1.6f * st * st)
        val streak = vel * 0.07f + Offset(0f, r * 0.2f * st)
        val col = lerp(Color.White, DfColors.Ember, st)
        drawLine(col.copy(alpha = 1f - st), pos - streak, pos, 1.8f * density, StrokeCap.Round)
    }
    // Clarao no instante do contacto.
    val flash = 1f - phase(t, 0.22f, 0.12f)
    if (t > 0.2f && flash > 0f) {
        drawCircle(
            Brush.radialGradient(
                listOf(Color.White.copy(alpha = 0.6f * flash), DfColors.Ember.copy(alpha = 0.25f * flash), Color.Transparent),
                center = c,
                radius = r * 1.2f,
            ),
            radius = r * 1.2f,
            center = c,
        )
    }
}

/** Texto com um gradiente por cima (o logotipo). */
fun Modifier.gradientText(brush: Brush): Modifier = this
    .graphicsLayer(alpha = 0.99f)
    .drawWithCache {
        onDrawWithContent {
            drawContent()
            drawRect(brush, blendMode = BlendMode.SrcAtop)
        }
    }
