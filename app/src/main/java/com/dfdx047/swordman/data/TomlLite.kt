package com.dfdx047.swordman.data

import java.io.File

/**
 * Leitor/escritor minimo de TOML para o mgr.toml e para validar os
 * .toml dos mods. Cobre o que o runtime usa: chaves simples, tabelas [a.b]
 * (que o SDK achata para "a_b_chave"), strings, inteiros, decimais e booleanos.
 * Arrays e tabelas inline sao aceites na leitura mas guardados como texto.
 */
object TomlLite {

    sealed class Value {
        data class Str(val v: String) : Value()
        data class Num(val v: Long) : Value()
        data class Dbl(val v: Double) : Value()
        data class Bool(val v: Boolean) : Value()
        data class Raw(val v: String) : Value()
    }

    class ParseError(val line: Int, message: String) : Exception(message)

    /** Le o ficheiro e devolve as chaves achatadas como o SDK as ve. */
    fun read(file: File): Map<String, Value> =
        if (file.isFile) parse(file.readText()) else emptyMap()

    fun parse(text: String): Map<String, Value> {
        val out = LinkedHashMap<String, Value>()
        var prefix = ""
        text.lineSequence().forEachIndexed { index, rawLine ->
            val line = stripComment(rawLine).trim()
            if (line.isEmpty()) return@forEachIndexed
            if (line.startsWith("[")) {
                val end = line.indexOf(']')
                if (end < 0) throw ParseError(index + 1, "']' em falta")
                val name = line.substring(if (line.startsWith("[[")) 2 else 1, end).trim()
                if (name.isEmpty()) throw ParseError(index + 1, "tabela sem nome")
                prefix = name.replace('.', '_').replace("\"", "")
                return@forEachIndexed
            }
            val eq = line.indexOf('=')
            if (eq <= 0) throw ParseError(index + 1, "esperava 'chave = valor'")
            var key = line.substring(0, eq).trim()
            if (key.length >= 2 && (key.first() == '"' || key.first() == '\'') && key.last() == key.first()) {
                key = key.substring(1, key.length - 1)
            }
            if (key.isEmpty()) throw ParseError(index + 1, "chave vazia")
            val value = parseValue(line.substring(eq + 1).trim(), index + 1)
            out[if (prefix.isEmpty()) key else "${prefix}_$key"] = value
        }
        return out
    }

    private fun stripComment(line: String): String {
        var quote: Char? = null
        var i = 0
        while (i < line.length) {
            val c = line[i]
            if (quote != null) {
                if (c == '\\' && quote == '"') {
                    i += 2
                    continue
                }
                if (c == quote) quote = null
            } else {
                if (c == '"' || c == '\'') quote = c
                if (c == '#') return line.substring(0, i)
            }
            i++
        }
        return line
    }

    private fun parseValue(s: String, line: Int): Value {
        if (s.isEmpty()) throw ParseError(line, "valor vazio")
        when {
            s.startsWith("\"") -> {
                if (s.length < 2 || !s.endsWith("\"")) throw ParseError(line, "string sem aspas finais")
                return Value.Str(unescape(s.substring(1, s.length - 1)))
            }
            s.startsWith("'") -> {
                if (s.length < 2 || !s.endsWith("'")) throw ParseError(line, "string sem aspas finais")
                return Value.Str(s.substring(1, s.length - 1))
            }
            s == "true" -> return Value.Bool(true)
            s == "false" -> return Value.Bool(false)
            s.startsWith("[") || s.startsWith("{") -> return Value.Raw(s)
        }
        val clean = s.replace("_", "")
        clean.toLongOrNull()?.let { return Value.Num(it) }
        clean.toDoubleOrNull()?.let { return Value.Dbl(it) }
        throw ParseError(line, "valor invalido: $s")
    }

    private fun unescape(s: String): String {
        val sb = StringBuilder()
        var i = 0
        while (i < s.length) {
            val c = s[i]
            if (c == '\\' && i + 1 < s.length) {
                when (val n = s[i + 1]) {
                    'n' -> sb.append('\n')
                    't' -> sb.append('\t')
                    'r' -> sb.append('\r')
                    '"' -> sb.append('"')
                    '\\' -> sb.append('\\')
                    else -> sb.append('\\').append(n)
                }
                i += 2
            } else {
                sb.append(c)
                i++
            }
        }
        return sb.toString()
    }

    fun format(value: Any): String = when (value) {
        is String -> "\"" + value.replace("\\", "\\\\").replace("\"", "\\\"") + "\""
        is Boolean -> value.toString()
        is Int, is Long -> value.toString()
        is Float -> String.format(java.util.Locale.ROOT, "%.4f", value.toDouble()).trimEnd('0').let { if (it.endsWith('.')) it + "0" else it }
        is Double -> String.format(java.util.Locale.ROOT, "%.4f", value).trimEnd('0').let { if (it.endsWith('.')) it + "0" else it }
        else -> error("tipo nao suportado: ${value::class}")
    }

    /**
     * Atualiza as chaves de topo do ficheiro mantendo o resto (comentarios,
     * outras chaves, tabelas), como o SaveConfig do SDK. Chaves novas entram
     * antes da primeira tabela. Escrita atomica (tmp + rename).
     */
    fun merge(file: File, updates: Map<String, Any>) {
        val lines = if (file.isFile) file.readLines().toMutableList() else mutableListOf()
        val done = HashSet<String>()
        var firstTable = lines.size
        for (i in lines.indices) {
            val t = lines[i].trim()
            if (t.startsWith("[")) {
                if (firstTable == lines.size) firstTable = i
                break
            }
            if (t.isEmpty() || t.startsWith("#")) continue
            val eq = t.indexOf('=')
            if (eq <= 0) continue
            val key = t.substring(0, eq).trim().trim('"')
            val v = updates[key] ?: continue
            lines[i] = "$key = ${format(v)}"
            done += key
        }
        val fresh = updates.filterKeys { it !in done }.map { (k, v) -> "$k = ${format(v)}" }
        lines.addAll(firstTable, fresh)
        file.parentFile?.mkdirs()
        val tmp = File(file.parentFile, file.name + ".tmp")
        tmp.writeText(lines.joinToString("\n", postfix = "\n"))
        if (!tmp.renameTo(file)) {
            file.writeText(tmp.readText())
            tmp.delete()
        }
    }
}
