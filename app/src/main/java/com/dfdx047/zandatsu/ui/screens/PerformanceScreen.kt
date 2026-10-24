package com.dfdx047.zandatsu.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.BlurOn
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material.icons.filled.PhotoSizeSelectLarge
import androidx.compose.material.icons.filled.Speed
import androidx.compose.material.icons.filled.Wallpaper
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.dfdx047.zandatsu.R
import com.dfdx047.zandatsu.data.DisplayRates
import com.dfdx047.zandatsu.data.FrameMode
import com.dfdx047.zandatsu.data.GameConfig
import com.dfdx047.zandatsu.data.GraphicsPreset
import com.dfdx047.zandatsu.data.frameMode
import com.dfdx047.zandatsu.data.withFrameMode
import com.dfdx047.zandatsu.data.InternalResolution
import com.dfdx047.zandatsu.data.RENUT_EXPERIMENTAL
import com.dfdx047.zandatsu.data.RENUT_RESOLUTIONS
import com.dfdx047.zandatsu.data.renutLabel
import com.dfdx047.zandatsu.data.RenutPreset
import com.dfdx047.zandatsu.data.XENOS_RESOLUTIONS
import com.dfdx047.zandatsu.data.RenutValidated
import com.dfdx047.zandatsu.data.RenutSupport
import com.dfdx047.zandatsu.data.chosenRenutPreset
import com.dfdx047.zandatsu.data.withRenutPreset
import com.dfdx047.zandatsu.data.matchingPreset
import com.dfdx047.zandatsu.data.recommendedPreset
import com.dfdx047.zandatsu.data.withPreset
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material3.OutlinedButton
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.LocalContext
import com.dfdx047.zandatsu.ui.LauncherViewModel
import com.dfdx047.zandatsu.ui.components.Level
import com.dfdx047.zandatsu.ui.components.Pill
import com.dfdx047.zandatsu.ui.components.SectionCard
import com.dfdx047.zandatsu.ui.components.SettingDropdown
import com.dfdx047.zandatsu.ui.components.SettingSegmented
import com.dfdx047.zandatsu.ui.components.SettingSlider
import com.dfdx047.zandatsu.ui.components.SettingSwitch
import com.dfdx047.zandatsu.ui.components.StatusLine
import com.dfdx047.zandatsu.ui.components.color
import kotlin.math.roundToInt

/**
 * Aba Desempenho: tudo o que pesa na GPU (FPS e o hack de 30, resolucao interna de 360p a
 * 4K, filtros FSR/CAS/bilinear, efeitos do jogo, proporcao) e o volume. Tudo vai para o
 * mgr.toml (GameConfig), lido pelo jogo no arranque; o menu do jogo (Back) mexe nas mesmas
 * opcoes ao vivo.
 */
@Composable
fun PerformanceScreen(vm: LauncherViewModel) {
    val c = vm.config

    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        StatusLine(stringResource(R.string.settings_saved_note), Level.INFO)

        // ---------------------------------------------------------- renderizador
        // 24/10: o Renut (Direct3D nativo) onde o aparelho o corre; nas Mali (sem texturas BC) fica o
        // Xenia sozinho e esta escolha nem aparece.
        val renutOk = remember { RenutSupport.supported() }
        // Adreno sem Renut com este driver: dizer porque (em vez de so esconder a escolha).
        if (!renutOk && vm.isAdreno) {
            StatusLine(
                if (RenutSupport.systemDriverOn6xx()) stringResource(R.string.drivers_renut_system_6xx)
                else stringResource(R.string.drivers_renut_missing, RenutSupport.missing() ?: "?"),
                Level.ERROR,
            )
        }
        if (renutOk) {
            SectionCard(
                stringResource(R.string.renderer_title),
                Icons.Filled.Memory,
                subtitle = stringResource(R.string.renderer_sub),
            ) {
                SettingSegmented(
                    title = stringResource(R.string.renderer_title),
                    options = listOf(true to stringResource(R.string.renderer_renut), false to stringResource(R.string.renderer_xenos)),
                    selected = c.useRenut,
                    onSelect = { v -> vm.updateConfig { it.copy(renderer = if (v) "renut" else "xenos") } },
                )
            }
        }
        if (c.useRenut) {
            RenutOptions(vm)
            return@Column
        }

        // ------------------------------------------------------- predefinicoes
        val recommended = remember { recommendedPreset() }
        val current = c.matchingPreset()
        val presetNames = mapOf(
            GraphicsPreset.MINIMUM to stringResource(R.string.preset_minimum),
            GraphicsPreset.LIGHT to stringResource(R.string.preset_light),
            GraphicsPreset.BALANCED to stringResource(R.string.preset_balanced),
            GraphicsPreset.HIGH to stringResource(R.string.preset_high),
            GraphicsPreset.ULTRA to stringResource(R.string.preset_ultra),
        )
        val presetDescs = mapOf(
            GraphicsPreset.MINIMUM to stringResource(R.string.preset_minimum_desc),
            GraphicsPreset.LIGHT to stringResource(R.string.preset_light_desc),
            GraphicsPreset.BALANCED to stringResource(R.string.preset_balanced_desc),
            GraphicsPreset.HIGH to stringResource(R.string.preset_high_desc),
            GraphicsPreset.ULTRA to stringResource(R.string.preset_ultra_desc),
        )
        val presetSocs = mapOf(
            GraphicsPreset.MINIMUM to stringResource(R.string.preset_minimum_socs),
            GraphicsPreset.LIGHT to stringResource(R.string.preset_light_socs),
            GraphicsPreset.BALANCED to stringResource(R.string.preset_balanced_socs),
            GraphicsPreset.HIGH to stringResource(R.string.preset_high_socs),
            GraphicsPreset.ULTRA to stringResource(R.string.preset_ultra_socs),
        )
        SectionCard(
            stringResource(R.string.preset_title),
            Icons.Filled.Tune,
            subtitle = stringResource(R.string.preset_sub),
        ) {
            GraphicsPreset.entries.forEach { p ->
                PresetCard(
                    name = presetNames.getValue(p),
                    desc = presetDescs.getValue(p),
                    socs = stringResource(R.string.preset_for, presetSocs.getValue(p)),
                    selected = current == p,
                    recommended = recommended == p,
                ) { vm.updateConfig { it.withPreset(p) } }
            }
            StatusLine(
                if (current == null) stringResource(R.string.preset_custom, presetNames.getValue(recommended))
                else stringResource(R.string.preset_recommended, presetNames.getValue(recommended)),
                if (current == recommended) Level.OK else Level.INFO,
            )
            if (current != recommended) {
                OutlinedButton(onClick = { vm.updateConfig { it.withPreset(recommended) } }) {
                    Text(stringResource(R.string.preset_apply_recommended, presetNames.getValue(recommended)))
                }
            }
        }

        // ------------------------------------------------------------ desempenho
        SectionCard(stringResource(R.string.settings_performance), Icons.Filled.Speed) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Text(stringResource(R.string.settings_api), style = MaterialTheme.typography.bodyLarge, modifier = Modifier.weight(1f))
                Text("Vulkan", style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.primary)
            }
            Text(stringResource(R.string.settings_api_note), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            HorizontalDivider()
            // O MGR corre a 60. 30 FPS = hack (o jogo continua a 60, so metade dos frames e
            // desenhada: mesma velocidade). 120 interpolado (patch 0092) so com tela de 120 Hz.
            val context = LocalContext.current
            val maxHz = remember { DisplayRates.maxRefreshHz(context) }
            val has120 = maxHz >= DisplayRates.MIN_120_HZ
            val modes = buildList {
                add(FrameMode.FPS30 to stringResource(R.string.perf_fps_30_hack))
                add(FrameMode.FPS60 to "60 FPS")
                if (has120) add(FrameMode.FPS120_INTERP to stringResource(R.string.perf_fps_120_interp))
            }
            SettingSegmented(
                title = stringResource(R.string.perf_frame_rate),
                subtitle = stringResource(R.string.perf_frame_rate_sub),
                options = modes,
                selected = c.frameMode,
                onSelect = { v -> vm.updateConfig { it.withFrameMode(v) } },
            )
            when {
                c.frameMode == FrameMode.FPS30 ->
                    StatusLine(stringResource(R.string.perf_fps_30_note), Level.INFO)
                c.frameMode == FrameMode.FPS120_INTERP ->
                    StatusLine(stringResource(R.string.perf_fps_120_interp_note), Level.INFO)
            }
            if (!has120 && c.frameRate >= 100) {
                // Config de outro aparelho (ou de antes): nesta tela fica a 60 Hz.
                StatusLine(stringResource(R.string.perf_fps_120_fallback), Level.WARN)
            }
            if (!has120) StatusLine(stringResource(R.string.perf_fps_120_unavailable, maxHz.roundToInt()), Level.INFO)
            SettingSegmented(
                title = stringResource(R.string.settings_resolution),
                subtitle = stringResource(R.string.perf_resolution_sub),
                options = XENOS_RESOLUTIONS.map { r -> r to r.label },
                selected = c.resolution,
                onSelect = { v -> vm.updateConfig { it.copy(resolution = v) } },
            )
            Text(
                stringResource(R.string.perf_resolution_no_1080),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            when {
                c.resolution.percent < 100 -> StatusLine(
                    stringResource(R.string.perf_resolution_low, (c.resolution.pixelFactor * 100).roundToInt()),
                    Level.INFO,
                )
                c.resolution.scale >= 2 -> {
                    StatusLine(stringResource(R.string.perf_resolution_high, c.resolution.scale * c.resolution.scale), Level.WARN)
                    StatusLine(stringResource(R.string.perf_resolution_mem), Level.INFO)
                }
            }
            if (c.resolution.scale >= 3) StatusLine(stringResource(R.string.settings_resolution_warn), Level.WARN)
            SettingSwitch(stringResource(R.string.settings_show_fps), checked = c.showFps) { on -> vm.updateConfig { it.copy(showFps = on) } }
        }

        // ---------------------------------------------------- efeitos e som
        SectionCard(
            stringResource(R.string.perf_effects_title),
            Icons.Filled.BlurOn,
            subtitle = stringResource(R.string.perf_effects_sub),
        ) {
            // 23/10: qualidade manual por categoria, como nos jogos de PC (os perfis acima mudam
            // estas mesmas opcoes; mexer numa deixa o perfil "Personalizado").
            SettingSegmented(
                title = stringResource(R.string.q_shadows),
                subtitle = stringResource(R.string.q_shadows_sub),
                options = listOf(true to stringResource(R.string.q_high), false to stringResource(R.string.q_off)),
                selected = c.shadows,
                onSelect = { v -> vm.updateConfig { it.copy(shadows = v) } },
            )
            // 23/10 07:37 (RP5): Iluminacao "Baixa" deixou blocos verdes (a textura do brilho fica
            // sem ser escrita e a composicao le lixo) e Efeitos "Baixos" deixou o cabelo do Raiden
            // preto (o cabelo e um desses shaders). Ficam fora ate serem refeitos.
            SettingSegmented(
                title = stringResource(R.string.q_lighting),
                subtitle = stringResource(R.string.q_lighting_sub),
                options = listOf(true to stringResource(R.string.q_high), false to stringResource(R.string.q_medium)),
                selected = c.glowBlur,
                onSelect = { v -> vm.updateConfig { it.copy(glowBlur = v, glowOff = false) } },
            )
            SettingSegmented(
                title = stringResource(R.string.q_effects),
                subtitle = stringResource(R.string.q_effects_sub),
                options = listOf(false to stringResource(R.string.q_high), true to stringResource(R.string.q_low)),
                selected = c.fxLow,
                onSelect = { v -> vm.updateConfig { it.copy(fxLow = v) } },
            )
            HorizontalDivider()
            SettingSlider(
                title = stringResource(R.string.settings_volume),
                value = c.volume.toFloat(),
                range = 0f..100f,
                label = { v -> "${v.roundToInt()}%" },
            ) { v -> vm.updateConfig { it.copy(volume = v.roundToInt().coerceIn(0, 100)) } }
        }

        // ---------------------------------------------------------------- imagem
        SectionCard(stringResource(R.string.settings_image), Icons.Filled.Wallpaper) {
            FilterSettings(vm, c, quality = true)
            HorizontalDivider()
            SettingDropdown(
                title = stringResource(R.string.settings_aa),
                options = listOf(
                    "none" to stringResource(R.string.off),
                    "fxaa" to "FXAA",
                    "fxaa_extreme" to "FXAA Extreme",
                ),
                selected = c.fxaa,
                onSelect = { v -> vm.updateConfig { it.copy(fxaa = v) } },
            )
            SettingDropdown(
                title = stringResource(R.string.settings_aniso),
                options = listOf(-1 to stringResource(R.string.aniso_game), 1 to "1x", 2 to "2x", 3 to "4x", 4 to "8x", 5 to "16x"),
                selected = c.anisotropic,
                onSelect = { v -> vm.updateConfig { it.copy(anisotropic = v) } },
            )
        }

        // ------------------------------------------------------------- proporcao
        SectionCard(stringResource(R.string.settings_aspect), Icons.Filled.PhotoSizeSelectLarge) {
            SettingSegmented(
                title = stringResource(R.string.settings_aspect_mode),
                options = listOf(true to stringResource(R.string.aspect_original), false to stringResource(R.string.aspect_stretch)),
                selected = c.letterbox,
                onSelect = { v -> vm.updateConfig { it.copy(letterbox = v) } },
            )
        }

    }
}

/** Um cartao por predefinicao: nome, o que muda, os processadores para que e recomendada. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun PresetCard(
    name: String,
    desc: String,
    socs: String?,
    selected: Boolean,
    recommended: Boolean,
    onClick: () -> Unit,
) {
    val scheme = MaterialTheme.colorScheme
    Card(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(14.dp),
        colors = CardDefaults.cardColors(
            containerColor = if (selected) scheme.primaryContainer.copy(alpha = 0.45f) else scheme.surfaceContainerHigh,
        ),
        border = BorderStroke(if (selected) 2.dp else 1.dp, if (selected) scheme.primary else scheme.outlineVariant),
    ) {
        Column(Modifier.padding(horizontal = 14.dp, vertical = 10.dp), verticalArrangement = Arrangement.spacedBy(3.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(
                    name,
                    style = MaterialTheme.typography.titleSmall,
                    color = if (selected) scheme.primary else scheme.onSurface,
                    modifier = Modifier.weight(1f),
                )
                if (recommended) Pill(stringResource(R.string.preset_badge_recommended), Level.OK.color())
                if (selected) Pill(stringResource(R.string.preset_badge_active), scheme.primary)
            }
            Text(desc, style = MaterialTheme.typography.bodyMedium)
            if (socs != null) Text(socs, style = MaterialTheme.typography.bodySmall, color = scheme.onSurfaceVariant)
        }
    }
}

/**
 * As opcoes do Renut: perfis Baixo/Medio/Alto/Ultra, resolucao (540p/720p, as medidas no Android),
 * sombras, brilho e anisotropico. O FSR, o FXAA, os 30/120 FPS e a proporcao sao do Xenia.
 */
@Composable
private fun RenutOptions(vm: LauncherViewModel) {
    val c = vm.config
    // Sem recomendacao por aparelho: so fica marcado o perfil que o jogador escolheu.
    val current = c.chosenRenutPreset()
    val names = mapOf(
        RenutPreset.MINIMUM to stringResource(R.string.preset_minimum),
        RenutPreset.LOW to stringResource(R.string.renut_preset_low),
        RenutPreset.MEDIUM to stringResource(R.string.renut_preset_medium),
        RenutPreset.HIGH to stringResource(R.string.renut_preset_high),
        RenutPreset.ULTRA to stringResource(R.string.renut_preset_ultra),
    )
    val descs = mapOf(
        RenutPreset.MINIMUM to stringResource(R.string.renut_preset_minimum_desc),
        RenutPreset.LOW to stringResource(R.string.renut_preset_low_desc),
        RenutPreset.MEDIUM to stringResource(R.string.renut_preset_medium_desc),
        RenutPreset.HIGH to stringResource(R.string.renut_preset_high_desc),
        RenutPreset.ULTRA to stringResource(R.string.renut_preset_ultra_desc),
    )
    // Perfis e opcoes de efeitos: so as validadas no aparelho (RenutValidated).
    if (RenutValidated.PRESETS) SectionCard(
        stringResource(R.string.preset_title),
        Icons.Filled.Tune,
        subtitle = stringResource(R.string.preset_sub),
    ) {
        RenutPreset.entries.forEach { p ->
            PresetCard(
                name = names.getValue(p),
                desc = descs.getValue(p),
                socs = null,
                selected = current == p,
                recommended = false,
            ) { vm.updateConfig { it.withRenutPreset(p) } }
        }
        if (current == null) StatusLine(stringResource(R.string.renut_preset_none), Level.INFO)
    }

    SectionCard(stringResource(R.string.settings_performance), Icons.Filled.Speed) {
        StatusLine(stringResource(R.string.renut_fps_note), Level.INFO)
        SettingSegmented(
            title = stringResource(R.string.settings_resolution),
            subtitle = stringResource(R.string.renut_resolution_sub),
            options = RENUT_RESOLUTIONS.map { r -> r to r.renutLabel },
            selected = if (c.resolution in RENUT_RESOLUTIONS) c.resolution else InternalResolution.P720,
            onSelect = { v -> vm.updateConfig { it.copy(resolution = v) } },
        )
        StatusLine(
            stringResource(R.string.renut_res_experimental),
            if (c.resolution in RENUT_EXPERIMENTAL) Level.WARN else Level.INFO,
        )
        SettingSwitch(stringResource(R.string.settings_show_fps), checked = c.showFps) { on -> vm.updateConfig { it.copy(showFps = on) } }
    }

    SectionCard(stringResource(R.string.settings_image), Icons.Filled.Wallpaper) {
        FilterSettings(vm, c, quality = false)
    }

    SectionCard(
        stringResource(R.string.perf_effects_title),
        Icons.Filled.BlurOn,
        subtitle = stringResource(R.string.perf_effects_sub),
    ) {
        if (RenutValidated.SHADOWS) SettingSegmented(
            title = stringResource(R.string.q_shadows),
            subtitle = stringResource(R.string.renut_shadows_sub),
            options = listOf(true to stringResource(R.string.q_high), false to stringResource(R.string.q_off)),
            selected = c.renutShadows,
            onSelect = { v -> vm.updateConfig { it.copy(renutShadows = v) } },
        )
        if (RenutValidated.BLOOM) SettingSegmented(
            title = stringResource(R.string.renut_bloom),
            subtitle = stringResource(R.string.renut_bloom_sub),
            options = listOf(true to stringResource(R.string.on), false to stringResource(R.string.off)),
            selected = c.renutBloom,
            onSelect = { v -> vm.updateConfig { it.copy(renutBloom = v) } },
        )
        if (RenutValidated.PRESETS) HorizontalDivider()
        SettingDropdown(
            title = stringResource(R.string.settings_aniso),
            options = listOf(-1 to stringResource(R.string.aniso_game), 2 to "2x", 3 to "4x", 4 to "8x", 5 to "16x"),
            selected = if (c.anisotropic <= 1) -1 else c.anisotropic,
            onSelect = { v -> vm.updateConfig { it.copy(anisotropic = v) } },
        )
        HorizontalDivider()
        SettingSlider(
            title = stringResource(R.string.settings_volume),
            value = c.volume.toFloat(),
            range = 0f..100f,
            label = { v -> "${v.roundToInt()}%" },
        ) { v -> vm.updateConfig { it.copy(volume = v.roundToInt().coerceIn(0, 100)) } }
    }
}

/**
 * Filtro da imagem final (bilinear, AMD CAS, AMD FSR 1) e a nitidez: o mesmo no Xenia e no Renut (patch renut/0019,
 * passe final do Renut). O modo do FSR (resolucao interna) so existe no Xenia: no Renut e a resolucao.
 */
@Composable
private fun FilterSettings(vm: LauncherViewModel, c: GameConfig, quality: Boolean) {
    SettingSegmented(
        title = stringResource(R.string.settings_filter),
        subtitle = stringResource(R.string.settings_filter_sub),
        options = listOf("bilinear" to "Bilinear", "cas" to "AMD CAS", "fsr" to "AMD FSR 1"),
        selected = c.presentEffect,
        onSelect = { v -> vm.updateConfig { it.copy(presentEffect = v) } },
    )
    if (c.presentEffect == "fsr") {
        if (quality) SettingDropdown(
            title = stringResource(R.string.settings_fsr_mode),
            subtitle = stringResource(R.string.settings_fsr_mode_sub),
            options = listOf(
                "auto" to stringResource(R.string.fsr_native),
                "quality" to stringResource(R.string.fsr_quality),
                "balanced" to stringResource(R.string.fsr_balanced),
                "performance" to stringResource(R.string.fsr_performance),
                "ultra_performance" to stringResource(R.string.fsr_ultra),
            ),
            selected = c.fsrQuality,
            onSelect = { v -> vm.updateConfig { it.copy(fsrQuality = v) } },
        )
        if (quality && c.fsrQuality != "auto" && c.resolutionScale < 2) {
            StatusLine(stringResource(R.string.settings_fsr_mode_no_effect), Level.INFO)
        }
        SettingSlider(
            title = stringResource(R.string.settings_fsr_sharpness),
            value = (2.0 - c.fsrSharpness).toFloat(),
            range = 0f..2f,
            label = { v -> "${(v / 2f * 100).roundToInt()}%" },
        ) { v -> vm.updateConfig { it.copy(fsrSharpness = (2.0 - v).coerceIn(0.0, 2.0)) } }
    }
    if (c.presentEffect == "cas") {
        SettingSlider(
            title = stringResource(R.string.settings_cas_sharpness),
            value = c.casSharpness.toFloat(),
            range = 0f..1f,
            label = { v -> "${(v * 100).roundToInt()}%" },
        ) { v -> vm.updateConfig { it.copy(casSharpness = v.toDouble()) } }
    }
}
