package com.dfdx047.swordman.data

import java.io.File

/**
 * Opcoes do jogo, guardadas no mgr.toml que o runtime le no arranque (rex::cvar::LoadConfig).
 * O launcher escreve-o antes de abrir o jogo; o menu do jogo muda as mesmas cvars ao vivo e
 * grava no mesmo ficheiro (game_state_jni.cpp), e o launcher le-o ao voltar.
 *
 * O SDK e o do Dragon Burst: as otimizacoes da GPU (dbz3_*) sao as dele. Os passos do render
 * nativo do Burst Limit (dbz3_bl_native_*) reconhecem os shaders desse jogo e ficam desligados
 * no MGR.
 */
data class GameConfig(
    // Desempenho / imagem
    val resolution: InternalResolution = InternalResolution.P720,  // draw_resolution_scale_x/y + draw_resolution_percent
    val presentEffect: String = "bilinear",   // present_effect bilinear|cas|fsr
    val fsrQuality: String = "auto",          // present_fsr_quality_mode
    val fsrSharpness: Double = 0.2,           // present_fsr_sharpness_reduction 0-2 (0 = mais nitido)
    val casSharpness: Double = 0.0,           // present_cas_additional_sharpness 0-1
    val fxaa: String = "none",                // swap_post_effect none|fxaa|fxaa_extreme
    val anisotropic: Int = -1,                // anisotropic_override -1 (jogo) ou 1-5 (1x-16x)
    // FPS: o MGR corre a 60. frameRate 30 = jogo a 60 a desenhar 1 frame em 2 (dbz3_frameskip,
    // a logica e a velocidade ficam as de 60). 120 interpolado = jogo a 60 + 1 frame gerado entre
    // cada dois (patch 0092). Ver FrameMode.
    val frameRate: Int = 60,
    val frameInterpolation: Boolean = false,
    // Desfoque do brilho do MGR (cadeia de 320x176, PS C4558052 + 4086D1DF): no RG Cube custa
    // ~2,6 ms e desliga-lo nao mudou a imagem em duas cenas (autoteste 20/10 23:03 e 21/10 06:40).
    val glowBlur: Boolean = false,  // desfoque do brilho: ~2 ms no Mali-G57, cortado por padrao (21/10)
    // Sombras projetadas no ecra (passe de tela cheia, PS BE9C3F69): ~3 ms no RP5. Desligadas a
    // imagem fica normal, so sem essas sombras (teste manual 23/10 05:16).
    val shadows: Boolean = true,
    // Iluminacao "Baixa" (23/10, experimental): a cadeia do brilho inteira saltada.
    val glowOff: Boolean = false,
    // Efeitos "Baixos" (23/10, experimental): os 7 shaders mais pesados do passe dos efeitos
    // transparentes (faiscas, rastros) saltados. So aparecem nesse passe (captura de 22/10).
    val fxLow: Boolean = false,
    // 0114 (experimental): aritmetica dos pixel shaders em fp16 (RelaxedPrecision).
    val psFp16: Boolean = false,
    val showFps: Boolean = false,             // debug_overlay
    val letterbox: Boolean = true,            // present_letterbox (16:9 com barras / esticar)
    val volume: Int = 100,                    // volume geral 0-100 (app, rex::audio::SetOutputGain)
    val vibration: Boolean = true,            // vibration
    val deadzone: Double = 0.0,               // left/right_stick_deadzone_percentage 0-0.5
    // Depuracao / otimizacoes da GPU (patches do Dragon Fury e do Dragon Burst)
    val verboseLog: Boolean = false,          // log_level = "debug" (senao "info")
    val perfLogging: Boolean = true,          // dbz3_perf_logging + logs/gpu_stats.log (ligado na fase de port)
    val directResolve: Int = 1,               // dbz3_direct_resolve_texture 0/1
    val directResolveFast: Boolean = true,    // dbz3_direct_resolve_skip_memory
    val rtDirect: Boolean = true,             // dbz3_rt_direct (render targets sem transferencias)
    val specShaders: Boolean = true,          // dbz3_spec_texture_signs + dbz3_tex_implicit_lod
    val clearTo1x: Boolean = true,            // dbz3_clear_to_1x (Clear() do XDK direto em 1x)
    val intoTexture: Boolean = true,          // dbz3_render_into_texture (passes direto na textura)
    val postLite: Boolean = true,             // dbz3_resolve_dedupe + dbz3_skip_null_draws + dbz3_discard_full_overwrite
    /** Todas as otimizacoes da GPU desligadas (caminho emulado "puro", para comparar a imagem). */
    val safeGpu: Boolean = false,
) {
    /** Escala inteira (1 abaixo de 720p e em 720p, 2 em 1440p, 3 em 2160p). */
    val resolutionScale: Int get() = resolution.scale

    fun toToml(): Map<String, Any> {
        val opt = !safeGpu
        val adreno = com.dfdx047.swordman.drivers.DriverRepository.isAdreno()
        return linkedMapOf(
            "gpu_plugin" to "xenos",
            "game_data_root" to AppPaths.gameDir.absolutePath,
            "gpu_allow_invalid_fetch_constants" to true,
            "log_file" to AppPaths.lastRunLog.absolutePath,
            "draw_resolution_scale_x" to resolution.scale.coerceIn(1, MAX_RESOLUTION_SCALE),
            "draw_resolution_scale_y" to resolution.scale.coerceIn(1, MAX_RESOLUTION_SCALE),
            // Abaixo de 720p (patch 0089, Vulkan): 50 = 640x360, 75 = 960x540. So vale com a escala 1.
            "draw_resolution_percent" to resolution.percent,
            "present_effect" to presentEffect,
            "present_fsr_quality_mode" to fsrQuality,
            "present_fsr_sharpness_reduction" to fsrSharpness.coerceIn(0.0, 2.0),
            "present_cas_additional_sharpness" to casSharpness.coerceIn(0.0, 1.0),
            "swap_post_effect" to fxaa,
            "anisotropic_override" to anisotropic.coerceIn(-1, 5),
            "frame_rate" to "60",
            "dbz3_frame_interpolation" to (frameMode == FrameMode.FPS120_INTERP),
            "dbz3_frame_interpolation_quality" to interpolationQuality,
            // 30 FPS: o jogo continua a 60 (mesma velocidade) e so metade dos frames e desenhada.
            "dbz3_frameskip" to if (frameMode == FrameMode.FPS30) 1 else 0,
            "debug_overlay" to showFps,
            "present_letterbox" to letterbox,
            "vibration" to vibration,
            "left_stick_deadzone_percentage" to deadzone.coerceIn(0.0, 1.0),
            "right_stick_deadzone_percentage" to deadzone.coerceIn(0.0, 1.0),
            "log_level" to if (verboseLog) "debug" else "info",
            "dbz3_perf_logging" to perfLogging,
            "dbz3_direct_resolve_texture" to if (opt) directResolve.coerceIn(0, 1) else 0,
            "dbz3_direct_resolve_skip_memory" to (opt && directResolveFast),
            // Turnip (21/10 noite): sem gravar a memoria o resolve do RP5 cai de 13-14 para 6-7 ms
            // por frame (60 fps fora das lutas). Imagem ainda com defeitos (Raiden branco no menu):
            // aceite por agora para otimizar; o modo seguro desliga tudo.
            "swordman_skip_memory_turnip" to (opt && directResolveFast),
            "dbz3_rt_direct" to (opt && rtDirect),
            "dbz3_rt_elide_restores" to (opt && adreno),
            "dbz3_skip_depth_restore" to opt,
            "dbz3_hoist_uploads" to opt,
            "dbz3_resolve_early_transition" to opt,
            // Copia por desenho: tambem no Adreno (Qualcomm 21/10: 9.15 -> 7.14 ms, imagem igual).
            "dbz3_direct_resolve_draw" to opt,
            "dbz3_rit_serve_resolves" to (opt && !adreno),
            "dbz3_resolve_dedupe_unread_neutral" to (opt && !adreno),
            "dbz3_spec_texture_signs" to (opt && specShaders),
            "dbz3_tex_implicit_lod" to (opt && specShaders),
            "dbz3_rt_keep_dc" to true,
            "dbz3_clear_to_1x" to (opt && clearTo1x),
            "dbz3_render_into_texture" to (opt && intoTexture),
            // 0169: quads de ecra inteiro do MGR desenhados como strips (brilho, imagem final).
            "swordman_strip_quad_ps" to (if (opt && intoTexture) MGR_STRIP_QUAD_PS else ""),
            // 0170: cada passo da cadeia do brilho com a sua assinatura (as texturas que le).
            "swordman_rit_sig_sampled" to (opt && intoTexture),
            // 0171 (23/10): no Turnip sem lista de formatos as cores ficaram certas (teste manual
            // no RP5): o passo 4 passa a valer tambem la.
            "dbz3_rit_textures_turnip" to (opt && intoTexture),
            "dbz3_resolve_dedupe" to (opt && postLite),
            "dbz3_skip_null_draws" to (opt && postLite),
            "dbz3_discard_full_overwrite" to (opt && postLite),
            // Passos do render nativo do Burst Limit: reconhecem os shaders desse jogo.
            "dbz3_bl_native_present" to false,
            "dbz3_bl_native_linear_depth" to false,
            "dbz3_bl_native_post_lite" to false,
            "dbz3_bl_native_contour" to false,
            "dbz3_bl_native_downsample" to false,
            "dbz3_bl_native_blur" to false,
            "dbz3_bl_native_glow_composite" to false,
            // Efeitos do Budokai 3 (hashes de shaders desse jogo) e restauros pelo hash do DBZ3.
            "dbz3_fx_bloom" to true,
            "dbz3_fx_dof" to true,
            "dbz3_fx_fxaa" to true,
            "dbz3_rt_restore_shaders" to "",
            // Occlusion queries do jogo (o MGR usa-as cerca de 45 s depois de abrir): o driver da
            // Mali cai em vkCmdEndQuery (RG Cube, 20/10). Fora do Adreno ficam desligadas e o
            // jogo recebe o numero fixo de amostras (tudo "visivel").
            "occlusion_query_enable" to adreno,
            // So ao vivo (autoteste / imagem congelada): cada arranque comeca desligado, mesmo
            // que um build antigo as tenha gravado no mgr.toml.
            "swordman_freeze_replay" to false,
            "swordman_freeze_record" to false,
            "swordman_ps_census" to false,
            "swordman_skip_ps" to listOfNotNull(
                if (glowBlur && !glowOff) null else MGR_GLOW_BLUR_PS,
                if (glowOff) MGR_GLOW_CHAIN_PS else null,
                if (shadows) null else MGR_SCREEN_SHADOW_PS,
                if (fxLow) MGR_HEAVY_FX_PS else null,
            ).joinToString(","),
            // Escolha explicita do brilho (22/10): as configs antigas gravaram "" (brilho ligado)
            // como padrao; so esta chave conta para manter o brilho.
            "swordman_glow_blur" to glowBlur,
            "swordman_shadows" to shadows,
            "swordman_glow_off" to glowOff,
            "swordman_fx_low" to fxLow,
            "swordman_ps_fp16" to psFp16,
            // Os contornos 0102/0103 eram para o crash acima (diagnostico errado): desligados.
            "dbz3_empty_fs_for_depth_only" to 0,
            "dbz3_skip_vertex_shaders" to "",
            // Memoria do jogo num buffer normal (sem sparse binding): menos um caminho com bugs.
            "vulkan_sparse_shared_memory" to false,
            "dbz3_ignore_fetch_lod_bias" to false,
        )
    }

    companion object {
        val EFFECTS = listOf("bilinear", "cas", "fsr")
        val FSR_QUALITY = listOf("auto", "quality", "balanced", "performance", "ultra_performance")
        val FXAA = listOf("none", "fxaa", "fxaa_extreme")
        /** Pixel shaders do desfoque do brilho do MGR (swordman_skip_ps). */
        const val MGR_GLOW_BLUR_PS = "C45580526034B1F2,4086D1DF9FA6220E"
        /**
         * Quads de ecra inteiro em triangle strip (captura de 22/10, um draw a cobrir o scissor no
         * inicio do passe): profundidade linear 7BE6, cadeia do brilho A755/C455/4086/38C3,
         * imagem final F854. Com eles o passo 4 desenha direto na textura do resolve (patch 0169).
         */
        const val MGR_STRIP_QUAD_PS =
            "7BE61437FF049E59,A755AD0AC2470588,C45580526034B1F2,4086D1DF9FA6220E,38C32FC0BC7F322D,F854BF3B7BF7AA9D"
        /** O resto da cadeia do brilho (reducao e soma), para a iluminacao "Baixa". */
        const val MGR_GLOW_CHAIN_PS = "A755AD0AC2470588,38C32FC0BC7F322D"
        /** Os shaders mais pesados dos efeitos transparentes (so no passe P18 da captura de 22/10). */
        const val MGR_HEAVY_FX_PS =
            "E664840CC5314C1D,D47E656630A5D1FD,98F7AAEB1C255FEE,CFD7160391146423,C9769A4005A60F77,C1D2AF15BA1F1EAC,2CE21E05F9680D34"
        /** Pixel shader das sombras projetadas no ecra (passe de tela cheia). */
        const val MGR_SCREEN_SHADOW_PS = "BE9C3F694476A5B5"
        const val MAX_RESOLUTION_SCALE = 3

        /** dbz3_frame_interpolation_quality para este aparelho (2 nas GPUs de Alto/Ultra). */
        val interpolationQuality: Int by lazy {
            try {
                if (recommendedPreset() >= GraphicsPreset.HIGH) 2 else 1
            } catch (e: Throwable) {
                1
            }
        }

        fun load(file: File = AppPaths.userToml): GameConfig {
            val m = try {
                TomlLite.read(file)
            } catch (e: TomlLite.ParseError) {
                AppLog.w("config", "mgr.toml invalido, uso as opcoes por omissao: ${e.message}")
                emptyMap()
            }
            val d = GameConfig()
            fun str(k: String, def: String) = (m[k] as? TomlLite.Value.Str)?.v ?: def
            fun bool(k: String, def: Boolean) = (m[k] as? TomlLite.Value.Bool)?.v ?: def
            fun int(k: String, def: Int) = when (val v = m[k]) {
                is TomlLite.Value.Num -> v.v.toInt()
                is TomlLite.Value.Dbl -> v.v.toInt()
                is TomlLite.Value.Str -> v.v.toIntOrNull() ?: def
                else -> def
            }
            fun dbl(k: String, def: Double) = when (val v = m[k]) {
                is TomlLite.Value.Num -> v.v.toDouble()
                is TomlLite.Value.Dbl -> v.v
                else -> def
            }
            val interp = bool("dbz3_frame_interpolation", false)
            val skip = int("dbz3_frameskip", 0) > 0
            return GameConfig(
                resolution = InternalResolution.from(
                    int("draw_resolution_scale_x", 1),
                    int("draw_resolution_percent", 100),
                ),
                presentEffect = str("present_effect", d.presentEffect).let { if (it in EFFECTS) it else "fsr" },
                fsrQuality = str("present_fsr_quality_mode", d.fsrQuality).let { if (it in FSR_QUALITY) it else "auto" },
                fsrSharpness = dbl("present_fsr_sharpness_reduction", d.fsrSharpness),
                casSharpness = dbl("present_cas_additional_sharpness", d.casSharpness),
                fxaa = str("swap_post_effect", d.fxaa).let { if (it in FXAA) it else "none" },
                anisotropic = int("anisotropic_override", d.anisotropic),
                frameRate = when {
                    skip -> 30
                    interp -> 120
                    else -> 60
                },
                frameInterpolation = interp,
                // 22/10: autoteste ao vivo no RP5 numa luta: -3.5 ms sem o desfoque do brilho (Cube:
                // -2.1 ms). Cortado por padrao tambem nas configs antigas; volta com o interruptor.
                glowBlur = bool("swordman_glow_blur", false),
                shadows = bool("swordman_shadows", true),
                // 23/10 07:37: os dois modos quebraram a imagem no RP5; desligados ate refazer.
                glowOff = false,
                fxLow = false,
                // 23/10 07:47: fp16 nos pixel shaders fazia os retangulos pretos a piscar (teste manual
                // no RP5); a opcao saiu do launcher e fica sempre desligada.
                psFp16 = false,
                showFps = bool("debug_overlay", d.showFps),
                letterbox = bool("present_letterbox", d.letterbox),
                volume = int("swordman_volume", d.volume).coerceIn(0, 100),
                vibration = bool("vibration", d.vibration),
                deadzone = dbl("left_stick_deadzone_percentage", d.deadzone).coerceIn(0.0, 0.5),
                verboseLog = str("log_level", "info").let { it == "debug" || it == "trace" },
                perfLogging = bool("dbz3_perf_logging", d.perfLogging),
                directResolve = if (int("dbz3_direct_resolve_texture", d.directResolve) == 0) 0 else 1,
                directResolveFast = bool("dbz3_direct_resolve_skip_memory", d.directResolveFast),
                rtDirect = bool("dbz3_rt_direct", d.rtDirect),
                specShaders = bool("dbz3_spec_texture_signs", d.specShaders),
                clearTo1x = bool("dbz3_clear_to_1x", d.clearTo1x),
                intoTexture = bool("dbz3_render_into_texture", d.intoTexture),
                postLite = bool("dbz3_resolve_dedupe", d.postLite),
                safeGpu = bool("swordman_safe_gpu", d.safeGpu),
            )
        }

        /**
         * Cheat "pular logos" (CheatsScreen), gravado no mgr.toml antes de cada arranque. As logos
         * do MGR sao imagens desenhadas pelo jogo (~26 s), nao videos (log de 23/10): o runtime
         * (patch 0168) corre o jogo BOOT_FAST_FORWARD vezes mais depressa ate ele abrir o video do
         * titulo (ou 20 s no maximo).
         */
        const val BOOT_FAST_FORWARD = 8.0

        fun saveSkipLogos(on: Boolean, file: File = AppPaths.userToml) {
            TomlLite.merge(
                file,
                mapOf(
                    "swordman_boot_fast_forward" to (if (on) BOOT_FAST_FORWARD else 1.0),
                    "swordman_skip_movies" to "",
                ),
            )
        }

        fun save(config: GameConfig, file: File = AppPaths.userToml) {
            // As opcoes so do app (volume, modo seguro) ficam no mesmo ficheiro com prefixo
            // swordman_ (o runtime ignora cvars que nao conhece).
            TomlLite.merge(
                file,
                config.toToml() + mapOf(
                    "swordman_volume" to config.volume.coerceIn(0, 100),
                    "swordman_safe_gpu" to config.safeGpu,
                ),
            )
        }
    }
}

/**
 * Taxa de quadros (launcher e menu do jogo). O MGR corre a 60.
 *  - 30: o jogo a 60 (mesma velocidade) a desenhar so 1 frame em cada 2 (dbz3_frameskip): para
 *    GPUs fracas, a meta do RG Cube.
 *  - 60: o original.
 *  - 120 interpolado: o jogo a 60 e um quadro gerado entre cada dois (patch 0092), so com tela de
 *    120 Hz; +1 quadro de latencia (~8 ms).
 */
enum class FrameMode(val fps: Int, val outputHz: Int) {
    FPS30(30, 60),
    FPS60(60, 60),
    FPS120_INTERP(60, 120),
}

val GameConfig.frameMode: FrameMode
    get() = when {
        frameRate == 30 -> FrameMode.FPS30
        frameRate >= 100 -> FrameMode.FPS120_INTERP
        else -> FrameMode.FPS60
    }

fun GameConfig.withFrameMode(m: FrameMode): GameConfig = copy(
    frameRate = when (m) {
        FrameMode.FPS30 -> 30
        FrameMode.FPS60 -> 60
        FrameMode.FPS120_INTERP -> 120
    },
    frameInterpolation = m == FrameMode.FPS120_INTERP,
)

/** Cvars ao vivo do menu do jogo para um modo. */
fun FrameMode.cvars(): Map<String, String> = mapOf(
    "frame_rate" to "60",
    "dbz3_frame_interpolation" to (this == FrameMode.FPS120_INTERP).toString(),
    "dbz3_frameskip" to (if (this == FrameMode.FPS30) "1" else "0"),
)

/**
 * Resolucao interna. O renderizador do Xenia so multiplica 720p por inteiros (2x = 1440p,
 * 3x = 2160p); abaixo de 720p o jogo e desenhado numa percentagem de 1280x720 (patch 0089,
 * draw_resolution_percent). 1080p seria 1,5x, que o renderizador nao faz: para uma tela de
 * 1080p o caminho e 720p com o FSR 1 a ampliar na apresentacao.
 */
enum class InternalResolution(val scale: Int, val percent: Int, val label: String) {
    P360(1, 50, "360p"),
    P540(1, 75, "540p"),
    P720(1, 100, "720p"),
    P1440(2, 100, "2K"),
    P2160(3, 100, "4K");

    /** Pixeis em relacao a 720p (0,25 em 360p, 4 em 1440p). */
    val pixelFactor: Double get() = scale * scale * (percent / 100.0) * (percent / 100.0)

    companion object {
        fun from(scale: Int, percent: Int): InternalResolution = when {
            scale >= 3 -> P2160
            scale == 2 -> P1440
            percent <= 60 -> P360
            percent < 90 -> P540
            else -> P720
        }
    }
}

/**
 * Predefinicoes graficas pelo peso na GPU. Mudam a resolucao, os FPS, o filtro de apresentacao e
 * o anisotropico. Aparelhos de referencia do MGR: Retroid Pocket 5 (SD865, Adreno 650) e RG Cube
 * (Unisoc T770/T820, Mali-G57): meta de 30 FPS no Cube.
 */
enum class GraphicsPreset { MINIMUM, LIGHT, BALANCED, HIGH, ULTRA }

private data class PresetSpec(
    val resolution: InternalResolution,
    val fps30: Boolean,
    val presentEffect: String,
    val anisotropic: Int,
    val shadows: Boolean,
    val glowBlur: Boolean,
)

// 23/10: perfis estilo PC (Minimo/Baixo/Medio/Alto/Ultra). As sombras projetadas no ecra (~3 ms
// no RP5, imagem normal sem elas) e o desfoque do brilho (~2,6 ms no Cube, ~3,7 ms no RP5) so
// nos perfis altos. Medio e o do RP5: 540p + FSR a 60 FPS.
private fun GraphicsPreset.spec(): PresetSpec = when (this) {
    GraphicsPreset.MINIMUM -> PresetSpec(InternalResolution.P360, true, "fsr", -1, shadows = false, glowBlur = false)
    GraphicsPreset.LIGHT -> PresetSpec(InternalResolution.P540, true, "fsr", -1, shadows = false, glowBlur = false)
    GraphicsPreset.BALANCED -> PresetSpec(InternalResolution.P540, false, "fsr", -1, shadows = false, glowBlur = false)
    GraphicsPreset.HIGH -> PresetSpec(InternalResolution.P720, false, "fsr", 4, shadows = true, glowBlur = false)
    GraphicsPreset.ULTRA -> PresetSpec(InternalResolution.P1440, false, "bilinear", 5, shadows = true, glowBlur = true)
}

/** A resolucao de cada predefinicao (para os cartoes). */
val GraphicsPreset.resolution: InternalResolution get() = spec().resolution

fun GameConfig.withPreset(p: GraphicsPreset): GameConfig {
    val t = p.spec()
    val keep = if (frameRate == 30) 60 else frameRate
    return copy(
        resolution = t.resolution,
        frameRate = if (t.fps30) 30 else keep,
        frameInterpolation = !t.fps30 && frameInterpolation,
        presentEffect = t.presentEffect,
        fsrQuality = "auto",
        fxaa = "none",
        anisotropic = t.anisotropic,
        shadows = t.shadows,
        glowBlur = t.glowBlur,
        glowOff = false,
        fxLow = false,
    )
}

/** A predefinicao que corresponde as opcoes atuais, ou null (personalizado). */
fun GameConfig.matchingPreset(): GraphicsPreset? = GraphicsPreset.entries.firstOrNull { p ->
    val t = p.spec()
    t.resolution == resolution && fxaa == "none" && t.anisotropic == anisotropic &&
        t.presentEffect == presentEffect && (frameRate == 30) == t.fps30 &&
        t.shadows == shadows && t.glowBlur == glowBlur && !glowOff && !fxLow
}

/**
 * Predefinicao recomendada para o SoC deste aparelho. Snapdragon pela tabela SOC_MODEL -> Adreno
 * do TurnipSupport; MediaTek, Unisoc, Exynos e Tensor pelo SOC_MODEL / placa.
 */
fun recommendedPreset(): GraphicsPreset {
    val soc = (if (android.os.Build.VERSION.SDK_INT >= 31) android.os.Build.SOC_MODEL ?: "" else "") +
        " " + android.os.Build.HARDWARE + " " + android.os.Build.BOARD
    val s = soc.uppercase(java.util.Locale.ROOT)
    fun has(vararg keys: String) = keys.any { it in s }
    if (com.dfdx047.swordman.drivers.DriverRepository.isAdreno() || has("SM8", "SM7", "SM6", "SM4", "SDM")) {
        val adreno = com.dfdx047.swordman.drivers.TurnipSupport.detect().adreno
        return when {
            adreno == 810 -> GraphicsPreset.BALANCED
            adreno == 750 || adreno >= 825 -> GraphicsPreset.ULTRA
            adreno in 730..749 -> GraphicsPreset.HIGH
            adreno == 710 -> GraphicsPreset.LIGHT
            adreno in 640..729 -> GraphicsPreset.BALANCED
            adreno in 618..639 -> GraphicsPreset.LIGHT
            adreno in 1..617 -> GraphicsPreset.MINIMUM
            has("SM8750", "SM8650", "SM8735", "SM8845", "SM8850") -> GraphicsPreset.ULTRA
            has("SM8550", "SM8475", "SM8450", "SM8635", "SM7675") -> GraphicsPreset.HIGH
            has("SM8350", "SM8250", "SM8150", "KONA", "LAHAINA", "MSMNILE") -> GraphicsPreset.BALANCED
            else -> GraphicsPreset.LIGHT
        }
    }
    return when {
        has("MT6989", "MT6991", "MT6985", "MT6983", "MT6897", "MT6896", "MT6895") -> GraphicsPreset.HIGH
        has("MT6893", "MT6891", "MT6886") -> GraphicsPreset.BALANCED
        has("MT6877", "MT6875", "MT6873", "MT6855", "MT6853", "MT6833", "MT6879") -> GraphicsPreset.LIGHT
        has("MT6789", "MT6785", "MT6781", "MT6769", "MT6768", "MT6765", "MT6762", "MT8") -> GraphicsPreset.MINIMUM
        // Unisoc T770/T820 (Mali-G57 MC4, RG Cube): 540p a 30 FPS. T6xx (Mali-G52): 360p.
        has("T820", "T770", "T760", "T750", "UMS9620") -> GraphicsPreset.LIGHT
        has("T606", "T610", "T612", "T616", "T618", "UMS512", "UMS9230") -> GraphicsPreset.MINIMUM
        has("S5E9925", "S5E9945", "S5E9955") -> GraphicsPreset.HIGH
        has("GS101", "GS201", "ZUMA", "TENSOR") -> GraphicsPreset.BALANCED
        else -> GraphicsPreset.LIGHT
    }
}
