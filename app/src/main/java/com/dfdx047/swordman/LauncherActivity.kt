package com.dfdx047.swordman

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import com.dfdx047.swordman.ui.LauncherApp
import com.dfdx047.swordman.ui.LauncherViewModel
import com.dfdx047.swordman.ui.theme.SwordManTheme

/**
 * Launcher Project SwordMan (Jetpack Compose). AppCompatActivity por causa do idioma
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
    }

    override fun onResume() {
        super.onResume()
        // Varredura do Turnip: continua sozinha (sem avisos de erro pelo meio).
        if (vm.continueTurnipSweep(this)) return
        // Ao voltar do jogo: o menu rapido pode ter mudado opcoes e ha log novo.
        vm.reloadAfterGame()
    }
}
