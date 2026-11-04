package com.dfdx047.zandatsu.diag

import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.os.Process
import android.system.Os
import com.dfdx047.zandatsu.data.AppPaths
import com.dfdx047.zandatsu.drivers.DriverKind
import com.dfdx047.zandatsu.drivers.DriverRepository
import com.dfdx047.zandatsu.drivers.TurnipSupport
import org.json.JSONObject
import java.io.File

/**
 * Um micro-teste de Vulkan por processo (android:process=":diag"). O [DiagRunner] arranca o servico com o id do teste;
 * no fim o processo mata-se, para o teste seguinte comecar num processo limpo. Se o driver o derrubar a meio, o
 * <test>.progress fica no disco com o passo, e o runner regista "fechou em <passo>".
 */
class DiagService : Service() {
    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val test = intent?.getStringExtra(EXTRA_TEST) ?: return START_NOT_STICKY
        val out = intent.getStringExtra(EXTRA_OUT) ?: return START_NOT_STICKY
        val options = intent.getStringExtra(EXTRA_OPTIONS).orEmpty()
        Thread({
            val dir = File(out)
            try {
                val (driverDir, driverName) = prepareDriver()
                DiagNative.run(test, out, applicationInfo.nativeLibraryDir, driverDir, driverName, options)
            } catch (t: Throwable) {
                try {
                    File(dir, "$test.json").writeText(
                        JSONObject().put("id", test).put("status", "fail")
                            .put("message", "erro no app: ${t.javaClass.simpleName}: ${t.message}").toString(),
                    )
                } catch (_: Exception) {
                }
            }
            stopSelf()
            Process.killProcess(Process.myPid())
        }, "diag-$test").start()
        return START_NOT_STICKY
    }

    /**
     * O driver ativo do jogo (Turnip, RADV ou PanVK), copiado para a pasta interna como no GameActivity; (null, null) =
     * o do sistema. No Turnip tambem as variaveis que o jogo poe sozinho (TurnipSupport.applyAutoEnv).
     */
    private fun prepareDriver(): Pair<String?, String?> {
        val active = AppPaths.activeDriverDir
        val lib = DriverRepository.libraryIn(active) ?: return null to null
        val kind = DriverRepository.kindOf(lib.name)
        if (!DriverRepository.fitsDevice(kind, false)) return null to null
        val target = File(filesDir, "diag_driver")
        target.deleteRecursively()
        target.mkdirs()
        active.listFiles()?.filter { it.isFile && it.name.endsWith(".so") }?.forEach { it.copyTo(File(target, it.name), true) }
        if (kind == DriverKind.TURNIP) {
            val env = HashMap<String, String>()
            try {
                TurnipSupport.applyAutoEnv(TurnipSupport.detect(), env)
            } catch (_: Throwable) {
            }
            env.forEach { (k, v) -> Os.setenv(k, v, true) }
        }
        return target.absolutePath + "/" to lib.name
    }

    companion object {
        const val EXTRA_TEST = "test"
        const val EXTRA_OUT = "out"
        const val EXTRA_OPTIONS = "options"
        const val PROCESS_SUFFIX = ":diag"
    }
}
