package com.dfdx047.swordman.drivers;

import android.os.Build;

import java.io.File;
import java.util.Locale;
import java.util.Map;

/**
 * Suporte ao Turnip (Mesa) por Snapdragon. Tabela tirada da pesquisa de 16/10 (OptXbox
 * docs/en/TURNIP.md): Build.SOC_MODEL -> Adreno -> geracao do Turnip -> estado e o que aplicar.
 *
 * O Vulkan diz a GPU exata (deviceName "Turnip Adreno (TM) 650") so depois de carregar o driver;
 * aqui, antes, so ha o SOC_MODEL (Android 12+) e o BOARD/HARDWARE para os mais antigos.
 */
public final class TurnipSupport {
    private TurnipSupport() {}

    /** Estado do Turnip neste GPU. */
    public enum Level {
        /** Turnip estavel (upstream, conformante): recomendado. */
        STABLE,
        /** So com builds com patches fora do Mesa (A710/720/722, A8xx): experimental. */
        EXPERIMENTAL,
        /** Sem suporte conhecido (A732, Mali, desconhecido): driver do sistema. */
        UNSUPPORTED,
    }

    public static final class Gpu {
        public final String soc;      // o SOC_MODEL como veio
        public final int adreno;      // 650, 740, 830... (0 = desconhecido)
        public final String gen;      // "a6xx", "a7xx", "a8xx" ou ""
        public final Level level;
        public final String builds;   // builds que a comunidade usa neste GPU
        public final String tuDebug;  // TU_DEBUG que se aplica sozinho ("" = nenhum)

        Gpu(String soc, int adreno, String gen, Level level, String builds, String tuDebug) {
            this.soc = soc;
            this.adreno = adreno;
            this.gen = gen;
            this.level = level;
            this.builds = builds;
            this.tuDebug = tuDebug;
        }

        @Override
        public String toString() {
            return "SoC " + (soc.isEmpty() ? "?" : soc) + " Adreno " + (adreno == 0 ? "?" : adreno)
                    + " (" + (gen.isEmpty() ? "?" : gen) + ") Turnip " + level
                    + (tuDebug.isEmpty() ? "" : " TU_DEBUG=" + tuDebug);
        }
    }

    private static final String MAINLINE = "Mesa (K11MCH1, StevenMXZ, MrPurple)";
    private static final String GEN8 = "Gen8 (StevenMXZ Gen8, whitebelyash)";
    private static final String A7_PATCH = "A710/A720 (K11MCH1 Gmem, Banners)";

    /** {prefixo do SOC_MODEL, adreno, geracao, nivel, builds, TU_DEBUG automatico}. */
    private static final Object[][] TABLE = {
            // a8xx (gen8: so com patches no kgsl do Android; o A830 tem o GMEM com falhas).
            {"SM8850", 840, "a8xx", Level.EXPERIMENTAL, GEN8, ""},
            {"SM8845", 829, "a8xx", Level.EXPERIMENTAL, GEN8, ""},
            {"SM8750", 830, "a8xx", Level.EXPERIMENTAL, GEN8, "sysmem"},
            {"SM8735", 825, "a8xx", Level.EXPERIMENTAL, GEN8, ""},
            {"SM7635", 810, "a8xx", Level.EXPERIMENTAL, GEN8, ""},
            {"SM6650", 810, "a8xx", Level.EXPERIMENTAL, GEN8, ""},
            // a7xx.
            {"SM8650", 750, "a7xx", Level.STABLE, MAINLINE, ""},
            {"SM8635", 735, "a7xx", Level.STABLE, MAINLINE, ""},
            {"SM8550", 740, "a7xx", Level.STABLE, MAINLINE, ""},
            {"SM8475", 730, "a7xx", Level.STABLE, MAINLINE, ""},
            {"SM8450", 730, "a7xx", Level.STABLE, MAINLINE, ""},
            {"SM7475", 725, "a7xx", Level.STABLE, MAINLINE, ""},
            {"SM7675", 732, "a7xx", Level.UNSUPPORTED, "-", ""},
            {"SM7750", 722, "a7xx", Level.EXPERIMENTAL, A7_PATCH, "sysmem"},
            {"SM7550", 720, "a7xx", Level.EXPERIMENTAL, A7_PATCH, "sysmem"},
            {"SM7435", 710, "a7xx", Level.EXPERIMENTAL, A7_PATCH, "sysmem"},
            {"SM6475", 710, "a7xx", Level.EXPERIMENTAL, A7_PATCH, "sysmem"},
            {"SM6450", 710, "a7xx", Level.EXPERIMENTAL, A7_PATCH, "sysmem"},
            // a6xx (Turnip conformante no Android).
            {"SM7450", 644, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SM8350", 660, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SM7325", 642, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SM7350", 642, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SM8250", 650, "a6xx", Level.STABLE, "MrPurple T19, K11MCH1 v24.3.0 R9", ""},
            {"SM8150", 640, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SM7250", 620, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SM7225", 619, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SM7150", 618, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SM7125", 618, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SM6375", 619, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SM6350", 619, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SM4450", 613, "a6xx", Level.EXPERIMENTAL, "whitebelyash v30+", ""},
            {"SM4375", 619, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SM4350", 619, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SM6150", 612, "a6xx", Level.EXPERIMENTAL, MAINLINE, ""},
            {"SM6225", 610, "a6xx", Level.STABLE, "MrPurple T19", ""},
            {"SM6125", 610, "a6xx", Level.STABLE, "MrPurple T19", ""},
            {"SM6115", 610, "a6xx", Level.STABLE, "MrPurple T19", ""},
            {"SM4250", 610, "a6xx", Level.STABLE, "MrPurple T19", ""},
            {"SDM845", 630, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SDM850", 630, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SDM710", 616, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SDM712", 616, "a6xx", Level.STABLE, MAINLINE, ""},
            {"SDM670", 615, "a6xx", Level.STABLE, MAINLINE, ""},
    };

    /** Nomes de placa dos aparelhos sem SOC_MODEL (Android < 12). */
    private static final String[][] BOARDS = {
            {"pineapple", "SM8650"}, {"kalama", "SM8550"}, {"taro", "SM8450"}, {"cape", "SM8475"},
            {"lahaina", "SM8350"}, {"kona", "SM8250"}, {"msmnile", "SM8150"}, {"sdm845", "SDM845"},
            {"sun", "SM8750"}, {"canoe", "SM8850"},
    };

    public static Gpu detect() {
        String soc = Build.VERSION.SDK_INT >= 31 && Build.SOC_MODEL != null ? Build.SOC_MODEL : "";
        String model = soc.toUpperCase(Locale.ROOT);
        if (model.isEmpty()) {
            String board = (Build.BOARD + " " + Build.HARDWARE).toLowerCase(Locale.ROOT);
            for (String[] b : BOARDS) {
                if (board.contains(b[0])) {
                    model = b[1];
                    break;
                }
            }
        }
        for (Object[] row : TABLE) {
            if (model.startsWith((String) row[0])) {
                return new Gpu(soc, (Integer) row[1], (String) row[2], (Level) row[3],
                        (String) row[4], (String) row[5]);
            }
        }
        boolean qualcomm = DriverRepository.INSTANCE.isAdreno();
        return new Gpu(soc, 0, "", qualcomm ? Level.EXPERIMENTAL : Level.UNSUPPORTED,
                qualcomm ? MAINLINE : "-", "");
    }

    /**
     * Variaveis do driver que se aplicam sozinhas com Turnip ativo (antes do vkCreateInstance;
     * o Turnip le o TU_DEBUG uma vez). Junta ao TU_DEBUG que o utilizador escolheu.
     */
    public static void applyAutoEnv(Gpu gpu, Map<String, String> env) {
        if (!gpu.tuDebug.isEmpty()) {
            String cur = env.get("TU_DEBUG");
            env.put("TU_DEBUG", cur == null || cur.isEmpty() ? gpu.tuDebug : cur + "," + gpu.tuDebug);
        }
        // Samsung (One UI) com Adreno 740: falhas de imagem sem isto (notas do K11MCH1).
        if (gpu.adreno == 740 && "samsung".equalsIgnoreCase(Build.MANUFACTURER)) {
            env.put("FD_DEV_FEATURES", "enable_tp_ubwc_flag_hint=1");
        }
    }

    /**
     * Bisect do Turnip: com "turnip_bisect" ligado, cada arranque do jogo com driver alternativo
     * aplica o passo seguinte desta lista (um so de cada vez) e escreve-o no log e em
     * logs/turnip_bisect.txt. Faz-se uma luta + autoteste em cada um; o passo em que a imagem
     * fica certa diz que parte do driver (ou do caminho do Xenia) falha.
     * {rotulo, variaveis de ambiente "NOME=valor;NOME=valor"}.
     */
    public static final String[][] BISECT = {
            {"0 base (nada mudado)", ""},
            {"1 sem dynamic rendering nem stencil export",
                    "REX_VULKAN_DYNAMIC_RENDERING=false;REX_DBZ3_VK_DISABLE_EXTENSIONS=VK_EXT_shader_stencil_export"},
            {"2 ir3 nopreamble", "IR3_SHADER_DEBUG=nopreamble"},
            {"3 ir3 nouboopt", "IR3_SHADER_DEBUG=nouboopt"},
            {"4 ir3 nofp16", "IR3_SHADER_DEBUG=nofp16"},
            {"5 sysmem (sem GMEM)", "TU_DEBUG=sysmem"},
            {"6 sem UBWC nem LRZ", "TU_DEBUG=noubwc,nolrz"},
            {"7 BC descomprimido", "REX_VULKAN_MASK_FEATURES=textureCompressionBC"},
            {"8 memoria num so buffer de 512 MB", "REX_DBZ3_VK_MAX_STORAGE_BUFFER_RANGE_MB=512"},
            {"9 sem robustez (robustBufferAccess, VK_EXT_robustness2)",
                    "REX_VULKAN_MASK_FEATURES=robustBufferAccess;REX_DBZ3_VK_DISABLE_EXTENSIONS=VK_EXT_robustness2"},
            {"10 sincronizacao total (flushall,syncdraw)", "TU_DEBUG=flushall,syncdraw"},
            {"11 depth float32", "REX_VULKAN_DISABLE_DEPTH_UNORM24=true"},
            // Do lado do jogo (as otimizacoes do Dragon Ball/MGR), para separar driver de port.
            {"12 sem passo 4 (render na textura)", "REX_DBZ3_RENDER_INTO_TEXTURE=false"},
            {"13 sem atalhos resolve->textura",
                    "REX_DBZ3_DIRECT_RESOLVE_TEXTURE=0;REX_DBZ3_DIRECT_RESOLVE_SKIP_MEMORY=false"},
            {"14 sem render targets diretos", "REX_DBZ3_RT_DIRECT=false"},
            {"15 Xenia puro (sem otimizacoes)",
                    "REX_DBZ3_RENDER_INTO_TEXTURE=false;REX_DBZ3_DIRECT_RESOLVE_TEXTURE=0;"
                            + "REX_DBZ3_DIRECT_RESOLVE_SKIP_MEMORY=false;REX_DBZ3_RT_DIRECT=false;"
                            + "REX_DBZ3_RESOLVE_DEDUPE=false;REX_DBZ3_SKIP_NULL_DRAWS=false;"
                            + "REX_DBZ3_DISCARD_FULL_OVERWRITE=false;REX_DBZ3_CLEAR_TO_1X=false;"
                            + "REX_DBZ3_HOIST_UPLOADS=false"},
            {"16 tudo do driver junto (sysmem, noubwc, nolrz, flushall, 512 MB)",
                    "TU_DEBUG=sysmem,noubwc,nolrz,flushall,syncdraw;REX_DBZ3_VK_MAX_STORAGE_BUFFER_RANGE_MB=512"},
    };

    /**
     * Segunda varredura (21/10): a primeira mostrou que no RP5 so "sem atalhos resolve->textura"
     * desenha certo. Cada passo desliga UMA parte desse atalho (o resto como na config).
     */
    public static final String[][] BISECT_RTT = {
            {"0 base (atalhos ligados)", ""},
            {"1 atalhos sem imageStore (buffer + copia)", "REX_SWORDMAN_RTT_STORAGE=false"},
            {"2 atalhos, mas a memoria tambem escrita", "REX_DBZ3_DIRECT_RESOLVE_SKIP_MEMORY=false"},
            {"3 sem o atalho float/32_FLOAT (0110)", "REX_SWORDMAN_RTT_FLOAT=false"},
            {"4 sem imageStore e memoria escrita",
                    "REX_SWORDMAN_RTT_STORAGE=false;REX_DBZ3_DIRECT_RESOLVE_SKIP_MEMORY=false"},
            {"5 sem imageStore, sem 0110",
                    "REX_SWORDMAN_RTT_STORAGE=false;REX_SWORDMAN_RTT_FLOAT=false"},
            {"6 sem passo 4 nem resolves servidos (so os atalhos)",
                    "REX_DBZ3_RENDER_INTO_TEXTURE=false;REX_DBZ3_RIT_SERVE_RESOLVES=false"},
            {"7 sem atalhos (referencia que desenha)", "REX_DBZ3_DIRECT_RESOLVE_TEXTURE=0"},
    };

    /** Lista de passos da varredura: "rtt" = BISECT_RTT, senao a do driver (BISECT). */
    public static String[][] steps(String set) {
        return "rtt".equals(set) ? BISECT_RTT : BISECT;
    }

    /** O kernel tem o kgsl (Android de fabrica)? Os drivers do AdrenoTools so funcionam com ele. */
    public static boolean hasKgsl() {
        return new File("/dev/kgsl-3d0").exists();
    }
}
