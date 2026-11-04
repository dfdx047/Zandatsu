package com.dfdx047.zandatsu

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import com.dfdx047.zandatsu.ui.LauncherApp
import com.dfdx047.zandatsu.ui.LauncherViewModel
import com.dfdx047.zandatsu.ui.theme.SwordManTheme

/**
 * Launcher Zandatsu (Jetpack Compose). AppCompatActivity por causa do idioma
 * por app (pt/en) tambem em Android 11 e 12.
 */
class LauncherActivity : AppCompatActivity() {
    private val vm: LauncherViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        setContent {
            SwordManTheme(themeMode = vm.themeMode, darkMode = vm.darkMode) {
                LauncherApp(vm)
            }
        }
        vm.checkUpdatesOnLaunch()
        handleDiagIntent(intent)
    }

    override fun onNewIntent(intent: android.content.Intent) {
        super.onNewIntent(intent)
        handleDiagIntent(intent)
    }

    /**
     * Teste de GPU pelo adb: am start -n com.dfdx047.zandatsu/.LauncherActivity -a com.dfdx047.zandatsu.DIAG
     * [--es diag_options force_crash=viewport_scissor/minimap/area_full] [--ez prompt true].
     */
    private fun handleDiagIntent(intent: android.content.Intent?) {
        if (intent?.action != "com.dfdx047.zandatsu.DIAG") return
        com.dfdx047.zandatsu.diag.DiagRunner.options = intent.getStringExtra("diag_options").orEmpty()
        intent.action = null
        // --ez prompt true: so o aviso de arranque (para o ver numa GPU sem problema conhecido).
        if (intent.getBooleanExtra("prompt", false)) vm.diagPromptGpu = com.dfdx047.zandatsu.diag.DiagRunner.gpu().name
        else vm.startDiag(this)
    }

    override fun onResume() {
        super.onResume()
        // Varredura do Turnip: continua sozinha (sem avisos de erro pelo meio).
        if (vm.continueTurnipSweep(this)) return
        // Ao voltar do jogo: o menu rapido pode ter mudado opcoes e ha log novo.
        vm.reloadAfterGame()
    }
}
