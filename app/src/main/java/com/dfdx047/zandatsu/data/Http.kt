package com.dfdx047.zandatsu.data

import com.dfdx047.zandatsu.BuildConfig
import java.io.File
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

/** HTTP simples (GitHub API e downloads). Corre sempre fora da thread principal. */
object Http {
    private const val USER_AGENT = "Zandatsu/" + BuildConfig.VERSION_NAME

    private fun open(url: String): HttpURLConnection =
        (URL(url).openConnection() as HttpURLConnection).apply {
            connectTimeout = 10_000
            readTimeout = 20_000
            instanceFollowRedirects = true
            setRequestProperty("User-Agent", USER_AGENT)
            setRequestProperty("Accept", "application/vnd.github+json")
        }

    fun getText(url: String): String {
        val c = open(url)
        try {
            val code = c.responseCode
            if (code !in 200..299) throw IOException("HTTP $code")
            return c.inputStream.bufferedReader().use { it.readText() }
        } finally {
            c.disconnect()
        }
    }

    /** Descarrega para [target] (via .part) e devolve o ficheiro. */
    fun download(url: String, target: File, onProgress: (Long, Long) -> Unit = { _, _ -> }): File {
        target.parentFile?.mkdirs()
        val part = File(target.parentFile, target.name + ".part")
        var c = open(url).apply { setRequestProperty("Accept", "application/octet-stream") }
        try {
            // GitHub redireciona os assets para outro dominio (HttpURLConnection nao segue https->https entre hosts sempre).
            var code = c.responseCode
            var hops = 0
            while (code in 300..399 && hops++ < 5) {
                val loc = c.getHeaderField("Location") ?: break
                c.disconnect()
                c = open(loc).apply { setRequestProperty("Accept", "application/octet-stream") }
                code = c.responseCode
            }
            if (code !in 200..299) throw IOException("HTTP $code")
            val total = c.contentLengthLong
            var done = 0L
            c.inputStream.use { input ->
                part.outputStream().use { out ->
                    val buf = ByteArray(256 * 1024)
                    while (true) {
                        val n = input.read(buf)
                        if (n < 0) break
                        out.write(buf, 0, n)
                        done += n
                        onProgress(done, total)
                    }
                }
            }
            target.delete()
            if (!part.renameTo(target)) throw IOException("rename ${target.name}")
            return target
        } catch (e: Exception) {
            part.delete()
            throw e
        } finally {
            c.disconnect()
        }
    }
}
