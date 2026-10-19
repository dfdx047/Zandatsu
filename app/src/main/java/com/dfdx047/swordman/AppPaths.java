package com.dfdx047.swordman;

import android.content.Context;

import java.io.File;

/**
 * Pastas do app. A pasta de dados e a mesma que o GameActivity passa ao runtime em
 * REX_APP_DIR (getExternalFilesDir), ou seja /sdcard/Android/data/com.dfdx047.swordman/files:
 *
 *   game/            jogo (default.xex + media/), copiado com tools/push-game.sh
 *   cvars.txt        opcoes do runtime (nome = valor), lidas ao abrir o jogo
 *   capture.req      pedido de captura de frames (o runtime le-o durante o jogo)
 *   driver/          driver Vulkan alternativo (.so), opcional
 *   logs/            log da ultima sessao e capturas (.dfc)
 */
final class AppPaths {
    private AppPaths() {}

    static File appDir(Context context) {
        File dir = context.getExternalFilesDir(null);
        if (dir == null) {
            dir = context.getFilesDir();
        }
        //noinspection ResultOfMethodCallIgnored
        dir.mkdirs();
        return dir;
    }

    static File gameDir(Context context) {
        return new File(appDir(context), "game");
    }

    static File xexFile(Context context) {
        return new File(gameDir(context), "default.xex");
    }

    static File mediaDir(Context context) {
        return new File(gameDir(context), "media");
    }

    static File logsDir(Context context) {
        return new File(appDir(context), "logs");
    }
}
