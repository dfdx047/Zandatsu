package com.dfdx047.swordman.data

import android.content.Context
import java.io.File

/**
 * Pastas do app. A pasta de dados e a mesma que o GameActivity passa ao runtime em
 * REX_APP_DIR (getExternalFilesDir, ou seja /sdcard/Android/data/com.dfdx047.swordman/files).
 * No Android o SDK trata-a como a "pasta do executavel" (filesystem_posix.cpp):
 *
 *   <appDir>/game/              jogo (default.xex + media/), importado da ISO
 *   <appDir>/mgr.toml           configuracoes do runtime (ver GameConfig)
 *   <appDir>/cvars.txt          overrides manuais para testes (GameActivity)
 *   <appDir>/capture.req        pedido de captura de frames (o runtime le-o durante o jogo)
 *   <appDir>/driver/            driver Vulkan ativo (o GameActivity copia-o para a pasta interna)
 *   <appDir>/drivers/           drivers instalados (um por pasta)
 *   <appDir>/logs/              log da ultima sessao, capturas, vigia e registo do app
 */
object AppPaths {
    lateinit var appDir: File
        private set
    lateinit var internalDir: File
        private set
    lateinit var cacheDir: File
        private set

    fun init(context: Context) {
        appDir = (context.getExternalFilesDir(null) ?: context.filesDir).also { it.mkdirs() }
        internalDir = context.filesDir
        cacheDir = context.cacheDir
    }

    val gameDir: File get() = File(appDir, "game")
    val xexFile: File get() = File(gameDir, "default.xex")
    val dataDir: File get() = File(gameDir, "media")
    val userToml: File get() = File(appDir, "mgr.toml")
    val logsDir: File get() = File(appDir, "logs")
    val lastRunLog: File get() = File(logsDir, "swordman_last.log")
    /** Registo do proprio app (launcher, importacao, drivers, menu do jogo): AppLog. */
    val appLog: File get() = File(logsDir, "app.log")
    val driversDir: File get() = File(appDir, "drivers")
    val activeDriverDir: File get() = File(appDir, "driver")
    val controlsFile: File get() = File(internalDir, "controls/layout_mgr_v1.json")
    val controlsMenuFile: File get() = File(internalDir, "controls/layout_mgr_menu_v1.json")
    /** Blade Mode livre (comando completo, os dois analogicos). */
    val controlsMapFile: File get() = File(internalDir, "controls/layout_mgr_full_v1.json")
    val updatesDir: File get() = File(cacheDir, "updates")
}
