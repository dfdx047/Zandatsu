package com.dfdx047.swordman.game

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.DashPathEffect
import android.graphics.Paint
import android.graphics.Path
import android.graphics.PorterDuff
import android.graphics.PorterDuffColorFilter
import android.graphics.RadialGradient
import android.graphics.Shader
import android.graphics.Typeface
import android.graphics.drawable.Drawable
import android.os.SystemClock
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import androidx.core.content.ContextCompat
import com.dfdx047.swordman.controls.ControlAction
import com.dfdx047.swordman.controls.ControlElement
import com.dfdx047.swordman.controls.ControlIcon
import com.dfdx047.swordman.controls.ControlLayout
import com.dfdx047.swordman.controls.ElementKind
import com.dfdx047.swordman.controls.GameActions
import com.dfdx047.swordman.controls.PadFamily
import com.dfdx047.swordman.controls.PadInput
import com.dfdx047.swordman.data.ButtonBadge
import com.dfdx047.swordman.data.ControlStyle
import com.dfdx047.swordman.data.Directional
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.hypot

/**
 * Controles por toque desenhados por cima do jogo (e no editor de layout).
 *
 * Estilo MGR: circulos de vidro grafite com borda de aco fina e icones brancos frios; premido
 * acende em vermelho. Classicos XBOX / PLAYSTATION / NINTENDO: letras ou simbolos do comando.
 * Cada botao leva um selo pequeno com o botao do comando (Xbox, PlayStation ou Nintendo).
 *
 * Analogicos: "stick" = esquerdo; "rstick" = direito (layout completo). Zona da camara (LOOK):
 * arrastar o dedo em qualquer sitio livre dela move o analogico direito a partir do ponto onde
 * o dedo pousou; um toque rapido carrega o R3 (centrar a camara).
 */
@SuppressLint("ViewConstructor")
class TouchControlsView(context: Context) : View(context) {

    interface Listener {
        fun onPress(element: ControlElement)
        fun onRelease(element: ControlElement)
        /** Analogico esquerdo ([right] = false) ou direito (rstick e zona da camara). */
        fun onStick(right: Boolean, x: Float, y: Float)
        fun onDpad(element: ControlElement, inputs: Set<PadInput>)
        /** Toque rapido na zona da camara: R3 por um instante. */
        fun onLookTap(element: ControlElement)
    }

    interface EditListener {
        fun onSelected(id: String?)
        fun onMoved(layout: ControlLayout)
    }

    var layout: ControlLayout = GameActions.defaultLayout()
        set(value) {
            field = value
            invalidate()
        }
    var style: ControlStyle = ControlStyle.GAME
        set(value) {
            field = value
            invalidate()
        }
    /** D-pad de 8 direcoes ou analogico para andar (o outro fica escondido). */
    var directional: Directional = Directional.STICK
        set(value) {
            field = value
            invalidate()
        }
    /** Indicacao do botao do comando no canto de cada botao. */
    var badge: ButtonBadge = ButtonBadge.XBOX
        set(value) {
            field = value
            invalidate()
        }
    var globalOpacity: Float = 0.75f
        set(value) {
            field = value.coerceIn(0.1f, 1f)
            invalidate()
        }
    var haptics = true
    var editMode = false
        set(value) {
            field = value
            invalidate()
        }
    var selectedId: String? = null
        set(value) {
            field = value
            invalidate()
        }
    var listener: Listener? = null
    var editListener: EditListener? = null

    private val density = resources.displayMetrics.density
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val ring = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE }
    private val text = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        textAlign = Paint.Align.CENTER
        typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
    }
    private val icons = HashMap<ControlIcon, Drawable>()

    private val pressed = HashSet<String>()
    /** pointerId -> id do elemento */
    private val pointers = HashMap<Int, String>()

    /** Analogico em uso: dedo e deslocamento (-1..1). */
    private class StickState(val pointer: Int, var dx: Float = 0f, var dy: Float = 0f)
    private val sticks = HashMap<String, StickState>()

    /** Zona da camara: onde o dedo pousou, onde esta e quando. */
    private class LookState(val pointer: Int, val ax: Float, val ay: Float, val downAt: Long) {
        var x = ax
        var y = ay
        var moved = false
    }
    private var look: LookState? = null
    private var lookId: String? = null

    private val dpadState = HashMap<String, Set<PadInput>>()

    // Edicao
    private var dragId: String? = null
    private var dragOffX = 0f
    private var dragOffY = 0f

    // Cores do MGR: vidro grafite, aco, branco frio; premido em vermelho, ciano na camara.
    private val red = Color.parseColor("#E3263A")
    private val cold = Color.parseColor("#E4E7EB")
    private val steel = Color.parseColor("#8C96A3")
    private val cyan = Color.parseColor("#7CC4E4")

    init {
        isHapticFeedbackEnabled = true
    }

    private fun icon(i: ControlIcon): Drawable =
        icons.getOrPut(i) { ContextCompat.getDrawable(context, i.drawable)!!.mutate() }

    private fun cx(e: ControlElement) = e.x * width
    private fun cy(e: ControlElement) = e.y * height
    private fun radius(e: ControlElement) = e.sizeDp * density / 2f

    private fun alphaOf(e: ControlElement, base: Float): Int =
        (255 * base * e.opacity * (if (editMode) 1f else globalOpacity)).toInt().coerceIn(0, 255)

    /** Familia dos rotulos (letras, simbolos) para o estilo escolhido. */
    private fun family(): PadFamily = when (style) {
        ControlStyle.PLAYSTATION -> PadFamily.PLAYSTATION
        ControlStyle.NINTENDO -> PadFamily.NINTENDO
        ControlStyle.XBOX -> PadFamily.XBOX
        ControlStyle.GAME -> when (badge) {
            ButtonBadge.PLAYSTATION -> PadFamily.PLAYSTATION
            ButtonBadge.NINTENDO -> PadFamily.NINTENDO
            else -> PadFamily.XBOX
        }
    }

    /**
     * Visivel agora: escondido pelo usuario ou pelo tipo de direcional escolhido. So o analogico
     * "stick" e a cruz "dpad" trocam um com o outro; no layout completo cada um so depende do
     * proprio "visivel".
     */
    private fun isShown(e: ControlElement): Boolean = e.visible && when (e.kind) {
        ElementKind.STICK -> layout.full || e.id != "stick" || directional == Directional.STICK
        ElementKind.DPAD -> layout.full || e.id != "dpad" || directional == Directional.DPAD
        ElementKind.BUTTON, ElementKind.LOOK -> true
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        // A zona da camara primeiro (fica por baixo dos botoes).
        for (e in layout.elements) if (e.kind == ElementKind.LOOK && isShown(e)) drawLook(canvas, e)
        for (e in layout.elements) {
            if (!isShown(e)) continue
            when (e.kind) {
                ElementKind.BUTTON -> drawButton(canvas, e)
                ElementKind.STICK -> drawStick(canvas, e)
                ElementKind.DPAD -> drawDpad(canvas, e)
                ElementKind.LOOK -> Unit
            }
            if (editMode && e.id == selectedId && e.kind != ElementKind.LOOK) {
                ring.color = red
                ring.alpha = 255
                ring.strokeWidth = 2f * density
                ring.pathEffect = DashPathEffect(floatArrayOf(8f * density, 6f * density), 0f)
                canvas.drawCircle(cx(e), cy(e), radius(e) + 6 * density, ring)
                ring.pathEffect = null
            }
        }
    }

    /** Entrada principal do botao (para o rotulo classico). */
    private fun mainInput(e: ControlElement): PadInput? =
        (e.action as? ControlAction.Press)?.inputs?.singleOrNull()

    private fun isFaceButton(e: ControlElement): Boolean =
        mainInput(e) in setOf(PadInput.A, PadInput.B, PadInput.X, PadInput.Y)

    /** Vidro grafite com borda fina de aco; premido acende em vermelho por baixo do icone. */
    private fun drawGlass(c: Canvas, e: ControlElement, x: Float, y: Float, r: Float, down: Boolean) {
        fill.shader = RadialGradient(
            x, y - r * 0.4f, r * 1.4f,
            intArrayOf(Color.argb(120, 52, 58, 66), Color.argb(100, 26, 29, 34), Color.argb(90, 10, 11, 13)),
            floatArrayOf(0f, 0.6f, 1f), Shader.TileMode.CLAMP,
        )
        fill.alpha = alphaOf(e, 1f)
        c.drawCircle(x, y, r, fill)
        if (down) {
            fill.shader = RadialGradient(
                x, y, r,
                intArrayOf(Color.argb(190, 227, 38, 58), Color.argb(110, 184, 29, 46), Color.argb(0, 184, 29, 46)),
                floatArrayOf(0f, 0.65f, 1f), Shader.TileMode.CLAMP,
            )
            fill.alpha = alphaOf(e, 1f)
            c.drawCircle(x, y, r, fill)
        }
        fill.shader = null
        ring.strokeWidth = 1.2f * density
        ring.color = if (down) red else steel
        ring.alpha = alphaOf(e, if (down) 1f else 0.6f)
        c.drawCircle(x, y, r - ring.strokeWidth / 2, ring)
    }

    private fun drawButton(c: Canvas, e: ControlElement) {
        val x = cx(e)
        val y = cy(e)
        val r = radius(e)
        val down = e.id in pressed
        val fam = family()
        // Estilos classicos (e os menus): A/B/X/Y com a letra/simbolo do comando.
        val classic = (style != ControlStyle.GAME || layout.menu) && isFaceButton(e)

        drawGlass(c, e, x, y, r, down)

        if (classic) {
            drawClassicGlyph(c, mainInput(e)!!, fam, x, y, r * 0.95f, alphaOf(e, 1f))
        } else if (e.icon == ControlIcon.MACRO) {
            // Botao sem icone: o rotulo do comando (LB, R2, L3...) ou o que o usuario escreveu.
            val label = mainInput(e)?.let { GameActions.classicLabel(it, fam) } ?: e.label
            text.color = if (down) Color.WHITE else cold
            text.alpha = alphaOf(e, 1f)
            text.textSize = r * (if (label.length > 2) 0.42f else 0.55f)
            c.drawText(label.take(5), x, y + text.textSize / 3, text)
        } else {
            val d = icon(e.icon)
            val s = (r * 1.0f).toInt()
            d.setBounds((x - s / 2).toInt(), (y - s / 2).toInt(), (x + s / 2).toInt(), (y + s / 2).toInt())
            d.colorFilter = PorterDuffColorFilter(if (down) Color.WHITE else cold, PorterDuff.Mode.SRC_IN)
            d.alpha = alphaOf(e, 1f)
            d.draw(c)
        }
        // Selo com o botao do comando (nos A/B/X/Y classicos ja e o proprio desenho).
        val showBadge = when (style) {
            ControlStyle.GAME -> badge != ButtonBadge.NONE
            else -> true
        }
        if (showBadge && !classic && e.icon != ControlIcon.MACRO) drawBadge(c, e, fam, x, y, r)
    }

    private fun badgeInputs(e: ControlElement): List<PadInput> = when (val a = e.action) {
        is ControlAction.Press -> a.inputs.sortedBy { it.ordinal }
        is ControlAction.Sequence -> a.steps.lastOrNull()?.inputs?.sortedBy { it.ordinal }.orEmpty()
    }

    private fun drawBadge(c: Canvas, e: ControlElement, fam: PadFamily, x: Float, y: Float, r: Float) {
        val inputs = badgeInputs(e)
        if (inputs.isEmpty()) return
        val br = (r * 0.34f).coerceIn(8f * density, 14f * density)
        val bx = x + r * 0.74f
        val by = y - r * 0.74f
        if (inputs.size == 1 && inputs[0] in FACE) {
            fill.shader = null
            fill.color = Color.argb(200, 10, 11, 13)
            fill.alpha = alphaOf(e, 0.85f)
            c.drawCircle(bx, by, br, fill)
            drawClassicGlyph(c, inputs[0], fam, bx, by, br * 1.05f, alphaOf(e, 1f))
            return
        }
        val label = inputs.joinToString("+") { GameActions.classicLabel(it, fam) }
        text.textSize = br * 0.9f
        val w = text.measureText(label) + br * 0.9f
        fill.shader = null
        fill.color = Color.argb(200, 10, 11, 13)
        fill.alpha = alphaOf(e, 0.85f)
        c.drawRoundRect(bx - w / 2, by - br * 0.78f, bx + w / 2, by + br * 0.78f, br * 0.4f, br * 0.4f, fill)
        text.color = cold
        text.alpha = alphaOf(e, 1f)
        c.drawText(label, bx, by + text.textSize / 3, text)
    }

    private fun classicColor(i: PadInput, fam: PadFamily): Int = when (fam) {
        PadFamily.PLAYSTATION -> when (i) {
            PadInput.A -> Color.parseColor("#7CB2E8")
            PadInput.B -> Color.parseColor("#FF6666")
            PadInput.X -> Color.parseColor("#E899C9")
            else -> Color.parseColor("#40E2A0")
        }
        PadFamily.XBOX -> when (i) {
            PadInput.A -> Color.parseColor("#5DC21E")
            PadInput.B -> Color.parseColor("#E5312C")
            PadInput.X -> Color.parseColor("#2C8BE5")
            else -> Color.parseColor("#F6C41B")
        }
        PadFamily.NINTENDO -> cold
    }

    /** Letra do Xbox/Nintendo ou simbolo do PlayStation, centrado em (x, y) com raio r. */
    private fun drawClassicGlyph(c: Canvas, i: PadInput, fam: PadFamily, x: Float, y: Float, r: Float, alpha: Int) {
        val color = classicColor(i, fam)
        if (fam != PadFamily.PLAYSTATION) {
            text.color = color
            text.alpha = alpha
            text.textSize = r * 0.9f
            c.drawText(GameActions.classicLabel(i, fam), x, y + text.textSize / 3, text)
            return
        }
        ring.color = color
        ring.alpha = alpha
        ring.strokeWidth = (r * 0.12f).coerceAtLeast(1.5f * density)
        val s = r * 0.42f
        when (i) {
            PadInput.A -> {
                c.drawLine(x - s, y - s, x + s, y + s, ring)
                c.drawLine(x - s, y + s, x + s, y - s, ring)
            }
            PadInput.B -> c.drawCircle(x, y, s, ring)
            PadInput.X -> c.drawRect(x - s * 0.9f, y - s * 0.9f, x + s * 0.9f, y + s * 0.9f, ring)
            else -> {
                val p = Path()
                p.moveTo(x, y - s)
                p.lineTo(x + s, y + s * 0.75f)
                p.lineTo(x - s, y + s * 0.75f)
                p.close()
                c.drawPath(p, ring)
            }
        }
    }

    private fun drawStick(c: Canvas, e: ControlElement) {
        val x = cx(e)
        val y = cy(e)
        val r = radius(e)
        drawGlass(c, e, x, y, r, false)
        // Marcas finas nos 4 pontos cardeais.
        ring.color = steel
        ring.alpha = alphaOf(e, 0.5f)
        ring.strokeWidth = 1.5f * density
        for ((dx, dy) in listOf(0f to -1f, 0f to 1f, -1f to 0f, 1f to 0f)) {
            c.drawLine(x + dx * r * 0.78f, y + dy * r * 0.78f, x + dx * r * 0.9f, y + dy * r * 0.9f, ring)
        }
        val st = sticks[e.id]
        val active = st != null && !editMode
        val kx = x + (st?.dx ?: 0f) * r * 0.55f
        val ky = y + (st?.dy ?: 0f) * r * 0.55f
        val kr = r * 0.38f
        fill.shader = RadialGradient(
            kx - kr * 0.3f, ky - kr * 0.3f, kr * 1.3f,
            intArrayOf(Color.argb(230, 70, 76, 86), Color.argb(230, 22, 24, 28)),
            null, Shader.TileMode.CLAMP,
        )
        fill.alpha = alphaOf(e, if (active) 0.95f else 0.75f)
        c.drawCircle(kx, ky, kr, fill)
        fill.shader = null
        ring.color = if (active) red else steel
        ring.alpha = alphaOf(e, 0.9f)
        ring.strokeWidth = 1.5f * density
        c.drawCircle(kx, ky, kr, ring)
        if (e.id == "rstick") {
            text.color = cold
            text.alpha = alphaOf(e, 0.8f)
            text.textSize = kr * 0.6f
            c.drawText("R", kx, ky + text.textSize / 3, text)
        }
    }

    /** Cruz de 8 direcoes: as direcoes premidas (incluindo diagonais) acendem. */
    private fun drawDpad(c: Canvas, e: ControlElement) {
        val x = cx(e)
        val y = cy(e)
        val r = radius(e)
        val state = dpadState[e.id].orEmpty()
        drawGlass(c, e, x, y, r, false)
        val arm = r * 0.28f
        fill.shader = null
        fill.color = Color.argb(70, 228, 231, 235)
        fill.alpha = alphaOf(e, 0.45f)
        c.drawRoundRect(x - arm, y - r * 0.8f, x + arm, y + r * 0.8f, arm * 0.3f, arm * 0.3f, fill)
        c.drawRoundRect(x - r * 0.8f, y - arm, x + r * 0.8f, y + arm, arm * 0.3f, arm * 0.3f, fill)
        fun lit(dir: PadInput, l: Float, t: Float, rr: Float, b: Float) {
            if (dir !in state) return
            fill.color = red
            fill.alpha = alphaOf(e, 0.8f)
            c.drawRoundRect(l, t, rr, b, arm * 0.3f, arm * 0.3f, fill)
        }
        lit(PadInput.UP, x - arm, y - r * 0.8f, x + arm, y - arm)
        lit(PadInput.DOWN, x - arm, y + arm, x + arm, y + r * 0.8f)
        lit(PadInput.LEFT, x - r * 0.8f, y - arm, x - arm, y + arm)
        lit(PadInput.RIGHT, x + arm, y - arm, x + r * 0.8f, y + arm)
        // Cruz das armas/itens (MGR): icones pequenos nas pontas em vez de setas.
        if (e.id == "items") {
            fun ic(i: ControlIcon, px: Float, py: Float) {
                val d = icon(i)
                val s = (r * 0.42f).toInt()
                d.setBounds((px - s / 2).toInt(), (py - s / 2).toInt(), (px + s / 2).toInt(), (py + s / 2).toInt())
                d.colorFilter = PorterDuffColorFilter(cold, PorterDuff.Mode.SRC_IN)
                d.alpha = alphaOf(e, 0.9f)
                d.draw(c)
            }
            ic(ControlIcon.AUGMENT, x, y - r * 0.55f)
            ic(ControlIcon.ITEM, x, y + r * 0.55f)
            ic(ControlIcon.SWAP, x - r * 0.55f, y)
            ic(ControlIcon.SWAP, x + r * 0.55f, y)
            return
        }
        text.color = cold
        text.alpha = alphaOf(e, 0.9f)
        text.textSize = r * 0.22f
        c.drawText("▲", x, y - r * 0.5f, text)
        c.drawText("▼", x, y + r * 0.62f, text)
        c.drawText("◀", x - r * 0.56f, y + r * 0.08f, text)
        c.drawText("▶", x + r * 0.56f, y + r * 0.08f, text)
    }

    /**
     * Zona da camara. Durante o jogo so se ve onde o dedo esta (anel e pomo em ciano);
     * no editor e um retangulo tracejado com o icone, para a mover e mudar de tamanho.
     */
    private fun drawLook(c: Canvas, e: ControlElement) {
        if (editMode) {
            val (l, t, rr, b) = lookRect(e)
            ring.color = if (e.id == selectedId) red else cyan
            ring.alpha = 200
            ring.strokeWidth = 1.5f * density
            ring.pathEffect = DashPathEffect(floatArrayOf(10f * density, 8f * density), 0f)
            c.drawRoundRect(l, t, rr, b, 16f * density, 16f * density, ring)
            ring.pathEffect = null
            val d = icon(ControlIcon.CAMERA)
            val s = (28 * density).toInt()
            d.setBounds((cx(e) - s / 2).toInt(), (cy(e) - s / 2).toInt(), (cx(e) + s / 2).toInt(), (cy(e) + s / 2).toInt())
            d.colorFilter = PorterDuffColorFilter(cyan, PorterDuff.Mode.SRC_IN)
            d.alpha = 220
            d.draw(c)
            return
        }
        val st = look ?: return
        if (lookId != e.id || !st.moved) return
        val r = LOOK_RADIUS_DP * density
        ring.color = cyan
        ring.alpha = alphaOf(e, 0.45f)
        ring.strokeWidth = 1.5f * density
        c.drawCircle(st.ax, st.ay, r, ring)
        val (dx, dy) = lookVector(st)
        fill.shader = null
        fill.color = cyan
        fill.alpha = alphaOf(e, 0.5f)
        c.drawCircle(st.ax + dx * r, st.ay + dy * r, r * 0.3f, fill)
    }

    /** Retangulo da zona da camara: o centro e (x, y); a largura e sizeDp*1.6 e a altura sizeDp. */
    private fun lookRect(e: ControlElement): FloatArray {
        val hw = e.sizeDp * density * 0.8f
        val hh = e.sizeDp * density * 0.5f
        return floatArrayOf(cx(e) - hw, cy(e) - hh, cx(e) + hw, cy(e) + hh)
    }

    private fun inLook(e: ControlElement, px: Float, py: Float): Boolean {
        val (l, t, r, b) = lookRect(e)
        return px in l..r && py in t..b
    }

    private fun lookVector(st: LookState): Pair<Float, Float> {
        val r = LOOK_RADIUS_DP * density
        var dx = (st.x - st.ax) / r
        var dy = (st.y - st.ay) / r
        val len = hypot(dx, dy)
        if (len > 1f) {
            dx /= len
            dy /= len
        }
        return dx to dy
    }

    // ------------------------------------------------------------------ toque

    private fun hit(px: Float, py: Float): ControlElement? {
        var best: ControlElement? = null
        var bestD = Float.MAX_VALUE
        for (e in layout.elements) {
            if (!isShown(e) || e.kind == ElementKind.LOOK) continue
            val d = hypot(px - cx(e), py - cy(e))
            val reach = radius(e) * (if (e.kind == ElementKind.BUTTON) 1.15f else 1.25f)
            if (d <= reach && d / reach < bestD) {
                bestD = d / reach
                best = e
            }
        }
        if (best != null) return best
        // Nenhum botao: a zona da camara, se o dedo estiver dentro dela.
        return layout.elements.firstOrNull { it.kind == ElementKind.LOOK && isShown(it) && inLook(it, px, py) }
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(ev: MotionEvent): Boolean {
        if (editMode) return onEditTouch(ev)
        when (ev.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val i = ev.actionIndex
                val e = hit(ev.getX(i), ev.getY(i)) ?: return true
                val pid = ev.getPointerId(i)
                when (e.kind) {
                    ElementKind.STICK -> {
                        if (e.id !in sticks) {
                            sticks[e.id] = StickState(pid)
                            pointers[pid] = e.id
                            updateStick(e, ev.getX(i), ev.getY(i))
                        }
                    }
                    ElementKind.DPAD -> {
                        pointers[pid] = e.id
                        updateDpad(e, ev.getX(i), ev.getY(i))
                    }
                    ElementKind.BUTTON -> {
                        pointers[pid] = e.id
                        pressButton(e)
                    }
                    ElementKind.LOOK -> {
                        if (look == null) {
                            look = LookState(pid, ev.getX(i), ev.getY(i), SystemClock.uptimeMillis())
                            lookId = e.id
                            pointers[pid] = e.id
                        }
                    }
                }
            }
            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until ev.pointerCount) {
                    val pid = ev.getPointerId(i)
                    val id = pointers[pid] ?: continue
                    val e = layout.elements.firstOrNull { it.id == id } ?: continue
                    when (e.kind) {
                        ElementKind.STICK -> updateStick(e, ev.getX(i), ev.getY(i))
                        ElementKind.DPAD -> updateDpad(e, ev.getX(i), ev.getY(i))
                        ElementKind.BUTTON -> {
                            // Deslizar o dedo para outro botao troca de botao.
                            val other = hit(ev.getX(i), ev.getY(i))
                            if (other != null && other.id != id && other.kind == ElementKind.BUTTON) {
                                releaseButton(e)
                                pointers[pid] = other.id
                                pressButton(other)
                            }
                        }
                        ElementKind.LOOK -> updateLook(ev.getX(i), ev.getY(i))
                    }
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                endPointer(ev.getPointerId(ev.actionIndex))
            }
            MotionEvent.ACTION_CANCEL -> {
                pointers.keys.toList().forEach(::endPointer)
            }
        }
        return true
    }

    private fun endPointer(pid: Int) {
        val id = pointers.remove(pid) ?: return
        val e = layout.elements.firstOrNull { it.id == id }
        if (e == null) {
            // Layout trocado com o dedo em cima: solta o que estava preso.
            sticks.remove(id)
            if (lookId == id) look = null
            return
        }
        when (e.kind) {
            ElementKind.STICK -> {
                sticks.remove(e.id)
                listener?.onStick(e.id == "rstick", 0f, 0f)
                invalidate()
            }
            ElementKind.DPAD -> {
                dpadState.remove(e.id)
                listener?.onDpad(e, emptySet())
                invalidate()
            }
            ElementKind.BUTTON -> releaseButton(e)
            ElementKind.LOOK -> {
                val st = look
                look = null
                if (st != null) {
                    if (st.moved) {
                        listener?.onStick(true, 0f, 0f)
                    } else if (SystemClock.uptimeMillis() - st.downAt < TAP_MS) {
                        listener?.onLookTap(e)
                    }
                }
                invalidate()
            }
        }
    }

    private fun pressButton(e: ControlElement) {
        if (!pressed.add(e.id)) return
        if (haptics) performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP)
        listener?.onPress(e)
        invalidate()
    }

    private fun releaseButton(e: ControlElement) {
        if (!pressed.remove(e.id)) return
        listener?.onRelease(e)
        invalidate()
    }

    private fun updateStick(e: ControlElement, px: Float, py: Float) {
        val st = sticks[e.id] ?: return
        val r = radius(e) * 0.55f
        var dx = (px - cx(e)) / r
        var dy = (py - cy(e)) / r
        val len = hypot(dx, dy)
        if (len > 1f) {
            dx /= len
            dy /= len
        }
        st.dx = dx
        st.dy = dy
        listener?.onStick(e.id == "rstick", dx, dy)
        invalidate()
    }

    private fun updateLook(px: Float, py: Float) {
        val st = look ?: return
        st.x = px
        st.y = py
        if (!st.moved && (abs(px - st.ax) > TAP_SLOP_DP * density || abs(py - st.ay) > TAP_SLOP_DP * density)) {
            st.moved = true
        }
        if (!st.moved) return
        val (dx, dy) = lookVector(st)
        listener?.onStick(true, dx, dy)
        invalidate()
    }

    private fun updateDpad(e: ControlElement, px: Float, py: Float) {
        val dx = px - cx(e)
        val dy = py - cy(e)
        val dead = radius(e) * 0.2f
        val set = HashSet<PadInput>()
        if (hypot(dx, dy) > dead) {
            val a = Math.toDegrees(atan2(dy.toDouble(), dx.toDouble()))
            // A cruz das armas/itens so tem 4 direcoes (diagonais trocavam de arma sem querer).
            if (e.id == "items") {
                when {
                    a >= -45 && a < 45 -> set += PadInput.RIGHT
                    a >= 45 && a < 135 -> set += PadInput.DOWN
                    a >= -135 && a < -45 -> set += PadInput.UP
                    else -> set += PadInput.LEFT
                }
            } else {
                if (a > -67.5 && a < 67.5) set += PadInput.RIGHT
                if (a > 112.5 || a < -112.5) set += PadInput.LEFT
                if (a > 22.5 && a < 157.5) set += PadInput.DOWN
                if (a < -22.5 && a > -157.5) set += PadInput.UP
            }
        }
        if (dpadState[e.id] != set) {
            if (set.isNotEmpty() && haptics && dpadState[e.id].isNullOrEmpty()) {
                performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP)
            }
            dpadState[e.id] = set
            listener?.onDpad(e, set)
            invalidate()
        }
    }

    /** Solta tudo (ex.: o jogo foi para segundo plano, ou o layout mudou). */
    fun releaseAll() {
        pointers.keys.toList().forEach(::endPointer)
        pressed.clear()
        sticks.clear()
        look = null
        invalidate()
    }

    // ----------------------------------------------------------------- edicao

    private fun onEditTouch(ev: MotionEvent): Boolean {
        when (ev.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                val e = hit(ev.x, ev.y)
                selectedId = e?.id
                editListener?.onSelected(e?.id)
                if (e != null) {
                    dragId = e.id
                    dragOffX = ev.x - cx(e)
                    dragOffY = ev.y - cy(e)
                }
            }
            MotionEvent.ACTION_MOVE -> {
                val id = dragId ?: return true
                if (width == 0 || height == 0) return true
                val nx = ((ev.x - dragOffX) / width).coerceIn(0f, 1f)
                val ny = ((ev.y - dragOffY) / height).coerceIn(0f, 1f)
                layout = layout.copy(elements = layout.elements.map { if (it.id == id) it.copy(x = nx, y = ny) else it })
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (dragId != null) editListener?.onMoved(layout)
                dragId = null
            }
        }
        return true
    }

    private companion object {
        val FACE = setOf(PadInput.A, PadInput.B, PadInput.X, PadInput.Y)
        /** Raio do "analogico" da camara: o dedo a esta distancia = analogico no maximo. */
        const val LOOK_RADIUS_DP = 56f
        const val TAP_MS = 220L
        const val TAP_SLOP_DP = 10f
    }
}
