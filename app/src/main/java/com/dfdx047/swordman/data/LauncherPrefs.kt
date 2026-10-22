package com.dfdx047.swordman.data

import android.content.Context
import android.content.SharedPreferences

enum class ThemeMode(val key: String) { GAME("game"), MATERIAL_YOU("material_you") }
enum class DarkMode(val key: String) { SYSTEM("system"), LIGHT("light"), DARK("dark") }
enum class AppLanguage(val tag: String) { SYSTEM(""), PORTUGUESE("pt"), ENGLISH("en"), SPANISH("es") }

/**
 * Orientacao do jogo: paisagem, vertical (o jogo em cima, controles por baixo) ou pelo sensor
 * (roda com o aparelho). Os valores sao lidos tambem pelo GameActivity (Java).
 */
enum class GameOrientation(val key: String) { LANDSCAPE("landscape"), PORTRAIT("portrait"), AUTO("auto") }

/**
 * Aspeto dos controles por toque. GAME = o estilo MGR (vidro grafite com icones
 * do jogo; o nome fica pelas preferencias gravadas); XBOX / PLAYSTATION / NINTENDO = letras ou
 * simbolos do comando.
 */
enum class ControlStyle(val key: String) { GAME("game"), XBOX("xbox"), PLAYSTATION("playstation"), NINTENDO("nintendo") }

/** Direcional na tela: analogico (padrao, o MGR e 3D) ou D-pad de 8 direcoes. */
enum class Directional(val key: String) { DPAD("dpad"), STICK("stick") }

/**
 * Controles na tela. AUTO: aparecem ao tocar na tela e somem quando se usa um
 * comando fisico; ON: sempre; OFF: nunca (a engrenagem e o pause continuam).
 */
enum class TouchMode(val key: String) { AUTO("auto"), ON("on"), OFF("off") }

/** Indicacao do botao do comando no canto de cada botao (estilo Project SwordMan). */
enum class ButtonBadge(val key: String) { XBOX("xbox"), PLAYSTATION("playstation"), NINTENDO("nintendo"), NONE("none") }

inline fun <reified T : Enum<T>> enumByKey(value: String?, default: T, key: (T) -> String): T =
    enumValues<T>().firstOrNull { key(it) == value } ?: default

/**
 * Preferencias do launcher (nao do jogo: essas vivem no mgr.toml).
 * O GameActivity le as mesmas chaves (PREFS_NAME, KEY_*).
 */
class LauncherPrefs(context: Context) {
    private val appContext = context.applicationContext

    // O jogo corre noutro processo (":game"). MODE_MULTI_PROCESS (obsoleto mas ainda
    // funcional) faz o Android reler o ficheiro quando o outro processo o mudou: sem
    // isto, o launcher gravaria por cima do que se mudou no menu dentro do jogo.
    @Suppress("DEPRECATION")
    private val prefs: SharedPreferences
        get() = appContext.getSharedPreferences(PREFS_NAME, Context.MODE_MULTI_PROCESS)

    var themeMode: ThemeMode
        get() = enumByKey(prefs.getString(KEY_THEME, null), ThemeMode.GAME) { it.key }
        set(v) = prefs.edit().putString(KEY_THEME, v.key).apply()

    var darkMode: DarkMode
        get() = enumByKey(prefs.getString(KEY_DARK, null), DarkMode.DARK) { it.key }
        set(v) = prefs.edit().putString(KEY_DARK, v.key).apply()

    var gameOrientation: GameOrientation
        get() = enumByKey(prefs.getString(KEY_ORIENTATION, null), GameOrientation.LANDSCAPE) { it.key }
        set(v) = prefs.edit().putString(KEY_ORIENTATION, v.key).apply()

    /**
     * Correcoes para o Turnip nos Adreno 6xx: bits independentes (ver TURNIP_* no
     * GameActivity, que le esta chave antes de o driver carregar).
     */
    var turnipCompatFlags: Int
        get() = prefs.getInt(KEY_TURNIP_COMPAT_FLAGS, 0)
        set(v) = prefs.edit().putInt(KEY_TURNIP_COMPAT_FLAGS, v).apply()

    /** Cheats: o jogo aperta Start sozinho no arranque para saltar as logos (GameOverlay). */
    var skipLogos: Boolean
        get() = prefs.getBoolean("cheat_skip_logos", true)
        set(v) = prefs.edit().putBoolean("cheat_skip_logos", v).apply()
    /** Segundos, desde o arranque, em que o Start e apertado. */
    var skipLogosSeconds: Int
        get() = prefs.getInt("cheat_skip_logos_seconds", 30)
        set(v) = prefs.edit().putInt("cheat_skip_logos_seconds", v).apply()

    /** Bisect do Turnip (TurnipSupport.BISECT): um passo por arranque do jogo. */
    var turnipBisect: Boolean
        get() = prefs.getBoolean("turnip_bisect", false)
        set(v) = prefs.edit().putBoolean("turnip_bisect", v).apply()
    var turnipBisectStep: Int
        get() = prefs.getInt("turnip_bisect_step", 0)
        set(v) = prefs.edit().putInt("turnip_bisect_step", v).apply()

    /** Driver alternativo que falhou [DriverBootFails.LIMIT]+ arranques seguidos; "" = nenhum. */
    val driverBootFailed: String
        get() = if (DriverBootFails.read() >= DriverBootFails.LIMIT) {
            com.dfdx047.swordman.drivers.DriverRepository.activeDir()?.let { java.io.File(it).name } ?: ""
        } else {
            ""
        }

    fun clearDriverBootState() {
        prefs.edit().remove("driver_boot_failed").remove("driver_boot_pending").apply()
        DriverBootFails.reset()
    }

    var controlStyle: ControlStyle
        get() = enumByKey(prefs.getString(KEY_CONTROL_STYLE, null), ControlStyle.GAME) { it.key }
        set(v) = prefs.edit().putString(KEY_CONTROL_STYLE, v.key).apply()

    var directional: Directional
        get() = enumByKey(prefs.getString(KEY_DIRECTIONAL, null), Directional.STICK) { it.key }
        set(v) = prefs.edit().putString(KEY_DIRECTIONAL, v.key).apply()

    var buttonBadge: ButtonBadge
        get() = enumByKey(prefs.getString(KEY_BADGE, null), ButtonBadge.XBOX) { it.key }
        set(v) = prefs.edit().putString(KEY_BADGE, v.key).apply()

    /** Pausar o jogo enquanto o menu dos controles ou o menu rapido do port estao abertos. */
    var pauseOnMenus: Boolean
        get() = prefs.getBoolean(KEY_PAUSE_MENUS, true)
        set(v) = prefs.edit().putBoolean(KEY_PAUSE_MENUS, v).apply()

    var touchMode: TouchMode
        get() {
            val v = prefs.getString(KEY_TOUCH_MODE, null)
            if (v == null && !prefs.getBoolean(KEY_TOUCH_ENABLED, true)) return TouchMode.OFF
            return enumByKey(v, TouchMode.AUTO) { it.key }
        }
        set(v) = prefs.edit().putString(KEY_TOUCH_MODE, v.key).apply()

    var touchEnabled: Boolean
        get() = prefs.getBoolean(KEY_TOUCH_ENABLED, true)
        set(v) = prefs.edit().putBoolean(KEY_TOUCH_ENABLED, v).apply()

    var touchHideWithPad: Boolean
        get() = prefs.getBoolean(KEY_TOUCH_HIDE_WITH_PAD, true)
        set(v) = prefs.edit().putBoolean(KEY_TOUCH_HIDE_WITH_PAD, v).apply()

    var touchHaptics: Boolean
        get() = prefs.getBoolean(KEY_TOUCH_HAPTICS, true)
        set(v) = prefs.edit().putBoolean(KEY_TOUCH_HAPTICS, v).apply()

    /** Opacidade global dos controles (0.1-1), multiplica a de cada botao. */
    var touchOpacity: Float
        get() = prefs.getFloat(KEY_TOUCH_OPACITY, 0.75f)
        set(v) = prefs.edit().putFloat(KEY_TOUCH_OPACITY, v).apply()

    var turnipNoticeShown: Boolean
        get() = prefs.getBoolean(KEY_TURNIP_NOTICE, false)
        set(v) = prefs.edit().putBoolean(KEY_TURNIP_NOTICE, v).apply()

    /** Aviso de primeiro arranque sobre GPUs/drivers (substitui o aviso so do Turnip). */
    var gpuNoticeShown: Boolean
        get() = prefs.getBoolean(KEY_GPU_NOTICE, false)
        set(v) = prefs.edit().putBoolean(KEY_GPU_NOTICE, v).apply()

    /** Ultima saida anormal do jogo ja mostrada (ApplicationExitInfo.timestamp). */
    var lastCrashShown: Long
        get() = prefs.getLong(KEY_LAST_CRASH_SHOWN, 0L)
        set(v) = prefs.edit().putLong(KEY_LAST_CRASH_SHOWN, v).apply()

    var checkUpdatesOnStart: Boolean
        get() = prefs.getBoolean(KEY_CHECK_UPDATES, true)
        set(v) = prefs.edit().putBoolean(KEY_CHECK_UPDATES, v).apply()

    /** Versao que o usuario mandou ignorar no pop-up de atualizacao. */
    var skippedUpdate: String?
        get() = prefs.getString(KEY_SKIPPED_UPDATE, null)
        set(v) = prefs.edit().putString(KEY_SKIPPED_UPDATE, v).apply()

    companion object {
        const val PREFS_NAME = "swordman"
        const val KEY_THEME = "theme"
        const val KEY_DARK = "dark_mode"
        const val KEY_ORIENTATION = "game_orientation"
        const val KEY_TURNIP_COMPAT_FLAGS = "turnip_compat_flags"
        const val KEY_CONTROL_STYLE = "control_style"
        const val KEY_DIRECTIONAL = "touch_directional"
        const val KEY_BADGE = "touch_badge"
        const val KEY_PAUSE_MENUS = "pause_on_menus"
        const val KEY_TOUCH_MODE = "touch_mode"
        const val KEY_TOUCH_ENABLED = "touch_enabled"
        const val KEY_TOUCH_HIDE_WITH_PAD = "touch_hide_with_pad"
        const val KEY_TOUCH_HAPTICS = "touch_haptics"
        const val KEY_TOUCH_OPACITY = "touch_opacity"
        const val KEY_TURNIP_NOTICE = "turnip_notice_shown"
        const val KEY_GPU_NOTICE = "gpu_notice_shown"
        const val KEY_LAST_CRASH_SHOWN = "last_crash_shown"
        const val KEY_CHECK_UPDATES = "check_updates"
        const val KEY_SKIPPED_UPDATE = "skipped_update"
    }
}
