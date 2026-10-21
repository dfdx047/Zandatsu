package com.dfdx047.swordman.game

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.view.PixelCopy
import com.dfdx047.swordman.LauncherActivity
import com.dfdx047.swordman.data.AppLog
import com.dfdx047.swordman.data.SessionState
import com.dfdx047.swordman.drivers.TurnipSupport
import java.io.File
import java.io.FileOutputStream

/**
 * Varredura automatica do Turnip (aba Drivers): o bisect do TurnipSupport.BISECT sem ninguem a
 * mexer. Cada arranque do jogo aplica um passo (GameActivity.applyTurnipCompat), fica ~35 s,
 * tira uma foto da tela (logs/turnip_sweep_NN.jpg), anota os FPS em logs/turnip_sweep.txt e
 * volta ao launcher, que abre o jogo de novo com o passo seguinte. Um passo que crasha fica
 * anotado como crash e a varredura continua. No fim o launcher exporta o zip dos logs.
 */
object TurnipSweep {
    private const val PREF = "swordman"
    private const val KEY_ACTIVE = "turnip_sweep"
    const val KEY_LABEL = "turnip_sweep_label"
    private const val RUN_MS = 35_000L

    @Suppress("DEPRECATION")
    private fun prefs(ctx: Context) = ctx.getSharedPreferences(PREF, Context.MODE_MULTI_PROCESS)

    @JvmStatic
    fun isActive(ctx: Context): Boolean = prefs(ctx).getBoolean(KEY_ACTIVE, false)

    fun logFile(appDir: File) = File(File(appDir, "logs"), "turnip_sweep.txt")

    fun append(appDir: File, line: String) {
        try {
            logFile(appDir).also { it.parentFile?.mkdirs() }.appendText(line + "\n")
        } catch (_: Exception) {
        }
        AppLog.i("turnip", line)
    }

    /** Liga a varredura a partir do passo 0 e limpa os resultados antigos. */
    fun start(ctx: Context, appDir: File, set: String = "driver") {
        File(appDir, "logs").listFiles { f -> f.name.startsWith("turnip_sweep") }?.forEach { it.delete() }
        prefs(ctx).edit()
            .putBoolean(KEY_ACTIVE, true)
            .putBoolean("turnip_bisect", true)
            .putInt("turnip_bisect_step", 0)
            .putString("turnip_sweep_set", set)
            .commit()
        append(appDir, "Varredura do Turnip ($set) - ${java.util.Date()} - ${steps(ctx).size} passos, ${RUN_MS / 1000} s cada")
    }

    fun stop(ctx: Context) {
        prefs(ctx).edit().putBoolean(KEY_ACTIVE, false).putBoolean("turnip_bisect", false)
            .putString("turnip_sweep_set", "driver").commit()
    }

    fun label(ctx: Context): String = prefs(ctx).getString(KEY_LABEL, "?") ?: "?"

    /** Proximo passo a correr; 0 depois do ultimo (a varredura acabou). */
    fun steps(ctx: Context): Array<Array<String>> =
        TurnipSupport.steps(prefs(ctx).getString("turnip_sweep_set", "driver"))

    fun nextStep(ctx: Context): Int =
        Math.floorMod(prefs(ctx).getInt("turnip_bisect_step", 0), steps(ctx).size)

    /** No GameActivity (onCreate): ao fim de RUN_MS tira a foto, anota e volta ao launcher. */
    @JvmStatic
    fun arm(activity: Activity, appDir: File) {
        if (!isActive(activity)) return
        val label = prefs(activity).getString(KEY_LABEL, "?") ?: "?"
        val main = Handler(Looper.getMainLooper())
        var framesAt20 = 0L
        main.postDelayed({ framesAt20 = NativeGameState.frames() }, RUN_MS - 10_000)
        main.postDelayed({
            val fps = (NativeGameState.frames() - framesAt20) / 10.0
            val index = label.substringBefore(' ').toIntOrNull() ?: 99
            val shot = File(File(appDir, "logs"), "turnip_sweep_%02d.jpg".format(index))
            capture(activity, shot) { ok ->
                append(appDir, String.format(java.util.Locale.ROOT, "passo %-50s %5.1f fps  foto %s",
                    label, fps, if (ok) shot.name else "falhou"))
                SessionState.markEnded(appDir)
                activity.startActivity(
                    Intent(activity, LauncherActivity::class.java)
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP),
                )
                activity.finishAndRemoveTask()
                main.postDelayed({ android.os.Process.killProcess(android.os.Process.myPid()) }, 400)
            }
        }, RUN_MS)
    }

    private fun capture(activity: Activity, out: File, done: (Boolean) -> Unit) {
        val surface = GameActivity.gameSurface()
        val w = surface?.width ?: 0
        val h = surface?.height ?: 0
        if (surface == null || w <= 0 || h <= 0) {
            done(false)
            return
        }
        val thread = HandlerThread("SwordManSweepShot").apply { start() }
        val bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        val main = Handler(Looper.getMainLooper())
        try {
            PixelCopy.request(surface, bmp, { result ->
                var ok = false
                if (result == PixelCopy.SUCCESS) {
                    try {
                        FileOutputStream(out).use { Bitmap.createScaledBitmap(bmp, 640, 360, true).compress(Bitmap.CompressFormat.JPEG, 85, it) }
                        ok = true
                    } catch (_: Exception) {
                    }
                }
                thread.quitSafely()
                main.post { done(ok) }
            }, Handler(thread.looper))
        } catch (_: Exception) {
            thread.quitSafely()
            done(false)
        }
    }
}
