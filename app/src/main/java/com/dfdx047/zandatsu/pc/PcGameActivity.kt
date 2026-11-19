package com.dfdx047.zandatsu.pc

import android.app.NativeActivity
import android.os.Bundle
import android.system.Os
import android.util.Log
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import com.dfdx047.zandatsu.drivers.DriverRepository
import java.io.File

/**
 * Jogo de PC (MGR:R) num processo proprio (:pcgame), como o GameActivity do 360. NativeActivity: o laco nativo e o
 * pc/android/mgr_android_main.cpp, carregado pelo libmgrlab.so (so o carregador, sem codigo do jogo).
 * Antes do super.onCreate: desencripta o libgame para um memfd e passa tudo ao nativo por variaveis de ambiente
 * (MGR_LIB_FD, MGR_EXE, MGR_DRIVER_DIR/NAME, MGR_HOOK_DIR, MGR_RENDER_SIZE).
 */
class PcGameActivity : NativeActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        try {
            val exe = File(PcGame.exePath(this))
            if (!exe.isFile) throw PcGame.PcException("executavel nao encontrado: ${exe.path}")
            val t0 = System.nanoTime()
            val fd = PcGame.decryptToMemfd(this, exe)
            Log.i(TAG, "libmgrpc desencriptado em ${(System.nanoTime() - t0) / 1_000_000} ms (fd $fd)")
            Os.setenv("MGR_LIB_FD", fd.toString(), true)
            Os.setenv("MGR_EXE", exe.absolutePath, true)
            Os.setenv("MGR_HOOK_DIR", applicationInfo.nativeLibraryDir + "/", true)
            Os.setenv("MGR_RENDER_SIZE", PcGame.renderSize(this), true)
            val drv = PcGame.driverDir(this)
            val installed = DriverRepository.installed().firstOrNull { it.dir.absolutePath == drv }
            if (installed != null) {
                // adrenotools abre o driver com dlopen: tem de estar numa pasta interna (o externo e noexec).
                val dir = File(filesDir, "pcdriver").apply { mkdirs() }
                val lib = File(dir, installed.library.name)
                if (lib.length() != installed.library.length()) installed.library.copyTo(lib, overwrite = true)
                Os.setenv("MGR_DRIVER_DIR", dir.absolutePath + "/", true)
                Os.setenv("MGR_DRIVER_NAME", lib.name, true)
                Log.i(TAG, "driver ${installed.name} (${installed.library})")
            } else Log.i(TAG, "driver do sistema")
        } catch (e: Exception) {
            Log.e(TAG, "nao arrancou", e)
            Os.setenv("MGR_START_ERROR", e.message ?: e.toString(), true)
        }
        super.onCreate(savedInstanceState)
        immersive()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) immersive()
    }

    private fun immersive() {
        window.decorView.windowInsetsController?.let {
            it.hide(WindowInsets.Type.systemBars())
            it.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    companion object { private const val TAG = "MGRPC" }
}
