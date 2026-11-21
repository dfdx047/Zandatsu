package com.dfdx047.zandatsu.pc

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import com.dfdx047.zandatsu.pc.data.AppPaths
import com.dfdx047.zandatsu.pc.ui.LauncherApp
import com.dfdx047.zandatsu.pc.ui.LauncherViewModel
import com.dfdx047.zandatsu.pc.ui.theme.SwordManTheme

/**
 * Launcher da app Zandatsu PC: a mesma launcher da app do 360 (Inicio com JOGAR, Graficos, Controles, Drivers, Mais
 * com Sistema e Sobre), com os graficos e os controles do port de PC. As opcoes ficam nas preferencias (PcGame,
 * GraphicOptions, LauncherPrefs) e o PcGameActivity passa-as ao nativo por variaveis de ambiente. AppCompatActivity
 * por causa do idioma por app tambem em Android 11 e 12.
 */
class PcLauncherActivity : AppCompatActivity() {
    private val vm: LauncherViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        AppPaths.init(this)
        setContent {
            SwordManTheme(themeMode = vm.themeMode, darkMode = vm.darkMode) {
                LauncherApp(vm)
            }
        }
        vm.checkUpdatesOnLaunch()
    }

    override fun onResume() {
        super.onResume()
        // Volta de outra app (permissao, SAF, jogo, editor, mapeador): os ecras releem o estado.
        vm.onResume()
    }
}
