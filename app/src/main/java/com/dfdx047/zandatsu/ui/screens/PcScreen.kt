package com.dfdx047.zandatsu.ui.screens

import android.content.Intent
import android.net.Uri
import android.os.Environment
import android.provider.Settings
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.dfdx047.zandatsu.drivers.DriverRepository
import com.dfdx047.zandatsu.pc.PcGame
import com.dfdx047.zandatsu.pc.PcGameActivity
import java.io.File

/**
 * Port de PC (beta): pasta do jogo de PC, driver Vulkan, resolucao livre e JOGAR. Renderizador: DXVK (D3D9 -> Vulkan);
 * OpenGL/gl4es nao serve para o D3D9 SM3 do MGR (PCPorts/gpu/README.md). As opcoes graficas do proprio jogo
 * ficam no menu OPTIONS dentro do jogo.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun PcScreen() {
    val c = LocalContext.current
    var exe by remember { mutableStateOf(PcGame.exePath(c).ifEmpty { PcGame.findExe().orEmpty() }) }
    var driver by remember { mutableStateOf(PcGame.driverDir(c)) }
    var size by remember { mutableStateOf(PcGame.renderSize(c)) }
    val access = Environment.isExternalStorageManager()
    val found = exe.isNotEmpty() && File(exe).isFile
    Column(
        Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("Metal Gear Rising: Revengeance (PC)", style = MaterialTheme.typography.titleLarge)
        Text(
            "Precisa da tua cópia de PC (pasta com o .exe e GameData). O código do jogo só é desencriptado com o teu executável.",
            style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        if (!access) OutlinedButton(onClick = {
            c.startActivity(Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:${c.packageName}")))
        }) { Text("Permitir acesso aos ficheiros (pasta do jogo)") }
        OutlinedTextField(exe, { exe = it; PcGame.setExePath(c, it) }, Modifier.fillMaxWidth(),
            label = { Text("Executável do jogo") }, singleLine = true,
            supportingText = { Text(if (found) "Encontrado" else "Não encontrado") })
        OutlinedButton(onClick = { PcGame.findExe()?.let { exe = it; PcGame.setExePath(c, it) } }) { Text("Procurar nos cartões") }

        Text("Driver Vulkan", style = MaterialTheme.typography.titleMedium)
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            FilterChip(driver.isEmpty(), { driver = ""; PcGame.setDriverDir(c, "") }, { Text("Sistema") })
            DriverRepository.installed().forEach { d ->
                FilterChip(driver == d.dir.absolutePath, { driver = d.dir.absolutePath; PcGame.setDriverDir(c, driver) }, { Text(d.name) })
            }
        }
        Text("Resolução (o ecrã escala)", style = MaterialTheme.typography.titleMedium)
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf("" to "Nativa", "1920x1080" to "1080p", "1600x900" to "900p", "1280x720" to "720p", "960x540" to "540p", "854x480" to "480p").forEach { (v, l) ->
                FilterChip(size == v, { size = v; PcGame.setRenderSize(c, v) }, { Text(l) })
            }
        }
        OutlinedTextField(size, { size = it; PcGame.setRenderSize(c, it) }, Modifier.fillMaxWidth(),
            label = { Text("Resolução livre (LxA)") }, singleLine = true)
        if (found) GameGraphicOptions(exe)
        Text("Renderizador: DXVK (Direct3D 9 → Vulkan). OpenGL/gl4es não suporta o Direct3D 9 deste jogo.",
            style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Button(onClick = { c.startActivity(Intent(c, PcGameActivity::class.java)) }, enabled = found && access,
            modifier = Modifier.fillMaxWidth()) { Text("JOGAR (PC)") }
    }
}

/** Opcoes graficas do jogo de PC (as mesmas do menu OPTIONS > GRAPHIC OPTIONS), gravadas no GraphicOption do jogo. */
@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun GameGraphicOptions(exe: String) {
    var v by remember(exe) { mutableStateOf(PcGame.readGraphicOptions(exe)) }
    fun set(i: Int, x: Int) { v = v.copyOf().also { it[i] = x }; runCatching { PcGame.writeGraphicOptions(exe, v) } }
    Text("Opções gráficas do jogo", style = MaterialTheme.typography.titleMedium)
    Text("As mesmas do menu do jogo. Odin 2 Mini no perfil 480+, combate do prólogo: padrão 15,4 fps; «Desempenho» (= preset LOW do jogo) 16,8 fps.",
        style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
    @Composable fun row(label: String, i: Int, options: List<Pair<Int, String>>) {
        Text(label, style = MaterialTheme.typography.labelLarge)
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            options.forEach { (x, l) -> FilterChip(v[i] == x, { set(i, x) }, { Text(l) }) }
        }
    }
    FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedButton(onClick = { v = PcGame.GRAPHIC_DEFAULT.copyOf(); runCatching { PcGame.writeGraphicOptions(exe, v) } }) { Text("Padrão do jogo") }
        OutlinedButton(onClick = {
            // O preset LOW do jogo, medido: MSAA off, aniso 2x, blur off, sombra e zangeki medios, zantime baixo.
            v = v.copyOf().also { it[1] = 1; it[2] = 1; it[3] = 1; it[7] = 0; it[8] = 1; it[9] = 0 }
            runCatching { PcGame.writeGraphicOptions(exe, v) }
        }) { Text("Desempenho") }
    }
    row("Antialiasing", 7, listOf(0 to "Desligado", 1 to "MSAA x2"))
    row("Filtro de textura", 1, listOf(0 to "Desligado", 1 to "Anisotrópico 2x", 2 to "Anisotrópico 4x"))
    row("Blur", 2, listOf(0 to "Ligado", 1 to "Desligado"))
    row("Sombra", 3, listOf(0 to "Baixa", 1 to "Média"))
    row("Zangeki", 8, listOf(0 to "Baixo", 1 to "Médio"))
    row("Zantime", 9, listOf(0 to "Baixo", 1 to "Médio"))
    row("Janela", 6, listOf(0 to "Ecrã inteiro", 1 to "Janela"))
    row("Brilho", 4, (0..10).map { it to "$it" })
}

