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
import com.dfdx047.zandatsu.drivers.DriverRepository
import com.dfdx047.zandatsu.drivers.TurnipSupport
import com.dfdx047.zandatsu.ui.LauncherViewModel

/**
 * Aviso do Turnip (26/10): numa Adreno com o driver do sistema o jogo corre no modo emulado (o Renut exige o
 * Turnip). Mostra o driver recomendado para a serie (TurnipSupport: 6xx
 * pelo teste de drivers no RP5, 7xx o do AYN Thor, 8xx o do AYN Odin 3) e, com [onOpenDrivers], um botao para a aba
 * Drivers. Nunca troca o driver.
 */
@Composable
fun TurnipWarning(vm: LauncherViewModel, onOpenDrivers: (() -> Unit)? = null) {
    val show = remember(vm.activeDriver) { DriverRepository.isAdreno() && DriverRepository.activeDir() == null }
    if (!show) return
    val gpu = vm.turnipGpu
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(14.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.errorContainer),
        border = BorderStroke(1.dp, Level.ERROR.color()),
    ) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(
                stringResource(R.string.turnip_required),
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.onErrorContainer,
            )
            if (gpu.adreno != 0 && gpu.level != TurnipSupport.Level.UNSUPPORTED) {
                Text(
                    stringResource(R.string.turnip_recommended, "Adreno ${gpu.adreno} (${gpu.gen})", gpu.builds),
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
