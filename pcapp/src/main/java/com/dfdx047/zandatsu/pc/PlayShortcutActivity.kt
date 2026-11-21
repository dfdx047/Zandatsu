package com.dfdx047.zandatsu.pc

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.os.Environment
import com.dfdx047.zandatsu.pc.data.AppLog
import com.dfdx047.zandatsu.pc.data.AppPaths
import java.io.File

/**
 * Atalho da tela inicial (como o da app do 360): sem ecra. Com o jogo pronto (exe, acesso aos ficheiros, codigo no
 * APK) grava as opcoes do jogo, como o JOGAR da launcher, e abre o PcGameActivity; senao abre a launcher.
 */
class PlayShortcutActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        AppPaths.init(this)
        val exe = PcGame.exePath(this)
        val ready = exe.isNotEmpty() && File(exe).isFile && Environment.isExternalStorageManager() && PcGame.available(this)
        if (!ready) {
            AppLog.i("shortcut", "jogo nao esta pronto: abre a launcher")
            startActivity(Intent(this, PcLauncherActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
            finish()
            return
        }
        Thread {
            val r = runCatching { GraphicOptions.write(this) }.getOrNull()
            if (r?.ok != true) AppLog.w("shortcut", "opcoes do jogo nao gravadas: ${r?.error}")
            runOnUiThread {
                startActivity(Intent(this, PcGameActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
                finish()
            }
        }.start()
    }

    companion object {
        const val ACTION = "com.dfdx047.zandatsu.pc.PLAY"
    }
}
