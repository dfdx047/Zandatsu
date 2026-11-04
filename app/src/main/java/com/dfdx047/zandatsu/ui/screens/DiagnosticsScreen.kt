package com.dfdx047.zandatsu.ui.screens

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.BugReport
import androidx.compose.material.icons.filled.Cancel
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Forum
import androidx.compose.material.icons.filled.HourglassEmpty
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material.icons.filled.RemoveCircleOutline
import androidx.compose.material.icons.filled.Share
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.core.content.FileProvider
import com.dfdx047.zandatsu.R
import com.dfdx047.zandatsu.SwordManApp
import com.dfdx047.zandatsu.diag.DiagRunner
import com.dfdx047.zandatsu.diag.DiagRunner.Status
import com.dfdx047.zandatsu.ui.LauncherViewModel
import com.dfdx047.zandatsu.ui.components.SectionCard
import org.json.JSONObject
import java.io.File
import java.net.URLEncoder

/** Configuracoes > Diagnostico. */
@Composable
fun DiagnosticsSection(vm: LauncherViewModel) {
    val context = LocalContext.current
    SectionCard(stringResource(R.string.diag_section), Icons.Filled.Memory, subtitle = stringResource(R.string.diag_section_sub)) {
        FilledTonalButton(onClick = { vm.showDiag = true }) {
            Icon(Icons.Filled.Memory, null)
            Spacer(Modifier.width(6.dp))
            Text(stringResource(R.string.diag_run))
        }
    }
}

/** O aviso de arranque (so nas GPUs com problema conhecido). */
@Composable
fun DiagPromptDialog(vm: LauncherViewModel, gpu: String) {
    val context = LocalContext.current
    AlertDialog(
        onDismissRequest = { vm.diagPromptAnswer(run = false, never = false, context = context) },
        icon = { Icon(Icons.Filled.Memory, null) },
        title = { Text(stringResource(R.string.diag_prompt_title)) },
        text = { Text(stringResource(R.string.diag_prompt_text, gpu)) },
        confirmButton = {
            Button(onClick = { vm.diagPromptAnswer(run = true, never = false, context = context) }) {
                Text(stringResource(R.string.diag_prompt_now))
            }
        },
        dismissButton = {
            Row {
                TextButton(onClick = { vm.diagPromptAnswer(run = false, never = true, context = context) }) {
                    Text(stringResource(R.string.diag_prompt_never))
                }
                TextButton(onClick = { vm.diagPromptAnswer(run = false, never = false, context = context) }) {
                    Text(stringResource(R.string.diag_prompt_later))
                }
            }
        },
    )
}

private fun testLabel(context: Context, id: String): String {
    val res = context.resources.getIdentifier("diag_t_$id", "string", context.packageName)
    return if (res != 0) context.getString(res) else id
}

@Composable
private fun statusText(s: Status): String = stringResource(
    when (s) {
        Status.PENDING -> R.string.diag_st_pending
        Status.RUNNING -> R.string.diag_st_running
        Status.PASS -> R.string.diag_st_pass
        Status.FAIL -> R.string.diag_st_fail
        Status.CRASH -> R.string.diag_st_crash
        Status.TIMEOUT -> R.string.diag_st_timeout
        Status.SKIP -> R.string.diag_st_skip
    },
)

/** O teste de GPU em tela cheia: o que vai no zip, o progresso e, no fim, passou/falhou e os tres botoes. */
@Composable
fun DiagnosticsScreen(vm: LauncherViewModel) {
    val context = LocalContext.current
    val st by DiagRunner.state.collectAsState()
    Dialog(
        onDismissRequest = { if (!st.running) vm.showDiag = false },
        properties = DialogProperties(usePlatformDefaultWidth = false, dismissOnClickOutside = false),
    ) {
        Surface(Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
            Column(
                Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                Text(stringResource(R.string.diag_title), style = MaterialTheme.typography.headlineSmall)
                Text(DiagRunner.gpu().name, style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.primary)
                val started = st.running || st.items.isNotEmpty()
                if (!started) {
                    Text(stringResource(R.string.diag_intro), style = MaterialTheme.typography.bodyMedium)
                }
                if (st.zip == null) {
                    SectionCard(stringResource(R.string.diag_contents_title)) {
                        Text(stringResource(R.string.diag_contents), style = MaterialTheme.typography.bodySmall)
                    }
                }
                if (st.items.isNotEmpty()) {
                    SectionCard(stringResource(R.string.diag_title)) {
                        st.items.forEach { TestRow(context, it) }
                    }
                }
                if (st.running) {
                    Text(stringResource(R.string.diag_running), color = MaterialTheme.colorScheme.tertiary)
                }
                st.zip?.let { zip ->
                    val pass = st.items.count { it.status == Status.PASS }
                    val crash = st.items.count { it.status == Status.CRASH || it.status == Status.TIMEOUT }
                    val fail = st.items.count { it.status == Status.FAIL }
                    Text(stringResource(R.string.diag_done, pass, fail, crash), style = MaterialTheme.typography.titleMedium)
                    Text(stringResource(R.string.diag_file, zip.name, (zip.length() / 1024).toInt()), style = MaterialTheme.typography.bodySmall)
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(onClick = { share(context, zip) }, Modifier.weight(1f)) {
                            Icon(Icons.Filled.Share, null); Spacer(Modifier.width(6.dp)); Text(stringResource(R.string.diag_share))
                        }
                    }
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        FilledTonalButton(onClick = { openIssue(context, zip, st.summary) }, Modifier.weight(1f)) {
                            Icon(Icons.Filled.BugReport, null); Spacer(Modifier.width(6.dp)); Text(stringResource(R.string.diag_issue))
                        }
                        FilledTonalButton(onClick = { open(context, SwordManApp.DISCORD_URL) }, Modifier.weight(1f)) {
                            Icon(Icons.Filled.Forum, null); Spacer(Modifier.width(6.dp)); Text(stringResource(R.string.diag_discord))
                        }
                    }
                    Text(stringResource(R.string.diag_issue_hint), style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                    SectionCard(stringResource(R.string.diag_files)) {
                        st.zipEntries.forEach { (name, size) ->
                            Text("$name  ${if (size >= 1024) "${size / 1024} KB" else "$size B"}", style = MaterialTheme.typography.bodySmall)
                        }
                    }
                }
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                    if (!st.running) {
                        OutlinedButton(onClick = { vm.showDiag = false }) { Text(stringResource(R.string.diag_close)) }
                        Spacer(Modifier.width(8.dp))
                        Button(onClick = { vm.startDiag(context) }) { Text(stringResource(if (st.zip == null) R.string.diag_start else R.string.diag_run)) }
                    }
                }
            }
        }
    }
}

@Composable
private fun TestRow(context: Context, item: DiagRunner.Item) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        when (item.status) {
            Status.RUNNING -> CircularProgressIndicator(Modifier.size(18.dp), strokeWidth = 2.dp)
            Status.PASS -> Icon(Icons.Filled.CheckCircle, null, Modifier.size(18.dp), tint = Color(0xFF4CAF50))
            Status.FAIL -> Icon(Icons.Filled.Cancel, null, Modifier.size(18.dp), tint = MaterialTheme.colorScheme.error)
            Status.CRASH, Status.TIMEOUT -> Icon(Icons.Filled.Warning, null, Modifier.size(18.dp), tint = Color(0xFFFF9800))
            Status.SKIP -> Icon(Icons.Filled.RemoveCircleOutline, null, Modifier.size(18.dp))
            Status.PENDING -> Icon(Icons.Filled.HourglassEmpty, null, Modifier.size(18.dp), tint = MaterialTheme.colorScheme.outline)
        }
        Spacer(Modifier.width(10.dp))
        Column(Modifier.weight(1f)) {
            Text(testLabel(context, item.id), style = MaterialTheme.typography.bodyMedium)
            val detail = listOfNotNull(
                item.crashedAt?.let { stringResource(R.string.diag_crashed_at, it) },
                item.message.takeIf { it.isNotEmpty() && item.crashedAt == null },
            ).joinToString(" · ")
            if (detail.isNotEmpty()) {
                Text(detail, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 2)
            }
        }
        Text(statusText(item.status), style = MaterialTheme.typography.labelMedium)
    }
}

private fun share(context: Context, zip: File) {
    val uri = FileProvider.getUriForFile(context, context.packageName + ".files", zip)
    val send = Intent(Intent.ACTION_SEND).setType("application/zip").putExtra(Intent.EXTRA_STREAM, uri)
        .putExtra(Intent.EXTRA_SUBJECT, zip.name).addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
    context.startActivity(Intent.createChooser(send, context.getString(R.string.diag_share)))
}

private fun open(context: Context, url: String) {
    try {
        context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
    } catch (_: Exception) {
    }
}

/** Issue nova com o titulo e o corpo preenchidos (a GPU, o aparelho e o resumo); o zip anexa-se a mao. */
private fun openIssue(context: Context, zip: File, s: JSONObject?) {
    val gpu = s?.optJSONObject("gpu")
    val dev = s?.optJSONObject("device")
    val vk = s?.optJSONObject("vulkan")
    val title = context.getString(R.string.diag_issue_title, gpu?.optString("name") ?: "?",
        "${dev?.optString("manufacturer")} ${dev?.optString("model")}".trim())
    val body = buildString {
        appendLine(context.getString(R.string.diag_issue_attach, zip.name))
        appendLine()
        appendLine("- App: ${s?.optJSONObject("app")?.optString("versionName")}")
        appendLine("- Device: ${dev?.optString("manufacturer")} ${dev?.optString("model")} (${dev?.optString("soc")}), Android ${dev?.optString("android")}")
        appendLine("- GPU: ${gpu?.optString("name")} (${gpu?.optString("family")})")
        appendLine("- Driver: ${s?.optJSONObject("driver")?.optString("active")} ${vk?.optString("driverName").orEmpty()} ${vk?.optString("driverInfo").orEmpty()}".trimEnd())
        appendLine("- Vulkan: ${vk?.optString("apiVersion").orEmpty()}  BC: ${vk?.opt("textureCompressionBC")}  ASTC: ${vk?.opt("textureCompressionASTC_LDR")}")
        appendLine()
        appendLine("| test | result | note |")
        appendLine("|---|---|---|")
        val tests = s?.optJSONArray("tests")
        for (i in 0 until (tests?.length() ?: 0)) {
            val t = tests!!.getJSONObject(i)
            val note = (t.optString("crashedAt").ifEmpty { t.optString("message") }).replace("|", "/").take(90)
            appendLine("| ${t.optString("id")} | ${t.optString("status")} | $note |")
        }
        s?.optJSONObject("perf")?.let { p ->
            appendLine("| perf | ${p.optString("status")} | ${p.optString("message").replace("|", "/").take(90)} |")
        }
    }
    val url = "https://github.com/dfdx047/Zandatsu/issues/new?title=" + URLEncoder.encode(title, "UTF-8") +
        "&body=" + URLEncoder.encode(body.take(5000), "UTF-8")
    open(context, url)
}
