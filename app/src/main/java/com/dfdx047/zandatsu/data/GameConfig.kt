package com.dfdx047.zandatsu.data

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
    // Renderizador (24/10): auto = o Renut (Direct3D nativo) nas Adreno onde ele corre (RenutSupport), senao o
    // Xenia emulado; renut / xenos = escolha do jogador (o Renut so vale onde nao falhou a verificacao; fora do
    // Adreno e experimental e so por escolha, 26/10).
    val renderer: String = "auto",
    // Opcoes do Renut (src/renut_effects.h): sombras do sol e brilho, ao vivo.
    val renutShadows: Boolean = true,
    val renutBloom: Boolean = true,
    // Perfil do Renut que o jogador escolheu (nome do RenutPreset; vazio = nenhum). Nada e pre-selecionado.
    val renutPreset: String = "",
    // Opcoes de 26/10, desligadas por padrao; os perfis ligam-nas onde o aparelho-alvo as validou (RenutPreset).
    val renutLazyMips: Boolean = false,     // swordman_renut_resolve_mips_lazy: mips dos resolves so quando lidos (>720p)
    val renutShadow1x: Boolean = false,     // renut_shadow_resolution "1x": sombras no tamanho do console (>720p)
    val audioLittleCores: Boolean = false,  // swordman_audio_little_cores: threads de audio nos nucleos pequenos
) {
    /**
     * O jogo corre no Renut com estas opcoes (escolha + aparelho). auto so escolhe o Renut numa GPU validada (Adreno):
     * nas outras fica o Xenia mesmo com renut_support.txt "ok", e o Renut so por escolha explicita.
     */
    val useRenut: Boolean get() = renderer != "xenos" && RenutSupport.supported() &&
        (renderer == "renut" || RenutSupport.validatedGpu())

    /**
     * sa2_render_height_handheld: as alturas que o Renut faz no Android (escalas de render_scale.h: 540p pelo patch
     * renut/0009, 4K pelo renut/0017). Os alvos de render escalam todos; a imagem final e reduzida para a tela.
     */
    val renutHeight: Int get() = when (resolution) {
        InternalResolution.P360 -> 360
        InternalResolution.P540 -> 540
        InternalResolution.P720 -> 720
        InternalResolution.P1080 -> 1080
        InternalResolution.P1440 -> 1440
        InternalResolution.P2160 -> 2160
    }

    /** Escala inteira do Xenia (1 abaixo de 720p e em 720p, 2 em 1080p/1440p, 3 em 2160p). */
    val resolutionScale: Int get() = resolution.scale

    fun toToml(): Map<String, Any> {
        val opt = !safeGpu
        val adreno = com.dfdx047.zandatsu.drivers.DriverRepository.isAdreno()
        val renut = useRenut
        return linkedMapOf(
            "gpu_plugin" to "xenos",
            // Renut (24/10): renderizador nativo. Os passos do Xenia abaixo ficam escritos (servem se o
            // jogo voltar ao Xenia por falta de funcoes Vulkan; o renut_support.txt diz ao launcher).
            "swordman_renderer" to if (renut) "renut" else "xenos",
            "swordman_renderer_choice" to renderer,
            "sa2_render_height_handheld" to renutHeight,
            // Principal no nucleo mais rapido, render nos outros grandes: +3 fps no RP5 (23/10).
            "swordman_renut_cores" to 2,
            // Opcao ainda nao validada no aparelho: fica escondida no menu e sempre ligada.
            "swordman_renut_shadows" to (renutShadows || !RenutValidated.SHADOWS),
            "swordman_renut_bloom" to (renutBloom || !RenutValidated.BLOOM),
            "swordman_renut_preset" to renutPreset,
            "swordman_renut_resolve_mips_lazy" to renutLazyMips,
            "renut_shadow_resolution" to if (renutShadow1x) "1x" else "Resolution",
            "swordman_audio_little_cores" to audioLittleCores,
            "renut_anisotropic_filtering" to when (anisotropic) {
                2 -> "2x"
                3 -> "4x"
                4 -> "8x"
                5 -> "16x"
                else -> "Game"
            },
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
            // 23/10 13:52: com o 0172 o passo 4 liga de verdade no Turnip; com o Turnip do ARMSX2 as
            // cores ficaram certas, com o Turnip v27.0.0-R1 sairam trocadas (R/B). So com o ARMSX2.
            "dbz3_rit_textures_turnip" to (opt && intoTexture &&
                com.dfdx047.zandatsu.drivers.DriverRepository.activeDir()?.contains("armsx2", true) == true),
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
        /**
         * Particulas dos efeitos (faiscas, poeira, rastros): quads de 4 vertices com mistura que leem
         * a profundidade linear (particulas suaves), so no passe dos efeitos (captura de 22/10).
         * 23/10: tirados da lista os 3 que sao malhas (D47E com rampa de cor 256x16 = o cabelo do
         * Raiden, que ficou preto; 98F7 e CFD7 leem a cena e podem ser cabelo/roupa).
         */
        const val MGR_HEAVY_FX_PS = "E664840CC5314C1D,C9769A4005A60F77,C1D2AF15BA1F1EAC,2CE21E05F9680D34"
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
                // 23/10 07:37: a iluminacao "Baixa" quebrou a imagem no RP5; desligada ate refazer.
                glowOff = false,
                fxLow = bool("swordman_fx_low", false),
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
                renderer = str("swordman_renderer_choice", d.renderer).let {
                    if (it in listOf("auto", "renut", "xenos")) it else "auto"
                },
                renutShadows = bool("swordman_renut_shadows", d.renutShadows),
                renutBloom = bool("swordman_renut_bloom", d.renutBloom),
                renutPreset = str("swordman_renut_preset", d.renutPreset),
                renutLazyMips = bool("swordman_renut_resolve_mips_lazy", d.renutLazyMips),
                renutShadow1x = str("renut_shadow_resolution", "Resolution") == "1x",
                audioLittleCores = bool("swordman_audio_little_cores", d.audioLittleCores),
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
    // So no Renut (sa2_render_height_handheld = 1080, escala 3/2); o Xenia so faz escalas inteiras (fica 2x).
    P1080(2, 75, "1080p"),
    P1440(2, 100, "1440p (2K)"),
    P2160(3, 100, "2160p (4K)");

    /** Pixeis em relacao a 720p (0,25 em 360p, 4 em 1440p). */
    val pixelFactor: Double get() = scale * scale * (percent / 100.0) * (percent / 100.0)

    companion object {
        fun from(scale: Int, percent: Int): InternalResolution = when {
            scale >= 3 -> P2160
            scale == 2 && percent < 90 -> P1080
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
    if (com.dfdx047.zandatsu.drivers.DriverRepository.isAdreno() || has("SM8", "SM7", "SM6", "SM4", "SDM")) {
        val adreno = com.dfdx047.zandatsu.drivers.TurnipSupport.detect().adreno
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

/**
 * O Renut corre neste aparelho com este driver? O jogo verifica as funcoes Vulkan de que ele precisa (texturas BC,
 * buffer device address, ...) ao arrancar no modo renut e escreve renut_support.txt ("ok", ou "no" e o que falta).
 * A verificacao vale para o driver com que o jogo arrancou (renut_support_driver.txt, escrito no JOGAR): trocar de
 * driver volta a testar. 26/10: o nativo ja nao pede texturas BC (descomprime-as) e basta-lhe 5 descriptor sets, por
 * isso fora do Adreno (Mali, Xclipse) o Renut tambem se oferece, mas so por escolha do jogador (experimental): o auto
 * so o escolhe numa GPU validada. Se faltar algo o jogo volta ao Xenia nessa sessao e escreve "no" e o motivo.
 */
object RenutSupport {
    private val dir get() = AppPaths.userToml.parentFile
    private fun driver() = com.dfdx047.zandatsu.drivers.DriverRepository.activeDir() ?: "system"

    /**
     * O Renut pode ser escolhido com este driver: "ok", ou ainda nao testado (no Adreno 6xx o driver do sistema nao
     * serve); "no" = falhou a verificacao.
     */
    fun supported(): Boolean = when (status()?.firstOrNull()) {
        "ok" -> true
        "no" -> false
        else -> !systemDriverOn6xx()
    }

    /**
     * GPU onde o Renut foi validado: Adreno e, desde 26/10, Mali Valhall (regressao completa na G57 do RG Cube, mais
     * rapido que o Xenia). So ai o "auto" o escolhe; nas outras e experimental.
     */
    fun validatedGpu(): Boolean = com.dfdx047.zandatsu.drivers.DriverRepository.let {
        it.isAdreno() || it.isMaliValhall(probe = false)
    }

    /** Linha para o AppLog: a decisao do Renut neste aparelho e porque. */
    fun describe(renderer: String): String {
        val dr = com.dfdx047.zandatsu.drivers.DriverRepository
        val gpuName = dr.gpuName()  // antes do validatedGpu (sem probe): a 1a vez guarda o nome da GPU
        val st = status()
        val raw = try {
            File(dir, "renut_support.txt").takeIf { it.isFile }?.readLines()?.joinToString(" | ") { it.trim() }
        } catch (e: Exception) {
            null
        }
        val tested = try {
            File(dir, "renut_support_driver.txt").takeIf { it.isFile }?.readText()?.trim()
        } catch (e: Exception) {
            null
        }
        val offered = supported()
        val use = renderer != "xenos" && offered && (renderer == "renut" || validatedGpu())
        val why = when {
            st?.firstOrNull() == "no" -> "falhou a verificacao: ${missing()}"
            st?.firstOrNull() == "ok" && !validatedGpu() -> "verificado ok, GPU nao validada (experimental)"
            st?.firstOrNull() == "ok" -> "verificado ok"
            systemDriverOn6xx() -> "Adreno 6xx com o driver do sistema"
            !validatedGpu() -> "ainda nao testado, GPU nao validada (experimental)"
            else -> "ainda nao testado"
        }
        val gpu = if (dr.isAdreno()) "Adreno" else if (dr.isMaliValhall(probe = false)) "Mali Valhall"
            else if (dr.isXclipse()) "Xclipse" else "outra"
        return "renut: ${if (offered) "oferecido" else "nao oferecido"} ($why); escolha $renderer -> " +
            (if (use) "renut" else "xenos") +
            (if (renderer == "auto" && offered && !validatedGpu()) " (auto fica no Xenia: GPU nao validada)" else "") +
            "; GPU $gpu '${gpuName.ifEmpty { "?" }}' SoC ${dr.socName()}; driver ${driver()}" +
            "; renut_support.txt=${raw ?: "-"} (testado com ${tested ?: "-"})"
    }

    /**
     * Adreno 6xx com o driver do sistema (Qualcomm): o Renut nao corre (RP5, 24/10). O launcher avisa nos drivers e
     * no Desempenho em vez de o jogo passar calado para o Xenia.
     */
    fun systemDriverOn6xx(): Boolean =
        com.dfdx047.zandatsu.drivers.DriverRepository.isAdreno() &&
            com.dfdx047.zandatsu.drivers.TurnipSupport.detect().gen == "a6xx" &&
            com.dfdx047.zandatsu.drivers.DriverRepository.activeDir() == null

    /** O que falta ao driver atual para o Renut (null = corre ou ainda nao foi testado). */
    fun missing(): String? {
        val s = status()?.takeIf { it.firstOrNull() == "no" } ?: return null
        return s.getOrNull(1)?.ifEmpty { null } ?: "?"
    }

    /** JOGAR: o driver com que o jogo vai arrancar (o resultado da verificacao fica ligado a ele). */
    fun noteLaunch() {
        try {
            File(dir, "renut_support_driver.txt").writeText(driver() + "\n")
        } catch (_: Exception) {
        }
    }

    private fun status(): List<String>? = try {
        val tested = File(dir, "renut_support_driver.txt").takeIf { it.isFile }?.readText()?.trim()
        // Sem o ficheiro do driver (versoes antigas): o resultado vale para o driver atual.
        if (tested != null && tested != driver()) {
            null
        } else {
            File(dir, "renut_support.txt").takeIf { it.isFile }?.readLines()?.map { it.trim() }
        }
    } catch (e: Exception) {
        null
    }
}

/** As resolucoes do Renut (menu Desempenho e menu do jogo). */
val RENUT_RESOLUTIONS = listOf(
    InternalResolution.P360, InternalResolution.P540, InternalResolution.P720, InternalResolution.P1080,
    InternalResolution.P1440, InternalResolution.P2160,
)

/**
 * Experimentais no Renut (24/10): no RP5 (Turnip) 1080p, 1440p e 4K cairam em 2 min de luta (GPU hang do kgsl,
 * "device lost"; memoria livre sempre acima de 3,5 GB). O menu marca-as com "*" e uma nota.
 */
val RENUT_EXPERIMENTAL = setOf(InternalResolution.P1080, InternalResolution.P1440, InternalResolution.P2160)

/** O nome no menu do Renut: as experimentais com "*". */
val InternalResolution.renutLabel: String
    get() = when (this) {
        InternalResolution.P1440 -> "2K"
        InternalResolution.P2160 -> "4K"
        else -> label
    } + if (this in RENUT_EXPERIMENTAL) "*" else ""

/** As do Xenia (sem 1080p: escala inteira). */
val XENOS_RESOLUTIONS = InternalResolution.entries.filter { it != InternalResolution.P1080 }

/**
 * Perfis do Renut (menu Desempenho e primeiro arranque), cada um com um aparelho-alvo (26/10). Uma opcao nova so
 * entra num perfil se no alvo nao derrubar o fps abaixo da meta nem estragar a imagem (regressao no aparelho):
 *  - Minimo: SD480+ (perfil sd480p do RP5: 2 A77 + 4 A55, GPU a 305 MHz), 30 fps estaveis. 540p FSR sem sombras nem
 *    brilho, audio nos nucleos pequenos: 40,4 fps (pior janela 38,3).
 *  - Baixo: RG Cube (Mali-G57), 30+. 540p FSR com sombras e brilho: 36,8 fps.
 *  - Medio: RP5 720p, 60. 720p com sombras e brilho (a 720p nao custam: GPU ~6 ms, o limite e a CPU) e audio nos
 *    nucleos pequenos: 59,9 fps (todas as janelas da luta entre 59,3 e 60,0; sem o audio nos pequenos, 58,2).
 *  - Alto: RP5 1080p / AYN Thor, o maximo estavel. 1080p com sombras e brilho, mips dos resolves so quando lidos,
 *    sombras no tamanho do console e audio nos nucleos pequenos: 55,6 fps no RP5 (pior janela 49,2).
 *  - Ultra: AYN Odin 3 (Adreno 830) 2K a 60 com FSR (medido pelo autor, 24/10), sombras, brilho, anisotropico 16x e
 *    os mips so quando lidos (validados no RP5 a 1080p; so tiram trabalho).
 */
enum class RenutPreset { MINIMUM, LOW, MEDIUM, HIGH, ULTRA }

/**
 * Opcoes do Renut ja validadas no RP5 (titulo, pausa, cutscene, luta, ripper mode, fotos antes/depois e fight-bench).
 * As que nao estao ficam escondidas nos menus e sempre ligadas; os perfis so aparecem com pelo menos uma validada.
 */
object RenutValidated {
    const val SHADOWS = true  // 24/10: titulo, pausa, luta; o prologo ate a 1.a luta e video
    const val BLOOM = true
    const val PRESETS = SHADOWS || BLOOM
}

private data class RenutPresetSpec(
    val resolution: InternalResolution,
    val shadows: Boolean,
    val bloom: Boolean,
    val anisotropic: Int,
    val effect: String = "bilinear",
    val lazyMips: Boolean = false,
    val shadow1x: Boolean = false,
    val audioLittle: Boolean = false,
)

private fun RenutPreset.spec(): RenutPresetSpec = when (this) {
    RenutPreset.MINIMUM -> RenutPresetSpec(
        InternalResolution.P540, shadows = false, bloom = false, anisotropic = -1, effect = "fsr", audioLittle = true,
    )
    RenutPreset.LOW -> RenutPresetSpec(InternalResolution.P540, shadows = true, bloom = true, anisotropic = -1, effect = "fsr")
    RenutPreset.MEDIUM -> RenutPresetSpec(InternalResolution.P720, shadows = true, bloom = true, anisotropic = -1, audioLittle = true)
    RenutPreset.HIGH -> RenutPresetSpec(
        InternalResolution.P1080, shadows = true, bloom = true, anisotropic = -1, lazyMips = true, shadow1x = true,
        audioLittle = true,
    )
    RenutPreset.ULTRA -> RenutPresetSpec(
        InternalResolution.P1440, shadows = true, bloom = true, anisotropic = 5, effect = "fsr", lazyMips = true,
    )
}

fun GameConfig.withRenutPreset(p: RenutPreset): GameConfig {
    val t = p.spec()
    return copy(
        resolution = t.resolution,
        renutShadows = t.shadows || !RenutValidated.SHADOWS,
        renutBloom = t.bloom || !RenutValidated.BLOOM,
        anisotropic = t.anisotropic,
        presentEffect = t.effect,
        renutLazyMips = t.lazyMips,
        renutShadow1x = t.shadow1x,
        audioLittleCores = t.audioLittle,
        renutPreset = p.name,
    )
}

/** O perfil que o jogador escolheu, se as opcoes ainda forem as dele (mudar uma opcao = personalizado). */
fun GameConfig.chosenRenutPreset(): RenutPreset? =
    matchingRenutPreset()?.takeIf { it.name == renutPreset }

fun GameConfig.matchingRenutPreset(): RenutPreset? = RenutPreset.entries.firstOrNull { p ->
    val t = p.spec()
    t.resolution == resolution && t.anisotropic == anisotropic && t.effect == presentEffect &&
        t.lazyMips == renutLazyMips && t.shadow1x == renutShadow1x && t.audioLittle == audioLittleCores &&
        (!RenutValidated.SHADOWS || t.shadows == renutShadows) && (!RenutValidated.BLOOM || t.bloom == renutBloom)
}

/**
 * Primeiro arranque (26/10): o perfil sugerido para a GPU deste aparelho (0 = Minimo .. 4 = Ultra). Primeiro pela
 * tabela dos aparelhos testados (Adreno 650 = Retroid Pocket 5, 740 = AYN Thor, 830 = AYN Odin 3, Mali-G57 = RG Cube;
 * as Adreno 610-619 sao a classe do SD480+, alvo do Minimo, simulada no RP5); senao pela familia: Adreno 6xx Medio,
 * 7xx Alto, 8xx Ultra; Mali do nivel da G57 (G5x/G6x) Baixo, mais nova Medio; Xclipse Medio. Nada e aplicado sem o
 * jogador aceitar.
 */
object GpuSuggestion {
    /** [known]: o aparelho testado com esta GPU (null = sugestao pela familia, em [family]). */
    data class Suggestion(val tier: Int, val known: String?, val family: String)

    fun detect(): Suggestion {
        val dr = com.dfdx047.zandatsu.drivers.DriverRepository
        val name = dr.gpuName()
        if (dr.isAdreno()) {
            val n = com.dfdx047.zandatsu.drivers.TurnipSupport.detect().adreno.takeIf { it != 0 }
                ?: Regex("Adreno[^0-9]*(\\d{3})", RegexOption.IGNORE_CASE).find(name)?.groupValues?.get(1)?.toIntOrNull()
                ?: 0
            return when {
                n == 650 -> Suggestion(2, "Retroid Pocket 5 (Adreno 650)", "Adreno 6xx")
                n == 740 -> Suggestion(3, "AYN Thor (Adreno 740)", "Adreno 7xx")
                n == 830 -> Suggestion(4, "AYN Odin 3 (Adreno 830)", "Adreno 8xx")
                n in 610..619 -> Suggestion(0, "SD480+ (Adreno 619)", "Adreno 6xx")
                n in 600..699 -> Suggestion(2, null, "Adreno 6xx")
                n in 700..799 -> Suggestion(3, null, "Adreno 7xx")
                n >= 800 -> Suggestion(4, null, "Adreno 8xx")
                else -> Suggestion(2, null, "Adreno")
            }
        }
        if (dr.isXclipse()) return Suggestion(2, null, "Xclipse")
        val mali = Regex("Mali-G(\\d+)", RegexOption.IGNORE_CASE).find(name)?.groupValues?.get(1)?.toIntOrNull()
        return when {
            mali == 57 -> Suggestion(1, "RG Cube (Mali-G57)", "Mali")
            mali != null && mali < 70 -> Suggestion(1, null, "Mali-G$mali")
            mali != null -> Suggestion(2, null, "Mali-G$mali")
            else -> Suggestion(1, null, name.ifEmpty { "?" })
        }
    }
}

/** O aparelho-alvo de cada perfil do Renut e o fps medido nele (luta do prologo; RenutPreset). */
val RENUT_PRESET_TARGETS = listOf(
    "Retroid Pocket 5 @ SD480+, 540p FSR: 40 fps",
    "RG Cube (Mali-G57), 540p FSR: 37 fps",
    "Retroid Pocket 5, 720p: 60 fps",
    "Retroid Pocket 5, 1080p: 56 fps · AYN Thor, 1080p FSR: 60 fps",
    "AYN Odin 3, 2K FSR: 60 fps",
)
