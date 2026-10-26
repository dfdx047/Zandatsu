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
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import com.dfdx047.zandatsu.R
import com.dfdx047.zandatsu.ui.LauncherViewModel

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
 * Primeiro arranque (26/10): "Detectamos a sua GPU" e o perfil sugerido para ela (GpuSuggestion: aparelho testado ou
 * familia), com Aceitar / Escolher eu mesmo. Nada e aplicado sem o jogador aceitar; "Escolher eu mesmo" mostra os
 * cinco perfis com o aparelho-alvo de cada um (Renut) ou o fps no RG Cube (Xenia). O ecra so fecha com uma escolha
 * (muda-se depois no Desempenho).
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
    val suggestion = remember { com.dfdx047.zandatsu.data.GpuSuggestion.detect() }
    val gpu = remember { com.dfdx047.zandatsu.drivers.DriverRepository.gpuName().ifEmpty { com.dfdx047.zandatsu.drivers.DriverRepository.socName() } }
    var chooseMyself by remember { mutableStateOf(false) }
    val target = { i: Int ->
        when {
            renut -> com.dfdx047.zandatsu.data.RENUT_PRESET_TARGETS[i]
            // Xenia: so o RG Cube foi medido; numa Adreno (driver do sistema) esse numero enganaria.
            com.dfdx047.zandatsu.drivers.DriverRepository.isAdreno() -> null
            else -> ProfileFps.CUBE[i]?.let { "RG Cube (Xenia): %.0f fps".format(it) }
        }
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
                Text(stringResource(R.string.gpu_detected_title, gpu), style = MaterialTheme.typography.headlineSmall)
                Text(
                    if (suggestion.known != null) stringResource(R.string.gpu_detected_known, suggestion.known)
                    else stringResource(R.string.gpu_detected_family, suggestion.family),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Text(
                    stringResource(
                        R.string.profile_pick_renderer,
                        stringResource(if (renut) R.string.renderer_renut else R.string.renderer_xenos),
                    ),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                com.dfdx047.zandatsu.ui.components.TurnipWarning(vm)
                if (!chooseMyself) {
                    Text(stringResource(R.string.profile_suggested), style = MaterialTheme.typography.titleMedium)
                    ProfileCard(
                        name = stringResource(names[suggestion.tier]),
                        desc = stringResource(descs[suggestion.tier]),
                        target = target(suggestion.tier),
                        modifier = Modifier.fillMaxWidth(),
                        onClick = null,
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                        Button(onClick = { vm.chooseProfile(suggestion.tier) }) { Text(stringResource(R.string.profile_accept)) }
                        OutlinedButton(onClick = { chooseMyself = true }) { Text(stringResource(R.string.profile_choose_myself)) }
                    }
                } else {
                    Text(stringResource(R.string.profile_pick_title), style = MaterialTheme.typography.titleMedium)
                    BoxWithConstraints(Modifier.fillMaxWidth()) {
                        val cards: @Composable (Modifier) -> Unit = { m ->
                            names.indices.forEach { i ->
                                ProfileCard(
                                    name = stringResource(names[i]) +
                                        if (i == suggestion.tier) " · " + stringResource(R.string.profile_suggested_tag) else "",
                                    desc = stringResource(descs[i]),
                                    target = target(i),
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
}

@Composable
private fun ProfileCard(
    name: String,
    desc: String,
    target: String?,
    modifier: Modifier,
    onClick: (() -> Unit)?,
) {
    val scheme = MaterialTheme.colorScheme
    val content: @Composable () -> Unit = {
        Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(name, style = MaterialTheme.typography.titleMedium, color = scheme.primary)
            Text(desc, style = MaterialTheme.typography.bodySmall)
            if (target != null) {
                Text(stringResource(R.string.profile_target, target), style = MaterialTheme.typography.labelLarge)
            }
        }
    }
    val colors = CardDefaults.cardColors(containerColor = scheme.surfaceContainerHigh)
    val shape = RoundedCornerShape(14.dp)
    val border = BorderStroke(1.dp, scheme.outlineVariant)
    if (onClick != null) {
        Card(onClick = onClick, modifier = modifier, shape = shape, colors = colors, border = border) { content() }
    } else {
        Card(modifier = modifier, shape = shape, colors = colors, border = border) { content() }
    }
}
