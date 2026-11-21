package com.dfdx047.zandatsu.pc.data

import android.content.Context
import android.content.SharedPreferences

enum class ThemeMode(val key: String) { GAME("game"), MATERIAL_YOU("material_you") }
enum class DarkMode(val key: String) { SYSTEM("system"), LIGHT("light"), DARK("dark") }
enum class AppLanguage(val tag: String) { SYSTEM(""), PORTUGUESE("pt"), ENGLISH("en"), SPANISH("es") }

/** Visual dos controles na tela (copiado da app do 360). */
enum class ControlStyle(val key: String) {
    GAME("game"), SCIFI("scifi"), XBOX("xbox"), PLAYSTATION("playstation"), NINTENDO("nintendo");

    val iconic: Boolean get() = this == GAME || this == SCIFI
}

enum class Directional(val key: String) { DPAD("dpad"), STICK("stick") }

/** Indicacao do botao do comando no canto de cada botao. */
enum class ButtonBadge(val key: String) { XBOX("xbox"), PLAYSTATION("playstation"), NINTENDO("nintendo"), NONE("none") }

/**
 * Modo de toque do jogo de PC (MGR_TOUCH_MODE): BUTTONS = os controles desenhados da app do 360; GESTURES e MOUSE =
 * sem botoes (so o de menu), o toque vai direto para o nativo.
 */
enum class PcTouchMode(val key: String) { BUTTONS("buttons"), GESTURES("gestures"), MOUSE("mouse") }

inline fun <reified T : Enum<T>> enumByKey(value: String?, default: T, key: (T) -> String): T =
    enumValues<T>().firstOrNull { key(it) == value } ?: default

/** Preferencias dos controles por toque (as mesmas chaves da app do 360, noutro pacote). */
class LauncherPrefs(context: Context) {
    private val appContext = context.applicationContext

    // O jogo corre noutro processo (":pcgame"): MODE_MULTI_PROCESS rele o ficheiro se o outro processo o mudou.
    @Suppress("DEPRECATION")
    private val prefs: SharedPreferences
        get() = appContext.getSharedPreferences(PREFS_NAME, Context.MODE_MULTI_PROCESS)

    val themeMode: ThemeMode get() = ThemeMode.GAME
    val darkMode: DarkMode get() = DarkMode.DARK

    var controlStyle: ControlStyle
        get() = enumByKey(prefs.getString("control_style", null), ControlStyle.GAME) { it.key }
        set(v) = prefs.edit().putString("control_style", v.key).apply()

    var directional: Directional
        get() = enumByKey(prefs.getString("touch_directional", null), Directional.STICK) { it.key }
        set(v) = prefs.edit().putString("touch_directional", v.key).apply()

    var buttonBadge: ButtonBadge
        get() = enumByKey(prefs.getString("touch_badge", null), ButtonBadge.XBOX) { it.key }
        set(v) = prefs.edit().putString("touch_badge", v.key).apply()

    /** Pausar o jogo enquanto o menu dos controles esta aberto. */
    var pauseOnMenus: Boolean
        get() = prefs.getBoolean("pause_on_menus", true)
        set(v) = prefs.edit().putBoolean("pause_on_menus", v).apply()

    var pcTouchMode: PcTouchMode
        get() = enumByKey(prefs.getString("pc_touch_mode", null), PcTouchMode.BUTTONS) { it.key }
        set(v) = prefs.edit().putString("pc_touch_mode", v.key).apply()

    var touchHaptics: Boolean
        get() = prefs.getBoolean("touch_haptics", true)
        set(v) = prefs.edit().putBoolean("touch_haptics", v).apply()

    var touchAutoSwitch: Boolean
        get() = prefs.getBoolean("touch_auto_switch", true)
        set(value) = prefs.edit().putBoolean("touch_auto_switch", value).apply()

    /** LT alterna: um toque entra no Blade Mode e outro sai (em vez de segurar). */
    var bladeToggle: Boolean
        get() = prefs.getBoolean("touch_blade_toggle", true)
        set(value) = prefs.edit().putBoolean("touch_blade_toggle", value).apply()

    var touchScale: Float
        get() = prefs.getFloat("touch_scale", 1f)
        set(value) = prefs.edit().putFloat("touch_scale", value).apply()

    var touchOpacity: Float
        get() = prefs.getFloat("touch_opacity", 0.75f)
        set(v) = prefs.edit().putFloat("touch_opacity", v).apply()

    companion object {
        const val PREFS_NAME = "zandatsu_pc"
    }
}
