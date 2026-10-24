package com.dfdx047.zandatsu.ui.screens

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
import com.dfdx047.zandatsu.R
import com.dfdx047.zandatsu.ui.LauncherViewModel
import com.dfdx047.zandatsu.ui.components.SectionCard
import com.dfdx047.zandatsu.ui.components.SettingSwitch

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
            ) { vm.changeSkipLogos(it) }
        }
    }
}
