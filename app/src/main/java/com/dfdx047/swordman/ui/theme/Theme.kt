package com.dfdx047.swordman.ui.theme

import android.os.Build
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.ColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp
import com.dfdx047.swordman.data.DarkMode
import com.dfdx047.swordman.data.ThemeMode

/**
 * Paleta do Project SwordMan, tirada do Metal Gear Rising: preto e grafite dos menus, o
 * vermelho do logotipo e do HUD, branco frio dos textos, o ciano frio do Blade Mode e o
 * laranja das faiscas. Minimalista: o vermelho so para o que importa (titulos, selecao, Jogar).
 * Os nomes antigos (Gold, Royal, Lavender...) ficam pelos ecras, com os tons novos.
 */
object DfColors {
    val Night = Color(0xFF0A0B0D)      // fundo
    val Panel = Color(0xFF16181C)      // cartoes
    val Royal = Color(0xFF2B3038)      // aco escuro (bordas, selecao)
    val Lavender = Color(0xFF7CC4E4)   // ciano frio (Blade Mode)
    val Gold = Color(0xFFE3263A)       // vermelho MGR (destaque principal)
    val Cream = Color(0xFFE4E7EB)      // branco frio
    val Ember = Color(0xFFFF6A3D)      // faiscas

    val Steel = Color(0xFF8C96A3)
    val Orange = Ember
    val OrangeDeep = Color(0xFFB81D2E)
    val Yellow = Gold
    val Blue = Lavender
    val BlueDeep = Royal
    val Navy = Night
    val StarRed = Gold
    val Ok = Color(0xFF3FBF8F)
    val Warn = Color(0xFFFFB020)
    val Error = Color(0xFFFF5A4E)
}

private val GameLight = lightColorScheme(
    primary = Color(0xFFB81D2E),
    onPrimary = Color.White,
    primaryContainer = Color(0xFFFFDAD9),
    onPrimaryContainer = Color(0xFF40000A),
    secondary = Color(0xFF3D5A6C),
    onSecondary = Color.White,
    secondaryContainer = Color(0xFFD3E6F3),
    onSecondaryContainer = Color(0xFF0B1E29),
    tertiary = Color(0xFFB2441D),
    onTertiary = Color.White,
    tertiaryContainer = Color(0xFFFFDBCF),
    onTertiaryContainer = Color(0xFF380D00),
    background = Color(0xFFF4F5F7),
    onBackground = Color(0xFF15171A),
    surface = Color(0xFFF4F5F7),
    onSurface = Color(0xFF15171A),
    surfaceVariant = Color(0xFFDFE2E7),
    onSurfaceVariant = Color(0xFF43474E),
    surfaceContainer = Color(0xFFEBEDF0),
    surfaceContainerHigh = Color(0xFFE3E6EA),
    outline = Color(0xFF73777F),
    outlineVariant = Color(0xFFC3C7CF),
    error = Color(0xFFBA1A1A),
)

private val GameDark = darkColorScheme(
    primary = DfColors.Gold,
    onPrimary = Color.White,
    primaryContainer = Color(0xFF5C0A14),
    onPrimaryContainer = Color(0xFFFFDAD9),
    secondary = DfColors.Lavender,
    onSecondary = Color(0xFF00202E),
    secondaryContainer = Color(0xFF1F2A33),
    onSecondaryContainer = Color(0xFFD3E6F3),
    tertiary = DfColors.Ember,
    onTertiary = Color(0xFF380D00),
    tertiaryContainer = Color(0xFF5A1E08),
    onTertiaryContainer = Color(0xFFFFDBCF),
    background = DfColors.Night,
    onBackground = DfColors.Cream,
    surface = DfColors.Night,
    onSurface = DfColors.Cream,
    surfaceVariant = Color(0xFF22262C),
    onSurfaceVariant = Color(0xFFB7BEC7),
    surfaceContainer = Color(0xFF121418),
    surfaceContainerHigh = DfColors.Panel,
    surfaceContainerHighest = Color(0xFF1F2328),
    outline = Color(0xFF6B737D),
    outlineVariant = Color(0xFF2E333A),
    error = DfColors.Error,
)

private val DfTypography = Typography().let { t ->
    t.copy(
        headlineMedium = t.headlineMedium.copy(fontWeight = FontWeight.Black),
        headlineSmall = t.headlineSmall.copy(fontWeight = FontWeight.ExtraBold),
        titleLarge = t.titleLarge.copy(fontWeight = FontWeight.ExtraBold),
        titleMedium = t.titleMedium.copy(fontWeight = FontWeight.Bold),
        labelLarge = t.labelLarge.copy(fontWeight = FontWeight.Bold, letterSpacing = 0.6.sp),
    )
}

/** Letras do logotipo: largas e espacadas, como os titulos dos menus do jogo. */
val LogoStyle = TextStyle(
    fontWeight = FontWeight.Black,
    fontSize = 26.sp,
    letterSpacing = 4.sp,
)

@Composable
fun SwordManTheme(themeMode: ThemeMode, darkMode: DarkMode, content: @Composable () -> Unit) {
    val dark = when (darkMode) {
        DarkMode.SYSTEM -> isSystemInDarkTheme()
        DarkMode.LIGHT -> false
        DarkMode.DARK -> true
    }
    val ctx = LocalContext.current
    val scheme: ColorScheme = if (themeMode == ThemeMode.MATERIAL_YOU && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        if (dark) dynamicDarkColorScheme(ctx) else dynamicLightColorScheme(ctx)
    } else {
        if (dark) GameDark else GameLight
    }
    MaterialTheme(colorScheme = scheme, typography = DfTypography, content = content)
}
