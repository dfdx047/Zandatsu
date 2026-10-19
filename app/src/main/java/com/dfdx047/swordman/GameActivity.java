package com.dfdx047.swordman;

import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;
import android.view.Display;
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
 * Atividade do jogo. O SDLActivity carrega libSDL3.so e libmain.so e chama SDL_main
 * (windowed_app_main_sdl.cpp do SDK) numa thread propria.
 *
 * Antes disso definimos as variaveis de ambiente que o runtime le no Android:
 *   REX_APP_DIR         pasta de dados (jogo em game/, cvars.txt, logs, cache)
 *   REX_NATIVE_LIB_DIR  pasta das .so extraidas (plugin da GPU, hooks do adrenotools)
 *   REX_LOG_FILE        log desta sessao (logs/swordman_last.log)
 *
 * Adaptado do GameActivity do DBZ3-Android (Dragon Fury), sem os controles por
 * toque e os menus do port (ficam para depois).
 */
public class GameActivity extends SDLActivity {
    static final String TAG = "swordman";
    static final String LAST_LOG = "swordman_last.log";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        File appDir = AppPaths.appDir(this);

        setEnv("REX_APP_DIR", appDir.getAbsolutePath());
        setEnv("REX_NATIVE_LIB_DIR", getApplicationInfo().nativeLibraryDir);
        // Compilacao de pipelines sincrona (como no Dragon Fury): no Android a async
        // escondia 1 frame de cada vez que faltava um pipeline. O cvars.txt pode mudar.
        setEnv("REX_DBZ3_ASYNC_SHADERS", "false");
        applyCvarOverrides(new File(appDir, "cvars.txt"));
        installCustomVulkanDriver(appDir);
        prepareLogFile(appDir);

        Log.i(TAG, "Pasta do jogo: " + AppPaths.gameDir(this).getAbsolutePath());
        super.onCreate(savedInstanceState);

        enterImmersive();
        requestSixtyHz();
        // Carga constante: pede ao sistema para nao deixar a GPU/CPU cair em estados
        // de poupanca entre frames. "env:SM_NO_SUSTAINED = 1" no cvars.txt desliga.
        if (!"1".equals(Os.getenv("SM_NO_SUSTAINED"))) {
            try {
                getWindow().setSustainedPerformanceMode(true);
                Log.i(TAG, "Modo de desempenho sustentado: ligado");
            } catch (RuntimeException e) {
                Log.w(TAG, "Modo de desempenho sustentado indisponivel", e);
            }
        }
    }

    @Override
    protected String[] getLibraries() {
        // libmain.so depende de librexruntime.so, librexgpu-xenos.so e
        // libc++_shared.so, que o linker do Android carrega sozinho.
        return new String[] {"SDL3", "main"};
    }

    /** Ecra inteiro imersivo (sem barras do sistema, por baixo do recorte). */
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

    /**
     * O jogo gera os frames por um relogio proprio (o vblank do Xenos), que nao esta
     * preso ao ecra. Pede o modo de 60 Hz pelo id e marca a superficie do SDL como
     * fonte fixa de 60 FPS, para evitar stutter em ecras de 90/120 Hz.
     */
    private void requestSixtyHz() {
        WindowManager.LayoutParams params = getWindow().getAttributes();
        params.preferredRefreshRate = 60.0f;
        try {
            Display display = getWindowManager().getDefaultDisplay();
            Display.Mode current = display.getMode();
            Display.Mode best = null;
            for (Display.Mode m : display.getSupportedModes()) {
                if (m.getPhysicalWidth() != current.getPhysicalWidth()
                        || m.getPhysicalHeight() != current.getPhysicalHeight()) {
                    continue;
                }
                if (Math.abs(m.getRefreshRate() - 60.0f) < 1.0f) {
                    best = m;
                }
            }
            if (best != null) {
                params.preferredDisplayModeId = best.getModeId();
            }
            Log.i(TAG, String.format(Locale.ROOT, "Ecra: atual %.2f Hz, modo pedido %s",
                    display.getRefreshRate(), best != null ? String.valueOf(best.getModeId()) : "nenhum de 60 Hz"));
        } catch (RuntimeException e) {
            Log.w(TAG, "Ecra: nao foi possivel escolher o modo de 60 Hz", e);
        }
        getWindow().setAttributes(params);

        if (mSurface != null) {
            mSurface.getHolder().addCallback(new SurfaceHolder.Callback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                }

                @Override
                public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                    try {
                        holder.getSurface().setFrameRate(60.0f, Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE);
                    } catch (RuntimeException e) {
                        Log.w(TAG, "Surface.setFrameRate falhou", e);
                    }
                }

                @Override
                public void surfaceDestroyed(SurfaceHolder holder) {
                }
            });
        }
    }

    /** O log desta sessao vai para logs/swordman_last.log (o cvars.txt pode mudar). */
    private static void prepareLogFile(File appDir) {
        File logs = new File(appDir, "logs");
        //noinspection ResultOfMethodCallIgnored
        logs.mkdirs();
        File log = new File(logs, LAST_LOG);
        //noinspection ResultOfMethodCallIgnored
        log.delete();
        File[] old = logs.listFiles();
        if (old != null) {
            for (File f : old) {
                String n = f.getName();
                if (n.startsWith("swordman_last.") && !n.equals(LAST_LOG)) {
                    //noinspection ResultOfMethodCallIgnored
                    f.delete();
                }
            }
        }
        if (Os.getenv("REX_LOG_FILE") == null) {
            setEnv("REX_LOG_FILE", log.getAbsolutePath());
        }
    }

    /**
     * Le cvars.txt da pasta do app (linhas "nome = valor", # comenta) e passa cada
     * uma ao runtime como variavel REX_NOME. "env:NOME = valor" define uma variavel
     * crua (ex. env:TU_DEBUG = sysmem para o Turnip). Editar e reabrir o jogo.
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
     * Driver Vulkan alternativo (ex. Turnip): se a pasta driver/ do app tiver um .so,
     * copia-o para a pasta interna (o Android nao carrega bibliotecas do armazenamento
     * partilhado) e diz ao runtime para o usar via libadrenotools. Sem .so, usa o
     * driver do sistema.
     */
    private void installCustomVulkanDriver(File appDir) {
        File internalDir = new File(getFilesDir(), "driver");
        File[] candidates = new File(appDir, "driver").listFiles(
                (dir, name) -> name.endsWith(".so"));
        if (candidates == null || candidates.length == 0) {
            deleteRecursively(internalDir);
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
        } catch (IOException e) {
            Log.e(TAG, "Falha a instalar o driver " + source, e);
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
