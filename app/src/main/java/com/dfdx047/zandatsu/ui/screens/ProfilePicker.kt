package com.dfdx047.zandatsu.ui.screens

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import com.dfdx047.zandatsu.R
import com.dfdx047.zandatsu.ui.LauncherViewModel
import com.dfdx047.zandatsu.ui.components.Level
import com.dfdx047.zandatsu.ui.components.StatusLine

/**
 * O fps de cada perfil (Minimo .. Ultra) medido em 60 s de luta (tools/renut-fight.sh, saves de teste, 24/10):
 * RP5 no Renut com o Turnip (fps de jogo; GPU 9.6 / 10.3 / 9.8 / 13.6 / 13.2 ms), RG Cube no Xenia (Mali-G57, sem
 * Renut; Minimo e Baixo sao os de 30 FPS). null = nao medido.
 */
object ProfileFps {
    val RP5: List<Double?> = listOf(58.1, 56.8, 58.2, 55.8, 57.3)
    val CUBE: List<Double?> = listOf(36.4, 41.5, 14.7, 10.4, 6.7)
}

/**
 * Primeiro arranque: "Escolha seu perfil". Os cinco perfis com o fps medido no RP5 e no Cube ao lado; nada vem
 * escolhido e o ecra so fecha com uma escolha (muda-se depois no Desempenho).
 */
@Composable
fun ProfilePicker(vm: LauncherViewModel) {
    val renut = vm.config.useRenut
    val names = listOf(
        R.string.preset_minimum, R.string.preset_light, R.string.preset_balanced, R.string.preset_high,
        R.string.preset_ultra,
    )
    val descs = if (renut) {
        listOf(
            R.string.renut_preset_minimum_desc, R.string.renut_preset_low_desc, R.string.renut_preset_medium_desc,
            R.string.renut_preset_high_desc, R.string.renut_preset_ultra_desc,
        )
    } else {
        listOf(
            R.string.preset_minimum_desc, R.string.preset_light_desc, R.string.preset_balanced_desc,
            R.string.preset_high_desc, R.string.preset_ultra_desc,
        )
    }
    Dialog(
        onDismissRequest = {},
        properties = DialogProperties(
            dismissOnBackPress = false,
            dismissOnClickOutside = false,
            usePlatformDefaultWidth = false,
        ),
    ) {
        Surface(Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
            Column(
                Modifier.verticalScroll(rememberScrollState()).padding(24.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                Text(stringResource(R.string.profile_pick_title), style = MaterialTheme.typography.headlineSmall)
                Text(
                    stringResource(
                        R.string.profile_pick_sub,
                        stringResource(if (renut) R.string.renderer_renut else R.string.renderer_xenos),
                    ),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                if (com.dfdx047.zandatsu.data.RenutSupport.systemDriverOn6xx()) {
                    StatusLine(stringResource(R.string.drivers_renut_system_6xx), Level.ERROR)
                }
                BoxWithConstraints(Modifier.fillMaxWidth()) {
                    val cards: @Composable (Modifier) -> Unit = { m ->
                        names.indices.forEach { i ->
                            ProfileCard(
                                name = stringResource(names[i]),
                                desc = stringResource(descs[i]),
                                rp5 = ProfileFps.RP5[i],
                                cube = ProfileFps.CUBE[i],
                                modifier = m,
                            ) { vm.chooseProfile(i) }
                        }
                    }
                    if (maxWidth > 720.dp) {
                        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) { cards(Modifier.weight(1f)) }
                    } else {
                        Column(verticalArrangement = Arrangement.spacedBy(10.dp)) { cards(Modifier.fillMaxWidth()) }
                    }
                }
            }
        }
    }
}

@Composable
private fun ProfileCard(
    name: String,
    desc: String,
    rp5: Double?,
    cube: Double?,
    modifier: Modifier,
    onClick: () -> Unit,
) {
    val scheme = MaterialTheme.colorScheme
    val fps = { v: Double? -> if (v == null) "—" else "%.0f fps".format(v) }
    Card(
        onClick = onClick,
        modifier = modifier,
        shape = RoundedCornerShape(14.dp),
        colors = CardDefaults.cardColors(containerColor = scheme.surfaceContainerHigh),
        border = BorderStroke(1.dp, scheme.outlineVariant),
    ) {
        Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(name, style = MaterialTheme.typography.titleMedium, color = scheme.primary)
            Text(desc, style = MaterialTheme.typography.bodySmall)
            Text(
                stringResource(R.string.profile_pick_fps_rp5, fps(rp5)),
                style = MaterialTheme.typography.labelLarge,
            )
            Text(
                stringResource(R.string.profile_pick_fps_cube, fps(cube)),
                style = MaterialTheme.typography.labelLarge,
            )
        }
    }
}
