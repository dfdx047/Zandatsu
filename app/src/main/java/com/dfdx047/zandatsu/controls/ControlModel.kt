package com.dfdx047.zandatsu.controls

import com.dfdx047.zandatsu.R
import com.dfdx047.zandatsu.data.AppPaths
import org.json.JSONArray
import org.json.JSONObject

/**
 * Entradas do comando virtual (Xbox 360). FORWARD/BACKWARD ficam pelos layouts antigos
 * (relativas ao lutador no Dragon Burst); no MGR nao se usam e valem esquerda/direita.
 */
enum class PadInput {
    A, B, X, Y, LB, RB, LT, RT, BACK, START, L3, R3,
    UP, DOWN, LEFT, RIGHT, FORWARD, BACKWARD,
}

/** Familia do comando para os rotulos e os selos dos botoes. */
enum class PadFamily { XBOX, PLAYSTATION, NINTENDO }

/** Um passo de uma sequencia: estas entradas durante [frames] ticks de 60 Hz. */
data class MacroStep(val inputs: Set<PadInput>, val frames: Int)

/** O que um botao no ecra faz. */
sealed class ControlAction {
    /** Carrega os botoes do comando enquanto o dedo esta em cima (1 ou varios = combinacao). */
    data class Press(val inputs: Set<PadInput>) : ControlAction()

    /**
     * Toca a sequencia ao carregar. Com [holdLast], o ultimo passo fica
     * premido enquanto o dedo estiver em cima.
     */
    data class Sequence(val steps: List<MacroStep>, val holdLast: Boolean) : ControlAction()
}

/**
 * STICK = analogico esquerdo (andar); DPAD = cruz (o "dpad" principal troca com o analogico
 * pela preferencia; os outros, como o das armas e itens, estao sempre); LOOK = zona da
 * camara (analogico direito: arrastar o dedo em qualquer sitio livre dessa zona; toque
 * rapido = R3, centrar a camara).
 */
enum class ElementKind { BUTTON, STICK, DPAD, LOOK }

/** Icone desenhado no botao (estilo MGR). */
enum class ControlIcon(val drawable: Int) {
    LIGHT(R.drawable.ic_btn_light),
    HEAVY(R.drawable.ic_btn_heavy),
    JUMP(R.drawable.ic_btn_jump),
    ACTION(R.drawable.ic_btn_action),
    BLADE(R.drawable.ic_btn_blade),
    RUN(R.drawable.ic_btn_run),
    LOCK(R.drawable.ic_btn_lock),
    SUBWEAPON(R.drawable.ic_btn_subweapon),
    DODGE(R.drawable.ic_btn_dodge),
    RIPPER(R.drawable.ic_btn_ripper),
    AUGMENT(R.drawable.ic_btn_augment),
    ITEM(R.drawable.ic_btn_item),
    SWAP(R.drawable.ic_btn_swap),
    CODEC(R.drawable.ic_btn_codec),
    START(R.drawable.ic_btn_start),
    CAMERA(R.drawable.ic_btn_camera),
    MACRO(R.drawable.ic_btn_macro),
}

/**
 * Um elemento do layout. Posicao = centro, em fracao da largura/altura do
 * ecra (em paisagem), tamanho = diametro em dp (na zona da camara: o lado).
 */
data class ControlElement(
    val id: String,
    val kind: ElementKind,
    val action: ControlAction,
    val icon: ControlIcon,
    val label: String,
    val x: Float,
    val y: Float,
    val sizeDp: Float,
    val opacity: Float = 1f,
    val visible: Boolean = true,
)

/**
 * [kind]: GAMEPLAY (combate, poucos botoes), MENU (A/B/X/Y com o simbolo do comando) ou
 * FULL (comando completo: os dois analogicos, cruz, todos os botoes). O GameOverlay troca
 * pelo estado do jogo quando o conhecer, e sempre pelo botao de troca.
 */
data class ControlLayout(
    val elements: List<ControlElement>,
    val version: Int = GameActions.LAYOUT_VERSION,
    val menu: Boolean = false,
    val full: Boolean = false,
) {
    val kind: LayoutKind get() = when {
        full -> LayoutKind.FULL
        menu -> LayoutKind.MENU
        else -> LayoutKind.GAMEPLAY
    }
}

enum class LayoutKind { GAMEPLAY, MENU, FULL }

/**
 * Comandos do Metal Gear Rising: Revengeance (Xbox 360), tela Opcoes > Controles do jogo (tipo A, lida no
 * aparelho em 27/10): LT Blade Mode; RT Ninja Run; LB usar a subarma; RB trocar o alvo travado; analogico esquerdo
 * andar; direito camara / Blade Mode livre. A saltar; B acao (portas, agarrar, Zandatsu); X ataque leve (+ analogico
 * na direcao do golpe = aparar); Y ataque forte; cruz: cima Augment Mode, esquerda/direita armas e itens, baixo usar
 * item; R3 centrar a camara; L3 + R3 Ripper Mode; Start pausa; Back Codec. Esquiva (Defensive Offense, comprada no
 * menu de personalizacao): direcao + X + A.
 * (O layout 1 tinha os gatilhos e os ombros trocados: o botao "Blade Mode" carregava o LB, o "mira" o RT...)
 *
 * No toque: o minimo de botoes. Os ataques e o salto no losango, o Blade Mode, a corrida, a
 * mira e a esquiva em arco a volta, a camara arrastando o dedo no lado direito livre do
 * ecra. O resto (Augment, Ripper, arma secundaria, itens) em botoes pequenos.
 */
object GameActions {
    /** Versao dos layouts do MGR (os ficheiros do Dragon Burst nao se reaproveitam). */
    const val LAYOUT_VERSION = 2

    val LIGHT = setOf(PadInput.X)
    val HEAVY = setOf(PadInput.Y)
    val JUMP = setOf(PadInput.A)
    val ACTION = setOf(PadInput.B)
    val BLADE = setOf(PadInput.LT)
    val RUN = setOf(PadInput.RT)
    val LOCK = setOf(PadInput.RB)
    val SUBWEAPON = setOf(PadInput.LB)
    /** Esquiva (Defensive Offense): X + A juntos (a direcao vem do analogico). */
    val DODGE = ControlAction.Press(setOf(PadInput.X, PadInput.A))
    /** Ripper Mode: L3 + R3. */
    val RIPPER = ControlAction.Press(setOf(PadInput.L3, PadInput.R3))
    /** Atalho do menu do Android (Back + Start). */
    val MENU_COMBO = setOf(PadInput.BACK, PadInput.START)

    /** Botoes que o editor pode juntar ("Adicionar"). */
    val HELPERS: List<Triple<String, ControlAction, ControlIcon>> = listOf(
        Triple("dodge", DODGE, ControlIcon.DODGE),
        Triple("ripper", RIPPER, ControlIcon.RIPPER),
        Triple("augment", ControlAction.Press(setOf(PadInput.UP)), ControlIcon.AUGMENT),
        Triple("item", ControlAction.Press(setOf(PadInput.DOWN)), ControlIcon.ITEM),
        Triple("weapon_l", ControlAction.Press(setOf(PadInput.LEFT)), ControlIcon.SWAP),
        Triple("weapon_r", ControlAction.Press(setOf(PadInput.RIGHT)), ControlIcon.SWAP),
        Triple("subweapon", ControlAction.Press(SUBWEAPON), ControlIcon.SUBWEAPON),
        Triple("center_cam", ControlAction.Press(setOf(PadInput.R3)), ControlIcon.CAMERA),
        Triple("menu", ControlAction.Press(MENU_COMBO), ControlIcon.MACRO),
    )

    private fun stick() =
        ControlElement("stick", ElementKind.STICK, ControlAction.Press(emptySet()), ControlIcon.MACRO, "", 0.14f, 0.68f, 150f)
    private fun dpad(visible: Boolean = true) =
        ControlElement("dpad", ElementKind.DPAD, ControlAction.Press(emptySet()), ControlIcon.MACRO, "", 0.14f, 0.68f, 150f,
            visible = visible)
    private fun look() =
        ControlElement("look", ElementKind.LOOK, ControlAction.Press(setOf(PadInput.R3)), ControlIcon.CAMERA, "", 0.62f, 0.42f, 300f)
    private fun button(id: String, input: Set<PadInput>, icon: ControlIcon, label: String, x: Float, y: Float, size: Float) =
        ControlElement(id, ElementKind.BUTTON, ControlAction.Press(input), icon, label, x, y, size)

    /** Combate: 4 no losango, 4 em arco, 2 pequenos em cima, a cruz das armas a esquerda. */
    fun defaultLayout(): ControlLayout {
        val cx = 0.875f
        val cy = 0.70f
        return ControlLayout(
            listOf(
                look(),
                stick(),
                dpad(),
                button("light", LIGHT, ControlIcon.LIGHT, "X", cx - 0.075f, cy, 62f),
                button("heavy", HEAVY, ControlIcon.HEAVY, "Y", cx, cy - 0.18f, 62f),
                button("action", ACTION, ControlIcon.ACTION, "B", cx + 0.075f, cy, 58f),
                button("jump", JUMP, ControlIcon.JUMP, "A", cx, cy + 0.18f, 62f),
                // Arco a volta do losango: o polegar direito chega a todos.
                button("blade", BLADE, ControlIcon.BLADE, "LT", 0.705f, 0.66f, 56f),
                ControlElement("dodge", ElementKind.BUTTON, DODGE, ControlIcon.DODGE, "X+A", 0.715f, 0.88f, 48f),
                button("run", RUN, ControlIcon.RUN, "RT", 0.735f, 0.43f, 52f),
                button("lock", LOCK, ControlIcon.LOCK, "RB", 0.81f, 0.28f, 48f),
                button("subweapon", SUBWEAPON, ControlIcon.SUBWEAPON, "LB", 0.945f, 0.30f, 44f),
                // Cruz pequena das armas/itens/Augment e o Ripper, em cima a esquerda.
                ControlElement("items", ElementKind.DPAD, ControlAction.Press(emptySet()), ControlIcon.MACRO, "", 0.075f, 0.30f, 92f),
                ControlElement("ripper", ElementKind.BUTTON, RIPPER, ControlIcon.RIPPER, "L3+R3", 0.19f, 0.25f, 40f),
                button("codec", setOf(PadInput.BACK), ControlIcon.CODEC, "BACK", 0.38f, 0.09f, 40f),
                button("start", setOf(PadInput.START), ControlIcon.START, "START", 0.62f, 0.09f, 40f),
            ),
        )
    }

    /**
     * Menus do jogo (27/10): so o direcional, confirmar (A) e voltar (B), e o Start (titulo, pausa, saltar cenas). O
     * resto dos botoes esta no comando completo (botao de troca). A cruz tem id proprio: aparece mesmo com o
     * analogico escolhido para andar.
     */
    fun defaultMenuLayout(): ControlLayout {
        val cx = 0.875f
        val cy = 0.66f
        return ControlLayout(
            listOf(
                ControlElement("m_dpad", ElementKind.DPAD, ControlAction.Press(emptySet()), ControlIcon.MACRO, "", 0.15f, 0.66f, 170f),
                button("m_a", JUMP, ControlIcon.MACRO, "A", cx - 0.03f, cy + 0.12f, 76f),
                button("m_b", ACTION, ControlIcon.MACRO, "B", cx + 0.065f, cy - 0.06f, 64f),
                button("m_start", setOf(PadInput.START), ControlIcon.START, "START", 0.62f, 0.09f, 40f),
            ),
            menu = true,
        )
    }

    /** Comando completo (os dois analogicos visiveis, cruz, L3/R3 e tudo o resto). */
    fun defaultFullLayout(): ControlLayout {
        fun pad(id: String, input: PadInput, x: Float, y: Float, size: Float) =
            button(id, setOf(input), ControlIcon.MACRO, classicLabel(input, PadFamily.XBOX), x, y, size)
        val cx = 0.875f
        val cy = 0.66f
        return ControlLayout(
            listOf(
                stick(),
                ControlElement("rstick", ElementKind.STICK, ControlAction.Press(emptySet()), ControlIcon.CAMERA, "R", 0.62f, 0.76f, 120f),
                ControlElement("dpad", ElementKind.DPAD, ControlAction.Press(emptySet()), ControlIcon.MACRO, "", 0.34f, 0.80f, 110f),
                pad("u_x", PadInput.X, cx - 0.075f, cy, 56f),
                pad("u_y", PadInput.Y, cx, cy - 0.18f, 56f),
                pad("u_b", PadInput.B, cx + 0.075f, cy, 56f),
                pad("u_a", PadInput.A, cx, cy + 0.18f, 56f),
                pad("u_lb", PadInput.LB, 0.06f, 0.36f, 48f),
                pad("u_lt", PadInput.LT, 0.06f, 0.18f, 48f),
                pad("u_rb", PadInput.RB, 0.94f, 0.36f, 48f),
                pad("u_rt", PadInput.RT, 0.94f, 0.18f, 48f),
                pad("u_l3", PadInput.L3, 0.24f, 0.44f, 40f),
                pad("u_r3", PadInput.R3, 0.76f, 0.44f, 40f),
                button("u_back", setOf(PadInput.BACK), ControlIcon.CODEC, "BACK", 0.38f, 0.09f, 40f),
                button("u_start", setOf(PadInput.START), ControlIcon.START, "START", 0.62f, 0.09f, 40f),
            ),
            menu = true,
            full = true,
        )
    }

    fun defaultLayout(kind: LayoutKind): ControlLayout = when (kind) {
        LayoutKind.MENU -> defaultMenuLayout()
        LayoutKind.FULL -> defaultFullLayout()
        LayoutKind.GAMEPLAY -> defaultLayout()
    }

    /**
     * Rotulo do botao no comando de cada familia. Nintendo: a posicao manda (o A do Xbox, em
     * baixo, e o B da Nintendo; o B do Xbox, a direita, e o A), como nos comandos da Switch.
     */
    fun classicLabel(input: PadInput, family: PadFamily): String = when (family) {
        PadFamily.XBOX -> when (input) {
            PadInput.BACK -> "BACK"
            PadInput.START -> "START"
            else -> common(input) ?: input.name
        }
        PadFamily.PLAYSTATION -> when (input) {
            PadInput.A -> "✕"
            PadInput.B -> "○"
            PadInput.X -> "□"
            PadInput.Y -> "△"
            PadInput.LB -> "L1"
            PadInput.RB -> "R1"
            PadInput.LT -> "L2"
            PadInput.RT -> "R2"
            PadInput.BACK -> "SELECT"
            PadInput.START -> "START"
            else -> common(input) ?: input.name
        }
        PadFamily.NINTENDO -> when (input) {
            PadInput.A -> "B"
            PadInput.B -> "A"
            PadInput.X -> "Y"
            PadInput.Y -> "X"
            PadInput.LB -> "L"
            PadInput.RB -> "R"
            PadInput.LT -> "ZL"
            PadInput.RT -> "ZR"
            PadInput.BACK -> "−"
            PadInput.START -> "+"
            else -> common(input) ?: input.name
        }
    }

    /** Compatibilidade: rotulo Xbox ou PlayStation. */
    fun classicLabel(input: PadInput, playstation: Boolean): String =
        classicLabel(input, if (playstation) PadFamily.PLAYSTATION else PadFamily.XBOX)

    private fun common(input: PadInput): String? = when (input) {
        PadInput.A -> "A"
        PadInput.B -> "B"
        PadInput.X -> "X"
        PadInput.Y -> "Y"
        PadInput.LB -> "LB"
        PadInput.RB -> "RB"
        PadInput.LT -> "LT"
        PadInput.RT -> "RT"
        PadInput.L3 -> "L3"
        PadInput.R3 -> "R3"
        PadInput.UP -> "↑"
        PadInput.DOWN -> "↓"
        PadInput.LEFT, PadInput.BACKWARD -> "←"
        PadInput.RIGHT, PadInput.FORWARD -> "→"
        else -> null
    }
}

/** Gravacao do layout em JSON (pasta interna da app). */
object ControlLayoutStore {

    private fun file(kind: LayoutKind) = when (kind) {
        LayoutKind.MENU -> AppPaths.controlsMenuFile
        LayoutKind.FULL -> AppPaths.controlsMapFile
        LayoutKind.GAMEPLAY -> AppPaths.controlsFile
    }

    fun load(kind: LayoutKind = LayoutKind.GAMEPLAY): ControlLayout {
        val f = file(kind)
        if (!f.isFile) return GameActions.defaultLayout(kind)
        return try {
            val saved = fromJson(JSONObject(f.readText()))
                .copy(menu = kind != LayoutKind.GAMEPLAY, full = kind == LayoutKind.FULL)
            // 27/10: o layout 1 de combate tinha gatilhos e ombros trocados; fica o do usuario (posicoes, tamanhos)
            // com cada botao na entrada certa. Os menus do layout 1 dao lugar aos novos (so direcional + A/B).
            if (saved.version == 1 && kind != LayoutKind.MENU && saved.elements.isNotEmpty()) {
                migrateV1(saved)
            } else if (saved.version != GameActions.LAYOUT_VERSION || saved.elements.isEmpty()) {
                GameActions.defaultLayout(kind)
            } else {
                saved
            }
        } catch (e: Exception) {
            com.dfdx047.zandatsu.data.AppLog.w("controls", "layout ${kind.name} invalido, uso o de omissao", e)
            GameActions.defaultLayout(kind)
        }
    }

    /** Layout 1 -> 2: os botoes de omissao do Blade Mode, Ninja Run, mira e subarma passam a entrada certa. */
    private fun migrateV1(saved: ControlLayout): ControlLayout {
        val fix = mapOf(
            "blade" to (setOf(PadInput.LB) to GameActions.BLADE),
            "run" to (setOf(PadInput.RB) to GameActions.RUN),
            "lock" to (setOf(PadInput.RT) to GameActions.LOCK),
            "subweapon" to (setOf(PadInput.LT) to GameActions.SUBWEAPON),
        )
        val elements = saved.elements.map { e ->
            val (old, new) = fix[e.id] ?: return@map e
            val press = e.action as? ControlAction.Press
            if (press?.inputs == old) e.copy(action = ControlAction.Press(new), label = new.first().name) else e
        }
        com.dfdx047.zandatsu.data.AppLog.i("controls", "layout 1 -> 2 (gatilhos e ombros do MGR)")
        return saved.copy(elements = elements, version = GameActions.LAYOUT_VERSION)
    }

    fun save(layout: ControlLayout) {
        val f = file(layout.kind)
        f.parentFile?.mkdirs()
        val tmp = java.io.File(f.parentFile, f.name + ".tmp")
        tmp.writeText(toJson(layout).toString(2))
        if (!tmp.renameTo(f)) {
            f.writeText(tmp.readText())
            tmp.delete()
        }
    }

    fun reset() {
        AppPaths.controlsFile.delete()
        AppPaths.controlsMenuFile.delete()
        AppPaths.controlsMapFile.delete()
    }

    private fun inputsToJson(s: Set<PadInput>) = JSONArray().apply { s.forEach { put(it.name) } }
    private fun inputsFromJson(a: JSONArray?): Set<PadInput> =
        if (a == null) emptySet() else (0 until a.length()).mapNotNull { i ->
            runCatching { PadInput.valueOf(a.getString(i)) }.getOrNull()
        }.toSet()

    private fun actionToJson(a: ControlAction): JSONObject = when (a) {
        is ControlAction.Press -> JSONObject().put("type", "press").put("inputs", inputsToJson(a.inputs))
        is ControlAction.Sequence -> JSONObject().put("type", "sequence").put("holdLast", a.holdLast)
            .put("steps", JSONArray().apply {
                a.steps.forEach { put(JSONObject().put("inputs", inputsToJson(it.inputs)).put("frames", it.frames)) }
            })
    }

    private fun actionFromJson(o: JSONObject): ControlAction = when (o.optString("type")) {
        "sequence" -> {
            val steps = o.optJSONArray("steps") ?: JSONArray()
            ControlAction.Sequence(
                (0 until steps.length()).map { i ->
                    val s = steps.getJSONObject(i)
                    MacroStep(inputsFromJson(s.optJSONArray("inputs")), s.optInt("frames", 2).coerceIn(1, 600))
                },
                o.optBoolean("holdLast", false),
            )
        }
        else -> ControlAction.Press(inputsFromJson(o.optJSONArray("inputs")))
    }

    fun toJson(layout: ControlLayout): JSONObject = JSONObject().put("version", layout.version).put(
        "elements",
        JSONArray().apply {
            layout.elements.forEach { e ->
                put(
                    JSONObject()
                        .put("id", e.id).put("kind", e.kind.name).put("action", actionToJson(e.action))
                        .put("icon", e.icon.name).put("label", e.label)
                        .put("x", e.x.toDouble()).put("y", e.y.toDouble()).put("size", e.sizeDp.toDouble())
                        .put("opacity", e.opacity.toDouble()).put("visible", e.visible),
                )
            }
        },
    )

    fun fromJson(o: JSONObject): ControlLayout {
        val arr = o.getJSONArray("elements")
        val list = (0 until arr.length()).map { i ->
            val e = arr.getJSONObject(i)
            ControlElement(
                id = e.getString("id"),
                kind = runCatching { ElementKind.valueOf(e.getString("kind")) }.getOrDefault(ElementKind.BUTTON),
                action = actionFromJson(e.optJSONObject("action") ?: JSONObject()),
                icon = runCatching { ControlIcon.valueOf(e.optString("icon")) }.getOrDefault(ControlIcon.MACRO),
                label = e.optString("label"),
                x = e.optDouble("x", 0.5).toFloat().coerceIn(0f, 1f),
                y = e.optDouble("y", 0.5).toFloat().coerceIn(0f, 1f),
                sizeDp = e.optDouble("size", 60.0).toFloat().coerceIn(28f, 600f),
                opacity = e.optDouble("opacity", 1.0).toFloat().coerceIn(0.05f, 1f),
                visible = e.optBoolean("visible", true),
            )
        }
        return ControlLayout(list, o.optInt("version", 1))
    }
}
