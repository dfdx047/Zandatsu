package com.dfdx047.zandatsu.game;

import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;
import android.view.Display;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.Locale;

/**
 * Atividade do jogo. O SDLActivity carrega libSDL3.so e libmain.so e chama
 * SDL_main (windowed_app_main_sdl.cpp) numa thread propria.
 *
 * Aberta pelo launcher (LauncherActivity) ou pelo atalho da tela inicial. As
 * opcoes do jogo ja estao no mgr.toml (escrito pelo launcher); aqui so se
 * aplica a orientacao, o driver Vulkan e os controles por toque (GameOverlay).
 *
 * Antes disso definimos as variaveis de ambiente que o runtime le no Android:
 *   REX_APP_DIR            pasta de dados (jogo, configuracoes, logs, cache)
 *   REX_NATIVE_LIB_DIR     pasta das .so extraidas (plugin da GPU)
 * (O SDK trata REX_APP_DIR como a pasta do executavel: o jogo fica em
 * game/ e o mgr.toml ao lado, como no PC.)
 */
public class GameActivity extends SDLActivity {
    private static final String TAG = "swordman";

    private GameOverlay mOverlay;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        applyOrientation();

        File appDir = getExternalFilesDir(null);
        if (appDir == null) {
            appDir = getFilesDir();
        }
        //noinspection ResultOfMethodCallIgnored
        appDir.mkdirs();

        setEnv("REX_APP_DIR", appDir.getAbsolutePath());
        setEnv("REX_NATIVE_LIB_DIR", getApplicationInfo().nativeLibraryDir);
        // Compilacao de pipelines sincrona: no Android a async escondia 1 frame
        // de cada vez que um pipeline faltava (travadinhas), e a sincrona custa
        // menos de 1 ms por frame com a cache do driver. O cvars.txt pode mudar.
        setEnv("REX_ASYNC_SHADER_COMPILATION", "false");
        // Sem o evento sentinela do SDL: no Android o SDL_WaitEvent poe um sentinela a cada volta (o que o acorda)
        // e o SDL_PeepEvents tira-o logo, e a thread de UI gira a 100% de um nucleo sempre que nao esta bloqueada
        // a pintar (modo Renut: 99% medidos, 23/10). O SDK nao usa SDL_PollEvent, que e quem precisa do sentinela.
        setEnv("SDL_POLL_SENTINEL", "0");
        applyTurnipCompat(appDir);
        applyCvarOverrides(new File(appDir, "cvars.txt"));
        if (!driverCrashedBefore(appDir)) {
            installCustomVulkanDriver(appDir);
        }
        prepareLogFile(appDir);
        com.dfdx047.zandatsu.data.AppLog.INSTANCE.setProcess("game");
        // O driver que este arranque usa fica no registo do app (entra no "Exportar logs").
        com.dfdx047.zandatsu.data.AppLog.INSTANCE.i("driver", mDriverNote);
        com.dfdx047.zandatsu.data.SessionState.INSTANCE.markStarting(appDir);
        com.dfdx047.zandatsu.data.AppLog.INSTANCE.i("game", "arranque: pasta " + appDir.getAbsolutePath() + "/game");
        super.onCreate(savedInstanceState);

        enterImmersive();
        // 60 Hz, ou 120 Hz com os modos de 120 FPS (patch 0092); o menu do jogo muda-o ao vivo.
        mOutputHz = com.dfdx047.zandatsu.data.DisplayRates.wantedHz(this);
        installSurfaceRateCallback();
        watchDisplay();
        requestDisplayRate(mOutputHz);
        // Jogo com carga constante: pede ao sistema para nao deixar a GPU/CPU cair em
        // estados de poupanca entre frames (buracos de 10-15 ms na GPU, logs de
        // 2026-10-13). "env:DF_NO_SUSTAINED = 1" no cvars.txt desliga, para A/B.
        if (!"1".equals(Os.getenv("DF_NO_SUSTAINED"))) {
            try {
                getWindow().setSustainedPerformanceMode(true);
                Log.i(TAG, "Modo de desempenho sustentado: ligado");
            } catch (RuntimeException e) {
                Log.w(TAG, "Modo de desempenho sustentado indisponivel", e);
            }
        }

        // Controles por toque por cima do jogo (se nao foram desligados no launcher).
        mOverlay = new GameOverlay(this);
        mOverlay.onCreate();
        watchFirstFrames(appDir);
        TurnipSweep.arm(this, appDir);
        CpuSampler.start();
    }

    /**
     * Estado da sessao para os avisos de erro do launcher (SessionState): "starting" ate o
     * jogo apresentar os primeiros frames, "running" depois, "ended" ao sair pelo menu. Se o
     * launcher encontrar "starting" ou "running" ao voltar, o jogo fechou sozinho.
     */
    private void watchFirstFrames(File appDir) {
        final android.os.Handler h = new android.os.Handler(android.os.Looper.getMainLooper());
        h.postDelayed(new Runnable() {
            int tries = 0;

            @Override
            public void run() {
                if (isFinishing() || isDestroyed()) {
                    return;
                }
                if (NativeGameState.INSTANCE.frames() > 30) {
                    com.dfdx047.zandatsu.data.SessionState.INSTANCE.markRunning(appDir);
                    com.dfdx047.zandatsu.data.AppLog.INSTANCE.i("game", "primeiros frames apresentados");
                    com.dfdx047.zandatsu.data.DriverBootFails.reset(appDir);
                } else if (++tries < 120) {
                    h.postDelayed(this, 1000);
                } else {
                    com.dfdx047.zandatsu.data.AppLog.INSTANCE.w("game", "2 min sem frames", null);
                }
            }
        }, 2000);
    }

    /**
     * Ecra inteiro imersivo (sem barra de estado nem de navegacao, por baixo do recorte).
     * Sem isto o SDL aplicava o estilo "janela" (o jogo cria a janela com fullscreen=false)
     * e a superficie ficava mais pequena que o ecra: 1920x918 num Retroid Pocket 5 de
     * 1920x1080 (logs de 2026-10-14), com a imagem cortada/encolhida nos portateis 16:9.
     * O SDK tambem passou a pedir ecra inteiro no Android; isto fica como reforco (e
     * repoe as barras escondidas quando a janela volta a ter foco).
     */
    @SuppressWarnings("deprecation")
    private void enterImmersive() {
        Window window = getWindow();
        window.clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
        window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        WindowManager.LayoutParams params = window.getAttributes();
        if (params.layoutInDisplayCutoutMode
                != WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES) {
            params.layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            window.setAttributes(params);
        }
        window.setDecorFitsSystemWindows(false);
        WindowInsetsController controller = window.getInsetsController();
        if (controller != null) {
            controller.hide(WindowInsets.Type.systemBars());
            controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            enterImmersive();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (mOverlay != null) {
            mOverlay.onResume();
        }
    }

    @Override
    protected void onPause() {
        if (mOverlay != null) {
            mOverlay.onPause();
        }
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        if (mDisplayListener != null) {
            android.hardware.display.DisplayManager dm =
                    (android.hardware.display.DisplayManager) getSystemService(DISPLAY_SERVICE);
            if (dm != null) {
                dm.unregisterDisplayListener(mDisplayListener);
            }
            mDisplayListener = null;
        }
        if (mOverlay != null) {
            mOverlay.onDestroy();
            mOverlay = null;
        }
        super.onDestroy();
    }

    /** Superficie do jogo (autoteste do render: fotos por PixelCopy). */
    public static android.view.SurfaceView gameSurface() {
        return mSurface;
    }

    /** Com o menu dos controles aberto, o Back do Android fecha o menu (nao vai para o jogo). */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        AutoPilot.onKeyEvent(event);
        if (mOverlay != null) {
            mOverlay.onPhysicalInput(event);
            // Menu do jogo do Android: navegacao pelo comando e Back + Start para o abrir.
            if (mOverlay.onMenuKeyEvent(event)) {
                return true;
            }
        }
        // Back (o botao Back do Odin e de outros portateis, ou o gesto de voltar):
        // com um menu aberto volta um nivel; com o jogo a correr abre o menu.
        if (mOverlay != null && event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            if (event.getAction() == KeyEvent.ACTION_UP && !event.isCanceled()) {
                if (mOverlay.isMenuOpen()) {
                    mOverlay.onBackPressed();
                } else {
                    mOverlay.openMenu();
                }
            }
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (mOverlay != null) {
            mOverlay.onPhysicalInput(event);
            if (mOverlay.onMenuMotion(event)) {
                return true;
            }
        }
        return super.dispatchGenericMotionEvent(event);
    }

    /** Controles no modo automatico: o primeiro toque na tela fa-los aparecer. */
    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (mOverlay != null && mOverlay.onScreenTouch(event)) {
            return true;
        }
        return super.dispatchTouchEvent(event);
    }

    /** Taxa de saida pedida (60, ou 120 com os modos de 120 FPS). */
    private int mOutputHz = 60;
    private android.hardware.display.DisplayManager.DisplayListener mDisplayListener;

    /**
     * O jogo gera os frames por um relogio proprio (o vblank do Xenos), que nao esta preso a
     * tela. Num ecra de 120 Hz com FIFO e o jogo a 60, cada frame fica 2 vblanks na tela, mas
     * basta o relogio oscilar 1 ms para alguns ficarem 1 ou 3 (8 ms / 25 ms): e o "stutter" que
     * se ve mesmo com o contador nos 60. O preferredRefreshRate sozinho e so uma sugestao que
     * varios fabricantes ignoram, por isso pedimos o modo pelo id e, alem disso, marcamos a
     * superficie do SDL como fonte fixa desse numero de FPS (API 30).
     *
     * 0092: a taxa segue a escolha do jogador: 60 Hz para 30/60 FPS; 120 Hz para os dois modos de
     * 120 FPS (o jogo a 120, ou a 60 com um quadro gerado entre cada dois pelo presenter). Sem
     * modo de 120 Hz na tela fica o de 60 Hz, com um aviso no log (o launcher ja esconde os
     * 120 FPS nessas telas). A taxa real da tela vai para o runtime (NativeGameState.setDisplayHz),
     * que so gera quadros se ela chegar.
     */
    public void requestDisplayRate(int hz) {
        mOutputHz = hz;
        WindowManager.LayoutParams params = getWindow().getAttributes();
        StringBuilder modes = new StringBuilder();
        Display.Mode best = null;
        try {
            Display display = getWindowManager().getDefaultDisplay();
            for (Display.Mode m : display.getSupportedModes()) {
                modes.append(String.format(Locale.ROOT, " %d:%dx%d@%.2f", m.getModeId(),
                        m.getPhysicalWidth(), m.getPhysicalHeight(), m.getRefreshRate()));
            }
            best = com.dfdx047.zandatsu.data.DisplayRates.pickMode(display, hz);
            if (best == null && hz != 60) {
                Log.w(TAG, String.format(Locale.ROOT,
                        "Ecra: sem modo de %d Hz nesta tela (maximo %.0f Hz): fica o de 60 Hz",
                        hz, com.dfdx047.zandatsu.data.DisplayRates.maxRefreshHz(display)));
                mOutputHz = 60;
                best = com.dfdx047.zandatsu.data.DisplayRates.pickMode(display, 60);
            }
            params.preferredRefreshRate = best != null ? best.getRefreshRate() : mOutputHz;
            params.preferredDisplayModeId = best != null ? best.getModeId() : 0;
            Log.i(TAG, String.format(Locale.ROOT, "Ecra: atual %.2f Hz, pedido %d Hz, modo pedido %s, modos:%s",
                    display.getRefreshRate(), hz,
                    best != null ? best.getModeId() + " (" + Math.round(best.getRefreshRate()) + " Hz)"
                            : "nenhum de " + mOutputHz + " Hz",
                    modes));
        } catch (RuntimeException e) {
            Log.w(TAG, "Ecra: nao foi possivel escolher o modo de " + hz + " Hz", e);
            params.preferredRefreshRate = mOutputHz;
        }
        getWindow().setAttributes(params);
        applySurfaceFrameRate();
    }

    /** Surface.setFrameRate com a taxa de saida atual (fonte fixa). */
    private void applySurfaceFrameRate() {
        if (mSurface == null) {
            return;
        }
        Surface surface = mSurface.getHolder().getSurface();
        if (surface == null || !surface.isValid()) {
            return;
        }
        try {
            surface.setFrameRate((float) mOutputHz, Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE);
        } catch (RuntimeException e) {
            Log.w(TAG, "Surface.setFrameRate falhou", e);
        }
    }

    private void installSurfaceRateCallback() {
        if (mSurface == null) {
            return;
        }
        mSurface.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                applySurfaceFrameRate();
                // O sistema muda o modo com algum atraso: regista o que ficou.
                mSurface.postDelayed(() -> {
                    Display d = getWindowManager().getDefaultDisplay();
                    Log.i(TAG, String.format(Locale.ROOT, "Ecra durante o jogo: %.2f Hz (pedido %d Hz)",
                            d.getRefreshRate(), mOutputHz));
                    reportDisplayRate();
                }, 3000);
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
            }
        });
    }

    /** A taxa real da tela vai para o runtime (o presenter so gera quadros se ela chegar). */
    private void reportDisplayRate() {
        try {
            float hz = getWindowManager().getDefaultDisplay().getRefreshRate();
            NativeGameState.INSTANCE.setDisplayHz(hz);
        } catch (RuntimeException e) {
            Log.w(TAG, "Ecra: taxa da tela indisponivel", e);
        }
    }

    private void watchDisplay() {
        android.hardware.display.DisplayManager dm =
                (android.hardware.display.DisplayManager) getSystemService(DISPLAY_SERVICE);
        if (dm == null) {
            return;
        }
        mDisplayListener = new android.hardware.display.DisplayManager.DisplayListener() {
            @Override
            public void onDisplayAdded(int displayId) {
            }

            @Override
            public void onDisplayRemoved(int displayId) {
            }

            @Override
            public void onDisplayChanged(int displayId) {
                if (displayId == Display.DEFAULT_DISPLAY) {
                    reportDisplayRate();
                }
            }
        };
        dm.registerDisplayListener(mDisplayListener, new android.os.Handler(android.os.Looper.getMainLooper()));
        reportDisplayRate();
    }

    /** Orientacao escolhida no launcher: paisagem (as duas), vertical ou pelo sensor (todas). */
    private void applyOrientation() {
        @SuppressWarnings("deprecation")
        SharedPreferences prefs = getSharedPreferences("swordman", MODE_MULTI_PROCESS);
        String mode = prefs.getString("game_orientation", "landscape");
        int orientation;
        if ("portrait".equals(mode)) {
            orientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT;
        } else if ("auto".equals(mode)) {
            orientation = ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR;
        } else {
            orientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
        }
        setRequestedOrientation(orientation);
    }

    /**
     * O log desta sessao vai para logs/swordman_last.log (os avisos de erro do
     * launcher mostram o fim dele). O cvars.txt pode mudar.
     */
    private static void prepareLogFile(File appDir) {
        File logs = new File(appDir, "logs");
        //noinspection ResultOfMethodCallIgnored
        logs.mkdirs();
        File log = new File(logs, "swordman_last.log");
        //noinspection ResultOfMethodCallIgnored
        log.delete();
        // As partes rodadas (.1.log ...) sao de sessoes antigas: so confundiam a
        // exportacao de logs (20 MB de uma sessao com o log detalhado).
        File[] old = logs.listFiles();
        if (old != null) {
            for (File f : old) {
                String n = f.getName();
                if (n.startsWith("swordman_last.") && !n.equals("swordman_last.log")) {
                    //noinspection ResultOfMethodCallIgnored
                    f.delete();
                }
            }
        }
        if (Os.getenv("REX_LOG_FILE") == null) {
            setEnv("REX_LOG_FILE", log.getAbsolutePath());
        }
    }

    @Override
    protected String[] getLibraries() {
        // libmain.so depende de librexruntime.so, librexgpu-xenos.so e
        // libc++_shared.so, que o linker do Android carrega sozinho.
        return new String[] {"SDL3", "main"};
    }

    /**
     * Correcoes para glitches do Turnip nos Adreno 6xx, escolhidas no launcher (aba
     * Drivers) como interruptores independentes (bits da pref "turnip_compat_flags").
     * Antes do cvars.txt, que pode sobrepor qualquer uma. TU_DEBUG so tem efeito no
     * Turnip; o driver da Qualcomm ignora-o.
     *   1  memoria do jogo num so storage buffer de 512 MB (patch 0025): o Turnip dos 6xx
     *      diz 128 MB e a memoria fica partida em 4 com um switch em cada acesso dos
     *      shaders; com isto a geometria do RP5 fica certa (2026-10-13 07:58)
     *   2  TU_DEBUG=sysmem (sem GMEM)
     *   4  TU_DEBUG=nolrz,noubwc
     *   8  texturas BC descomprimidas (vulkan_mask_features=textureCompressionBC)
     *   16 sem dynamic rendering e sem stencil export
     *   32 depth em float 32 (vulkan_disable_depth_unorm24)
     *   64 diagnostico: TU_DEBUG=flushall,syncdraw (se corrigir, falta uma barreira)
     */
    static final int TURNIP_SINGLE_BUFFER = 1, TURNIP_SYSMEM = 2, TURNIP_NOLRZ_UBWC = 4,
            TURNIP_BC_DECOMPRESS = 8, TURNIP_SIMPLE_PASSES = 16, TURNIP_DEPTH_F32 = 32,
            TURNIP_FULL_SYNC = 64,
            // Compilador de shaders do Turnip (ir3): se a imagem errada for um shader mal
            // compilado (as texturas do jogo no RP5 com Turnip, 16/10), um destes muda-a.
            TURNIP_IR3_NOPREAMBLE = 128, TURNIP_IR3_NOUBOOPT = 256, TURNIP_IR3_NOFP16 = 512;

    /** Que driver Vulkan este arranque usa (vai para o AppLog). */
    private String mDriverNote = "sistema";

    /**
     * O jogo nunca volta sozinho ao driver do sistema: as otimizacoes deste port sao feitas para
     * o Turnip. Se o Turnip fechar o jogo antes do primeiro frame, o launcher conta a falha
     * (DriverBootFails) e, a partir da 3a seguida, sugere o driver do sistema com o log para
     * enviar. Aqui so se limpam as chaves da protecao antiga (que trocava sozinha de driver).
     */
    private boolean driverCrashedBefore(File appDir) {
        @SuppressWarnings("deprecation")
        SharedPreferences prefs = getSharedPreferences("swordman", MODE_MULTI_PROCESS);
        if (prefs.contains("driver_boot_pending") || prefs.contains("driver_boot_failed")) {
            prefs.edit().remove("driver_boot_pending").remove("driver_boot_failed").commit();
        }
        int fails = com.dfdx047.zandatsu.data.DriverBootFails.INSTANCE.read(appDir);
        if (fails > 0) {
            mDriverNote = "(" + fails + " arranque(s) seguidos sem frames com o driver alternativo)";
        }
        return false;
    }

    /** TU_DEBUG que a tabela do TurnipSupport aplica sozinho (vazio = nenhum). */
    private String autoTuDebug = "";

    private void applyTurnipCompat(File appDir) {
        @SuppressWarnings("deprecation")
        SharedPreferences prefs = getSharedPreferences("swordman", MODE_MULTI_PROCESS);
        // 23/10: os interruptores manuais e a varredura sairam do launcher (os drivers ja funcionam
        // com o que o TurnipSupport aplica sozinho). Valores antigos gravados sao apagados.
        if (prefs.contains("turnip_compat_flags") || prefs.getBoolean("turnip_bisect", false)) {
            prefs.edit().remove("turnip_compat_flags").putBoolean("turnip_bisect", false).commit();
        }
        int flags = 0;
        // Com driver alternativo: o que a tabela do TurnipSupport manda para este Snapdragon
        // (ex. TU_DEBUG=sysmem no A830 e nos A710/720/722). O buffer unico de 512 MB e o BC
        // descomprimido ficam so como escolha manual: o Turnip diz 128 MB em todas as geracoes e
        // o problema das cores no RP5 era o do passo 4 (patch 0077), nao estes.
        File[] drivers = new File(appDir, "driver").listFiles((dir, name) -> name.endsWith(".so"));
        boolean custom = drivers != null && drivers.length > 0;
        com.dfdx047.zandatsu.drivers.TurnipSupport.Gpu gpu =
                com.dfdx047.zandatsu.drivers.TurnipSupport.detect();
        Log.i(TAG, "GPU: " + gpu + " kgsl=" + com.dfdx047.zandatsu.drivers.TurnipSupport.hasKgsl()
                + " driver=" + (custom ? drivers[0].getName() : "sistema"));
        if (custom && prefs.getBoolean("turnip_bisect", false)) {
            // Bisect do Turnip: so o passo desta vez, nada mais (para o resultado ser claro).
            String[][] steps = com.dfdx047.zandatsu.drivers.TurnipSupport.steps(
                    prefs.getString("turnip_sweep_set", "driver"));
            int step = Math.floorMod(prefs.getInt("turnip_bisect_step", 0), steps.length);
            prefs.edit().putInt("turnip_bisect_step", (step + 1) % steps.length)
                    .putString(TurnipSweep.KEY_LABEL, steps[step][0]).commit();
            for (String kv : steps[step][1].split(";")) {
                int eq = kv.indexOf('=');
                if (eq > 0) setEnv(kv.substring(0, eq), kv.substring(eq + 1));
            }
            String line = "Turnip bisect passo " + steps[step][0] + " [" + steps[step][1] + "] driver "
                    + drivers[0].getName();
            Log.i(TAG, line);
            try {
                File logs = new File(appDir, "logs");
                //noinspection ResultOfMethodCallIgnored
                logs.mkdirs();
                try (java.io.FileWriter w = new java.io.FileWriter(new File(logs, "turnip_bisect.txt"), true)) {
                    w.write(new java.util.Date() + "  " + line + "\n");
                }
            } catch (IOException e) {
                Log.w(TAG, "turnip_bisect.txt", e);
            }
            return;
        }
        if (custom) {
            java.util.HashMap<String, String> env = new java.util.HashMap<>();
            com.dfdx047.zandatsu.drivers.TurnipSupport.applyAutoEnv(gpu, env);
            for (java.util.Map.Entry<String, String> e : env.entrySet()) {
                if (!e.getKey().equals("TU_DEBUG")) {
                    setEnv(e.getKey(), e.getValue());
                    Log.i(TAG, "Turnip: " + e.getKey() + "=" + e.getValue() + " (automatico)");
                }
            }
            autoTuDebug = env.containsKey("TU_DEBUG") ? env.get("TU_DEBUG") : "";
        }
        if (flags == 0 && autoTuDebug.isEmpty()) {
            return;
        }
        StringBuilder tu = new StringBuilder();
        if (!autoTuDebug.isEmpty()) tu.append(autoTuDebug).append(',');
        if ((flags & TURNIP_SYSMEM) != 0) tu.append("sysmem,");
        if ((flags & TURNIP_NOLRZ_UBWC) != 0) tu.append("nolrz,noubwc,");
        if ((flags & TURNIP_FULL_SYNC) != 0) tu.append("flushall,syncdraw,");
        if (tu.length() > 0) {
            tu.setLength(tu.length() - 1);
            setEnv("TU_DEBUG", tu.toString());
        }
        StringBuilder ir3 = new StringBuilder();
        if ((flags & TURNIP_IR3_NOPREAMBLE) != 0) ir3.append("nopreamble,");
        if ((flags & TURNIP_IR3_NOUBOOPT) != 0) ir3.append("nouboopt,");
        if ((flags & TURNIP_IR3_NOFP16) != 0) ir3.append("nofp16,");
        if (ir3.length() > 0) {
            ir3.setLength(ir3.length() - 1);
            setEnv("IR3_SHADER_DEBUG", ir3.toString());
            Log.i(TAG, "Turnip: IR3_SHADER_DEBUG=" + ir3);
        }
        if ((flags & TURNIP_SINGLE_BUFFER) != 0) {
            setEnv("REX_DBZ3_VK_MAX_STORAGE_BUFFER_RANGE_MB", "512");
        }
        if ((flags & TURNIP_BC_DECOMPRESS) != 0) {
            setEnv("REX_VULKAN_MASK_FEATURES", "textureCompressionBC");
        }
        if ((flags & TURNIP_SIMPLE_PASSES) != 0) {
            setEnv("REX_VULKAN_DYNAMIC_RENDERING", "false");
            setEnv("REX_DBZ3_VK_DISABLE_EXTENSIONS", "VK_EXT_shader_stencil_export");
        }
        if ((flags & TURNIP_DEPTH_F32) != 0) {
            setEnv("REX_VULKAN_DISABLE_DEPTH_UNORM24", "true");
        }
        Log.i(TAG, "Correcoes do Turnip: flags " + flags + " TU_DEBUG=" + tu);
    }

    /**
     * Le cvars.txt da pasta da app (linhas "nome = valor", # para comentarios) e
     * passa cada uma ao runtime como variavel REX_NOME. Serve para testar opcoes
     * do runtime sem recompilar: editar o ficheiro e reabrir o jogo.
     */
    private static void applyCvarOverrides(File file) {
        if (!file.isFile()) {
            return;
        }
        try (BufferedReader reader = new BufferedReader(new FileReader(file))) {
            String line;
            while ((line = reader.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) {
                    continue;
                }
                int eq = line.indexOf('=');
                if (eq <= 0) {
                    continue;
                }
                String name = line.substring(0, eq).trim();
                String value = line.substring(eq + 1).trim();
                if (value.length() >= 2 && value.startsWith("\"") && value.endsWith("\"")) {
                    value = value.substring(1, value.length() - 1);
                }
                if (name.startsWith("env:")) {
                    // "env:TU_DEBUG = sysmem": variavel crua para o driver (testes A/B
                    // do Turnip, MESA_*, etc.), sem o prefixo REX_.
                    String env = name.substring(4).trim();
                    setEnv(env, value);
                    Log.i(TAG, "env " + env + " = " + value);
                    continue;
                }
                setEnv("REX_" + name.toUpperCase(Locale.ROOT), value);
                Log.i(TAG, "cvar " + name + " = " + value);
            }
        } catch (IOException e) {
            Log.e(TAG, "Falha a ler " + file, e);
        }
    }

    /**
     * Driver Vulkan alternativo (ex. Turnip): se a pasta "driver" da pasta da app
     * tiver um .so, copia-o para a pasta interna (o Android nao deixa carregar
     * bibliotecas a partir do armazenamento partilhado) e diz ao runtime para o
     * usar via libadrenotools. Sem .so nessa pasta, usa-se o driver do sistema.
     */
    private void installCustomVulkanDriver(File appDir) {
        File internalDir = new File(getFilesDir(), "driver");
        File[] candidates = new File(appDir, "driver").listFiles(
                (dir, name) -> name.endsWith(".so"));
        if (candidates == null || candidates.length == 0) {
            deleteRecursively(internalDir);
            return;
        }
        // Fora do Adreno so o driver do sistema (24/10: o MaliSX2 corrompia a imagem no Mali-G57).
        if (!com.dfdx047.zandatsu.drivers.DriverRepository.INSTANCE.isAdreno()) {
            deleteRecursively(internalDir);
            mDriverNote = "sistema (nao e Adreno: " + candidates[0].getName() + " ignorado)";
            return;
        }
        File source = candidates[0];
        File target = new File(internalDir, source.getName());
        try {
            if (!target.isFile() || target.length() != source.length()
                    || target.lastModified() != source.lastModified()) {
                deleteRecursively(internalDir);
                //noinspection ResultOfMethodCallIgnored
                internalDir.mkdirs();
                Files.copy(source.toPath(), target.toPath(), StandardCopyOption.REPLACE_EXISTING);
                //noinspection ResultOfMethodCallIgnored
                target.setLastModified(source.lastModified());
            }
            setEnv("REX_VULKAN_DRIVER_DIR", internalDir.getAbsolutePath() + "/");
            setEnv("REX_VULKAN_DRIVER_NAME", target.getName());
            Log.i(TAG, "Driver Vulkan alternativo: " + target.getName());
            mDriverNote = "alternativo: " + target.getName() + (mDriverNote.startsWith("(") ? " " + mDriverNote : "");
        } catch (IOException e) {
            Log.e(TAG, "Falha a instalar o driver " + source, e);
            mDriverNote = "sistema (falha a copiar " + source.getName() + ": " + e.getMessage() + ")";
        }
    }

    private static void deleteRecursively(File file) {
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteRecursively(child);
            }
        }
        //noinspection ResultOfMethodCallIgnored
        file.delete();
    }

    private static void setEnv(String name, String value) {
        try {
            Os.setenv(name, value, true);
        } catch (ErrnoException e) {
            Log.e(TAG, "Falha a definir " + name, e);
        }
    }
}
