package com.dfdx047.swordman.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.FastForward
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.dfdx047.swordman.R
import com.dfdx047.swordman.ui.LauncherViewModel
import com.dfdx047.swordman.ui.components.SectionCard
import com.dfdx047.swordman.ui.components.SettingSlider
import com.dfdx047.swordman.ui.components.SettingSwitch
import kotlin.math.roundToInt

/** Aba Cheats: atalhos que mudam o jogo (por agora: saltar as logos do arranque). */
@Composable
fun CheatsScreen(vm: LauncherViewModel) {
    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        SectionCard(stringResource(R.string.cheats_boot_title), Icons.Filled.FastForward) {
            SettingSwitch(
                title = stringResource(R.string.cheats_skip_logos),
                subtitle = stringResource(R.string.cheats_skip_logos_sub),
                checked = vm.skipLogos,
            ) { vm.setSkipLogos(it) }
            SettingSlider(
                title = stringResource(R.string.cheats_skip_logos_seconds),
                value = vm.skipLogosSeconds.toFloat(),
                range = 10f..60f,
                steps = 9,
                subtitle = stringResource(R.string.cheats_skip_logos_seconds_sub),
                label = { "${it.roundToInt()} s" },
            ) { vm.setSkipLogosSeconds(it.roundToInt()) }
        }
    }
}
