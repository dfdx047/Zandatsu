
package com.dfdx047.swordman.game

import androidx.compose.runtime.Composable
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.savedstate.SavedStateRegistry
import androidx.savedstate.SavedStateRegistryController
import androidx.savedstate.SavedStateRegistryOwner
import com.dfdx047.swordman.controls.ControlsEditor
import com.dfdx047.swordman.controls.LayoutKind
import com.dfdx047.swordman.data.LauncherPrefs


/**
 * O GameActivity e um SDLActivity (Activity simples, sem Lifecycle do AndroidX).
 * Para pôr um ComposeView por cima do jogo basta um "dono" de lifecycle e de
 * saved state sempre ativo.
 */
class OverlayOwner : LifecycleOwner, SavedStateRegistryOwner {
    private val registry = LifecycleRegistry(this)
    private val savedState = SavedStateRegistryController.create(this)

    init {
        savedState.performAttach()
        savedState.performRestore(null)
        registry.currentState = Lifecycle.State.RESUMED
    }

    override val lifecycle: Lifecycle get() = registry
    override val savedStateRegistry: SavedStateRegistry get() = savedState.savedStateRegistry

    fun destroy() {
        registry.currentState = Lifecycle.State.DESTROYED
    }
}

enum class InGameScreen { CLOSED, MENU, EDITOR }

/** Acoes do menu, implementadas pelo GameOverlay. */
interface InGameActions {
    fun resume()
    fun openEditor()
    fun closeEditor()
    fun isInBattle(): Boolean
    /** Layout dos controles por toque em uso agora (luta, menus ou camara livre). */
    fun currentLayoutKind(): LayoutKind = if (isInBattle()) LayoutKind.GAMEPLAY else LayoutKind.MENU
    fun applyAppearance()
    fun exitToLauncher()
    /** Pede ao runtime uma captura de frames (logs/capture, ficheiros .dfc) para o renderizador nativo. */
    fun captureFrames()
    /** Autoteste do render (RenderSelfTest): so numa luta. */
    fun startSelfTest(): Boolean = false
    /** Taxa da tela para a taxa de quadros escolhida (60 ou 120 Hz, patch 0092). */
    fun setDisplayRate(hz: Int) {}
}

/** O menu do jogo (AndroidMenu.kt) ou o editor dos controles por cima do jogo pausado. */
@Composable
fun InGameOverlay(
    screen: InGameScreen,
    prefs: LauncherPrefs,
    actions: InGameActions,
    model: MenuModel,
    controller: MenuController,
) {
    when (screen) {
        InGameScreen.CLOSED -> Unit
        InGameScreen.EDITOR -> ControlsEditor(
            prefs,
            overGame = true,
            initialKind = actions.currentLayoutKind(),
        ) { actions.closeEditor() }
        InGameScreen.MENU -> AndroidGameMenu(model, controller)
    }
}
