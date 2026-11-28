package com.dfdx047.zandatsu.ui.screens

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.EmojiEvents
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Visibility
import androidx.compose.material.icons.filled.VisibilityOff
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.dfdx047.zandatsu.R
import com.dfdx047.zandatsu.data.AchievementItem
import com.dfdx047.zandatsu.ui.LauncherViewModel
import com.dfdx047.zandatsu.ui.components.SectionCard
import com.dfdx047.zandatsu.ui.theme.DfColors

private enum class AchFilter { ALL, UNLOCKED, LOCKED }

/** Tela de Conquistas (issue #91): lista, progresso em Gamerscore, filtros e datas de desbloqueio. */
@Composable
fun AchievementsScreen(vm: LauncherViewModel) {
    var filter by remember { mutableStateOf(AchFilter.ALL) }
    var revealSecrets by remember { mutableStateOf(false) }

    LaunchedEffect(Unit) {
        vm.refreshAchievements()
    }

    val summary = vm.achievements
    val loading = vm.achievementsLoading

    val visible = remember(summary, filter) {
        val all = summary?.items.orEmpty()
        when (filter) {
            AchFilter.ALL -> all.sortedWith(compareByDescending<AchievementItem> { it.unlocked }.thenBy { it.id })
            AchFilter.UNLOCKED -> all.filter { it.unlocked }.sortedByDescending { it.unlockedAtMs ?: 0L }
            AchFilter.LOCKED -> all.filter { !it.unlocked }.sortedBy { it.id }
        }
    }

    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        item(key = "header") {
            SectionCard(
                title = stringResource(R.string.tab_achievements),
                icon = Icons.Filled.EmojiEvents,
                subtitle = if (summary != null && summary.totalCount > 0) {
                    stringResource(
                        R.string.achievements_progress_sub,
                        summary.unlockedCount,
                        summary.totalCount,
                        summary.unlockedScore,
                        summary.totalScore,
                    )
                } else {
                    stringResource(R.string.more_achievements_sub)
                },
            ) {
                if (loading && summary == null) {
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                    Text(
                        stringResource(R.string.achievements_loading),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                } else if (summary == null || !summary.gameInstalled) {
                    Text(
                        stringResource(R.string.achievements_no_game),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                } else if (summary.totalCount == 0) {
                    Text(
                        stringResource(R.string.achievements_empty),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                } else {
                    val ratio = summary.unlockedCount.toFloat() / summary.totalCount.coerceAtLeast(1)
                    LinearProgressIndicator(
                        progress = { ratio.coerceIn(0f, 1f) },
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(8.dp)
                            .clip(RoundedCornerShape(4.dp)),
                    )
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        FilterChip(
                            selected = filter == AchFilter.ALL,
                            onClick = { filter = AchFilter.ALL },
                            label = { Text(stringResource(R.string.achievements_filter_all, summary.totalCount)) },
                        )
                        FilterChip(
                            selected = filter == AchFilter.UNLOCKED,
                            onClick = { filter = AchFilter.UNLOCKED },
                            label = { Text(stringResource(R.string.achievements_filter_unlocked, summary.unlockedCount)) },
                        )
                        FilterChip(
                            selected = filter == AchFilter.LOCKED,
                            onClick = { filter = AchFilter.LOCKED },
                            label = { Text(stringResource(R.string.achievements_filter_locked, summary.totalCount - summary.unlockedCount)) },
                        )
                        Spacer(Modifier.weight(1f))
                        IconButton(onClick = { revealSecrets = !revealSecrets }) {
                            Icon(
                                if (revealSecrets) Icons.Filled.Visibility else Icons.Filled.VisibilityOff,
                                contentDescription = stringResource(R.string.achievements_toggle_secrets),
                            )
                        }
                        IconButton(onClick = { vm.refreshAchievements() }) {
                            Icon(Icons.Filled.Refresh, contentDescription = stringResource(R.string.saves_refresh))
                        }
                    }
                }
            }
        }

        if (summary != null && summary.totalCount > 0 && visible.isEmpty()) {
            item(key = "filtered_empty") {
                Text(
                    stringResource(R.string.achievements_filter_none),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(vertical = 12.dp, horizontal = 4.dp),
                )
            }
        }

        items(visible, key = { it.id }) { item ->
            AchievementCard(item = item, revealSecrets = revealSecrets)
        }
    }
}

@Composable
private fun AchievementCard(item: AchievementItem, revealSecrets: Boolean) {
    val hidden = item.isSecret && !item.unlocked && !revealSecrets
    val title = if (hidden) stringResource(R.string.achievements_secret_title) else item.label
    val body = when {
        hidden -> stringResource(R.string.achievements_secret_desc)
        item.unlocked -> item.description.ifBlank { item.unachieved }
        else -> item.unachieved.ifBlank { item.description }
    }
    val borderColor = if (item.unlocked) DfColors.Gold.copy(alpha = 0.7f) else MaterialTheme.colorScheme.outlineVariant
    val container = if (item.unlocked) MaterialTheme.colorScheme.surfaceContainerHigh else MaterialTheme.colorScheme.surfaceContainer

    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(14.dp),
        colors = CardDefaults.cardColors(containerColor = container),
        border = BorderStroke(1.dp, borderColor),
    ) {
        Row(
            modifier = Modifier.padding(14.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Box(
                modifier = Modifier
                    .size(42.dp)
                    .background(
                        if (item.unlocked) DfColors.Gold.copy(alpha = 0.2f) else MaterialTheme.colorScheme.surfaceVariant,
                        CircleShape,
                    ),
                contentAlignment = Alignment.Center,
            ) {
                Icon(
                    imageVector = if (item.unlocked) Icons.Filled.EmojiEvents else Icons.Filled.Lock,
                    contentDescription = null,
                    tint = if (item.unlocked) DfColors.Gold else MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.size(22.dp),
                )
            }
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(
                        text = title,
                        style = MaterialTheme.typography.titleSmall,
                        fontWeight = if (item.unlocked) FontWeight.SemiBold else FontWeight.Medium,
                        maxLines = 2,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.weight(1f),
                    )
                    if (item.gamerscore > 0) {
                        Text(
                            text = "${item.gamerscore}G",
                            style = MaterialTheme.typography.labelMedium,
                            color = if (item.unlocked) DfColors.Gold else MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier
                                .background(MaterialTheme.colorScheme.surfaceVariant, RoundedCornerShape(8.dp))
                                .padding(horizontal = 8.dp, vertical = 2.dp),
                        )
                    }
                }
                if (body.isNotBlank()) {
                    Text(
                        text = body,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                if (item.unlocked) {
                    val date = item.formattedDate()
                    Text(
                        text = if (date != null) {
                            stringResource(R.string.achievements_unlocked_at, date)
                        } else {
                            stringResource(R.string.achievements_unlocked_badge)
                        },
                        style = MaterialTheme.typography.labelSmall,
                        color = DfColors.Gold,
                    )
                }
            }
        }
    }
}
