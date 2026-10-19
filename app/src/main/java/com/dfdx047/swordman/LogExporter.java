package com.dfdx047.swordman;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

/**
 * Junta num .zip o que ajuda a depurar e grava em Download/SwordMan-logs-<data>.zip.
 * O layout e o mesmo do "Exportar logs" do DBZ3-Android, que o OptXbox le:
 *
 *   aparelho.txt             modelo, Android, SoC, driver, ficheiros do jogo
 *   logcat.txt               logcat do app (o jogo corre no processo :game)
 *   logs/swordman_last.log   log da sessao (linhas "dbz3: perf ...", fins de passe, ...)
 *   logs/capture/*.dfc       a captura de frames mais recente e os .timing.txt
 *   cvars.txt                opcoes do runtime usadas
 *
 * No PC:  python3 -m optxbox report --adb --adb-pattern "SwordMan-logs-*.zip"
 */
final class LogExporter {
    private LogExporter() {}

    static String export(Context context) throws IOException {
        String stamp = new SimpleDateFormat("yyyyMMdd-HHmmss", Locale.ROOT).format(new Date());
        String name = "SwordMan-logs-" + stamp + ".zip";
        File tmp = new File(context.getCacheDir(), name);
        File appDir = AppPaths.appDir(context);
        File logs = AppPaths.logsDir(context);

        try (ZipOutputStream zip = new ZipOutputStream(new FileOutputStream(tmp))) {
            putText(zip, "aparelho.txt", deviceReport(context));
            putText(zip, "logcat.txt", run("logcat", "-d", "-v", "threadtime"));
            File[] files = logs.listFiles();
            if (files != null) {
                for (File f : files) {
                    putFile(zip, "logs/" + f.getName(), f);
                }
            }
            File capture = new File(logs, "capture");
            File[] captures = capture.listFiles();
            if (captures != null) {
                File newest = null;
                for (File f : captures) {
                    if (f.getName().endsWith(".dfc")
                            && (newest == null || f.lastModified() > newest.lastModified())) {
                        newest = f;
                    }
                    if (f.getName().endsWith(".timing.txt")) {
                        putFile(zip, "logs/capture/" + f.getName(), f);
                    }
                }
                if (newest != null) {
                    putFile(zip, "logs/capture/" + newest.getName(), newest);
                }
            }
            putFile(zip, "cvars.txt", new File(appDir, "cvars.txt"));
        }

        ContentValues values = new ContentValues();
        values.put(MediaStore.Downloads.DISPLAY_NAME, name);
        values.put(MediaStore.Downloads.MIME_TYPE, "application/zip");
        values.put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS);
        ContentResolver resolver = context.getContentResolver();
        Uri uri = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);
        if (uri == null) {
            throw new IOException("sem acesso a Download/");
        }
        try (OutputStream out = resolver.openOutputStream(uri);
             InputStream in = new FileInputStream(tmp)) {
            if (out == null) {
                throw new IOException("sem acesso a Download/");
            }
            copy(in, out);
        }
        //noinspection ResultOfMethodCallIgnored
        tmp.delete();
        return name;
    }

    private static String deviceReport(Context context) {
        StringBuilder sb = new StringBuilder();
        sb.append("Project SwordMan ").append(BuildConfig.VERSION_NAME)
                .append(" (").append(BuildConfig.VERSION_CODE).append(")\n");
        sb.append("Aparelho: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL)
                .append(" (").append(Build.DEVICE).append(")\n");
        sb.append("Android: ").append(Build.VERSION.RELEASE)
                .append(" (API ").append(Build.VERSION.SDK_INT).append(")\n");
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            sb.append("SoC: ").append(Build.SOC_MANUFACTURER).append(' ').append(Build.SOC_MODEL).append('\n');
        }
        sb.append("Hardware: ").append(Build.HARDWARE).append("  Board: ").append(Build.BOARD).append('\n');
        File[] drivers = new File(AppPaths.appDir(context), "driver").listFiles();
        String driver = "sistema";
        if (drivers != null) {
            for (File f : drivers) {
                if (f.getName().endsWith(".so")) {
                    driver = f.getName();
                }
            }
        }
        sb.append("Driver Vulkan ativo: ").append(driver).append("\n\n");
        sb.append("Arquivos do jogo:\n");
        sb.append("  default.xex: ").append(AppPaths.xexFile(context).isFile() ? "ok" : "falta").append('\n');
        sb.append("  media/: ").append(AppPaths.mediaDir(context).isDirectory() ? "ok" : "falta").append("\n\n");
        sb.append("getprop (graficos):\n");
        for (String l : run("getprop").split("\n")) {
            String low = l.toLowerCase(Locale.ROOT);
            for (String k : new String[] {"vulkan", "egl", "gfx", "gpu", "opengles", "hwui", "soc",
                    "board.platform", "adreno", "mali"}) {
                if (low.contains(k)) {
                    sb.append("  ").append(l).append('\n');
                    break;
                }
            }
        }
        return sb.toString();
    }

    private static String run(String... cmd) {
        try {
            Process p = new ProcessBuilder(cmd).redirectErrorStream(true).start();
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            try (InputStream in = p.getInputStream()) {
                copy(in, out);
            }
            p.waitFor();
            return out.toString(StandardCharsets.UTF_8.name());
        } catch (Exception e) {
            return "erro: " + e.getMessage();
        }
    }

    private static void putText(ZipOutputStream zip, String entry, String text) throws IOException {
        zip.putNextEntry(new ZipEntry(entry));
        zip.write(text.getBytes(StandardCharsets.UTF_8));
        zip.closeEntry();
    }

    private static void putFile(ZipOutputStream zip, String entry, File f) throws IOException {
        if (!f.isFile()) {
            return;
        }
        zip.putNextEntry(new ZipEntry(entry));
        try (InputStream in = new BufferedInputStream(new FileInputStream(f))) {
            copy(in, zip);
        }
        zip.closeEntry();
    }

    private static void copy(InputStream in, OutputStream out) throws IOException {
        byte[] buf = new byte[64 * 1024];
        int n;
        while ((n = in.read(buf)) > 0) {
            out.write(buf, 0, n);
        }
    }
}
