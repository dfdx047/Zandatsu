package com.dfdx047.zandatsu.pc.data

import android.content.Context
import java.io.File

/**
 * Pastas da app de PC (com.dfdx047.zandatsu.pc, separada da app do 360):
 *
 *   <appDir>/drivers/    drivers Vulkan instalados (um por pasta, formato AdrenoTools)
 *   <appDir>/logs/       registo da app (app.log)
 *   <filesDir>/controls  layouts dos controles por toque
 */
object AppPaths {
    lateinit var appDir: File
        private set
    lateinit var internalDir: File
        private set
    lateinit var cacheDir: File
        private set

    fun init(context: Context) {
        if (::appDir.isInitialized) return
        appDir = (context.getExternalFilesDir(null) ?: context.filesDir).also { it.mkdirs() }
        internalDir = context.filesDir
        cacheDir = context.cacheDir
    }

    val logsDir: File get() = File(appDir, "logs")
    val appLog: File get() = File(logsDir, "app.log")
    val driversDir: File get() = File(appDir, "drivers")
    val controlsFile: File get() = File(internalDir, "controls/layout_mgr_v1.json")
    val controlsMenuFile: File get() = File(internalDir, "controls/layout_mgr_menu_v1.json")
    val controlsMapFile: File get() = File(internalDir, "controls/layout_mgr_full_v1.json")
}
