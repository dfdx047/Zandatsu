package com.dfdx047.swordman

import android.app.Application
import com.dfdx047.swordman.data.AppLog
import com.dfdx047.swordman.data.AppPaths

class SwordManApp : Application() {
    override fun onCreate() {
        super.onCreate()
        AppPaths.init(this)
        // O jogo corre no processo ":game" (GameActivity muda-o para "game" no AppLog).
        val proc = getProcessName().substringAfter(':', "app")
        AppLog.process = proc
        AppLog.i("app", "arranque ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE}) processo $proc")
        // Crash em Kotlin/Java (launcher ou menus do jogo): fica no app.log antes de o processo morrer.
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { t, e ->
            try {
                AppLog.e("crash", "excecao nao apanhada na thread ${t.name}: ${e.stackTraceToString().take(4000)}")
                Thread.sleep(150)  // deixa a thread do log escrever
            } catch (_: Throwable) {
            }
            previous?.uncaughtException(t, e)
        }
    }

    companion object {
        /** Repositorio do port (pagina "Sobre" e verificacao de atualizacoes). */
        const val GITHUB_OWNER = "dfdx047"
        const val GITHUB_REPO = "ProjectSwordMan"
        const val GITHUB_URL = "https://github.com/$GITHUB_OWNER/$GITHUB_REPO"
        const val DEVELOPER = "dfdx047"
        const val DISCORD_URL = "https://discord.gg/nXSANX9V6k"
        const val YOUTUBE_URL = "https://www.youtube.com/@DFDX47"
        const val KOFI_URL = "https://ko-fi.com/dfdx047"
        /** SDK de recompilacao estatica usado (fork com os patches Android do Dragon Burst). */
        const val ORIGINAL_PROJECT_URL = "https://github.com/iExplosiveRage/rexglue-sdk"
    }
}
