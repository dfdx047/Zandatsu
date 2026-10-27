package com.dfdx047.zandatsu.ui.components

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.dfdx047.zandatsu.R
import com.dfdx047.zandatsu.data.RenutSupport
import com.dfdx047.zandatsu.drivers.DriverRepository
import com.dfdx047.zandatsu.drivers.TurnipSupport
import com.dfdx047.zandatsu.ui.LauncherViewModel

/**
 * Aviso do driver (27/10): aparece so quando o driver ativo nao roda o Renut ([RenutSupport.blocker]: a verificacao
 * de funcoes que o jogo faz com esse driver, ou, antes dela, o que ja se sabe do driver do sistema), nao por ser
 * Adreno. Mostra o driver que roda o Renut nesta serie (TurnipSupport.recommendedForRenut) e, com [onOpenDrivers],
 * um botao para a aba Drivers. Nunca troca o driver.
 */
@Composable
fun TurnipWarning(vm: LauncherViewModel, onOpenDrivers: (() -> Unit)? = null) {
    val block = remember(vm.activeDriver) { if (DriverRepository.isAdreno()) RenutSupport.blocker() else null }
    if (block == null) return
    val gpu = vm.turnipGpu
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(14.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.errorContainer),
        border = BorderStroke(1.dp, Level.ERROR.color()),
    ) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(
                renutBlockedText(block),
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.onErrorContainer,
            )
            if (gpu.adreno != 0 && gpu.level != TurnipSupport.Level.UNSUPPORTED) {
                Text(
                    stringResource(R.string.turnip_recommended, "Adreno ${gpu.adreno} (${gpu.gen})", TurnipSupport.recommendedForRenut(gpu)),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onErrorContainer,
                )
            }
            if (onOpenDrivers != null) {
                Button(onClick = onOpenDrivers) { Text(stringResource(R.string.turnip_open_drivers)) }
            } else {
                Text(
                    stringResource(R.string.turnip_drivers_later),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onErrorContainer,
                )
            }
        }
    }
}

/** O aviso de que o driver ativo nao roda o Renut, com o motivo de [RenutSupport.blocker]. */
@Composable
fun renutBlockedText(block: String): String = stringResource(
    R.string.renut_blocked,
    if (block.contains(RenutSupport.CRASH_QUALCOMM_A7XX)) stringResource(R.string.renut_block_crash)
    else stringResource(R.string.renut_block_missing, block),
)
