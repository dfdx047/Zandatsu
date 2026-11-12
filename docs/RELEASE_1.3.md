# Zandatsu v1.3

Metal Gear Rising: Revengeance (Xbox 360) no Android · on Android · en Android.
Nenhum arquivo do jogo incluído · No game files included · No incluye archivos del juego.

**APK:** `Zandatsu-v1.3.apk` (1.3, versionCode 6)
**SHA-256:** `2eac7a1d5f0372cadb2ec44836710f8a1fc3a0179f4d6d03c19327d51a365566`

---

## Português (Brasil)

Quem tem a v1.2.2 (ou mais antiga) instala por cima e mantém saves, jogo e ajustes.

**Novo: menu do jogo refeito.** Abre como um painel pequeno do lado direito, com o jogo pausado, no mesmo tema da
launcher (escuro, claro ou Material You). São 4 abas: Pausa, Vídeo, Controles e Sistema, só com opções que valem na
hora, incluindo a predefinição gráfica (Mínimo a Ultra). Usa-se por toque ou pelo controle: LB/RB mudam de aba, a
cruz ou o analógico escolhem, A confirma, B fecha. O botão voltar dos consoles portáteis (Retroid, Odin e outros) abre
e fecha o menu.

**Novo: DLC do Xbox 360.** Botão **Importar DLC** no Início, ao lado de "Importar pasta". Escolha os pacotes do DLC
que você tem (os arquivos da pasta `Content/0000000000000000/4B4E080A/00000002` do console). O jogo instala na próxima
vez que abrir e a lista mostra cada um como instalado, com botão para remover. Por enquanto só os trajes funcionam no jogo (White, Inferno, Commando e Ninja Armor, Cyborg Ninja, MGS4 Raiden). Sam's
e Wolf's Side Story e a VR Mission Expansion instalam, mas ainda não aparecem no jogo. Um pacote danificado não fecha
mais o jogo: aparece em vermelho na lista, para remover e importar outra cópia.

**Novo: launcher reorganizada.** 5 abas com nome (Início, Gráficos, Controles, Drivers, Mais), barra lateral com o
aparelho deitado, Início com o resumo da configuração e a legenda dos controles em grade. Em pé, os textos e as opções
não ficam cortados nem sobrepostos.

**APK menor:** de 224 MB para 77 MB. Continua um APK só, com o jogo para CPUs ARMv8.2 e ARMv8.0, escolhido sozinho.

Correções
- Mali-G615 (Dimensity 8300/8350): céu preto no Nativo (#24, #35).
- Luta presa em 20 fps nos primeiros 40 a 90 segundos: o Android não recebia o tempo de GPU do Nativo e não subia o
  clock. No POCO X6 Pro: 50–55 fps desde o início (antes 20).
- Cutscenes e vídeos com solavancos em telas de 90/120 Hz: agora com ritmo regular (95% dos quadros no tempo certo,
  antes 69%).
- Ultrawide: piscadas em 16:9 durante o jogo e imagem repetida perto do A/B nos menus depois do Start.
- Pular as logos volta a funcionar no Nativo (tela de título em 9 s).
- Nativo com vsync desligado não fica mais preso em 30 fps.
- CPU: as threads do jogo só são acordadas quando há trabalho para elas.
- Proporção com nomes mais claros: **Widescreen 16:9** (a imagem original) e **Ultrawide** com a proporção real da sua
  tela (ex.: 20:9).

Testado no AYN Odin 2 Mini (Snapdragon 8 Gen 2, 1080p, Alto, Nativo): instala por cima da 1.2.2 mantendo jogo e ajustes;
59–60 fps na primeira luta com o driver do sistema e com o Turnip MrPurple T30 (GPU cerca de 30% mais rápida no Turnip),
sem nenhum crash em 6 carregamentos. Mesmo resultado que a 1.2.2 no mesmo aparelho.

## English

If you have v1.2.2 (or older), install over it: saves, game and settings stay.

**New: in-game menu redone.** It opens as a small panel on the right side, with the game paused, in the same theme as
the launcher (dark, light or Material You). There are 4 tabs: Pause, Video, Controls and System, with only the options
that apply right away, including the graphics preset (Minimum to Ultra). Use it by touch or with a controller: LB/RB
switch tabs, the d-pad or stick picks, A confirms, B closes. The back button of handhelds (Retroid, Odin and others)
opens and closes the menu.

**New: Xbox 360 DLC.** **Import DLC** button on Home, next to "Import folder". Pick the packages of the DLC you own
(the files in the console's `Content/0000000000000000/4B4E080A/00000002` folder). The game installs them the next time
it opens, and the list shows each one as installed, with a remove button. For now only the costumes work in the game (White, Inferno, Commando and Ninja Armor, Cyborg Ninja, MGS4 Raiden). Sam's
and Wolf's Side Story and the VR Mission Expansion install but don't show up in the game yet. A damaged package no
longer closes the game: it shows in red in the list, to remove and import another copy.

**New: reorganized launcher.** 5 named tabs (Home, Graphics, Controls, Drivers, More), a side bar in landscape, Home
with a summary of your setup and the controls legend as a grid. In portrait, text and options are no longer cut off or
overlapping.

**Smaller APK:** from 224 MB to 77 MB. Still one APK, with the game for ARMv8.2 and ARMv8.0 CPUs, picked on its own.

Fixes
- Mali-G615 (Dimensity 8300/8350): black sky on Native (#24, #35).
- Fights stuck at 20 fps for the first 40 to 90 seconds: Android was not getting Native's GPU time and didn't raise
  the clock. On the POCO X6 Pro: 50–55 fps from the start (was 20).
- Stuttering cutscenes and videos on 90/120 Hz screens: now evenly paced (95% of frames on time, was 69%).
- Ultrawide: 16:9 flashes during gameplay and a repeated image near A/B in the menus after Start.
- Skip logos works again on Native (title screen in 9 s).
- Native with vsync off is no longer stuck at 30 fps.
- CPU: game threads are only woken up when there is work for them.
- Clearer aspect names: **Widescreen 16:9** (the original image) and **Ultrawide** with your screen's real ratio
  (e.g. 20:9).

Tested on the AYN Odin 2 Mini (Snapdragon 8 Gen 2, 1080p, High, Native): installs over 1.2.2 keeping game and settings;
59–60 fps in the first fight on the system driver and on Turnip MrPurple T30 (GPU about 30% faster on Turnip), with no
crash in 6 loads. Same result as 1.2.2 on the same device.

## Español

Si tienes la v1.2.2 (o una anterior), instala encima: se mantienen partidas, juego y ajustes.

**Nuevo: menú del juego rehecho.** Se abre como un panel pequeño a la derecha, con el juego en pausa, con el mismo tema
del launcher (oscuro, claro o Material You). Tiene 4 pestañas: Pausa, Vídeo, Controles y Sistema, solo con opciones
que se aplican al momento, incluido el perfil gráfico (Mínimo a Ultra). Se usa con toque o con mando: LB/RB cambian de
pestaña, la cruceta o el stick eligen, A confirma, B cierra. El botón atrás de las consolas portátiles (Retroid, Odin y
otras) abre y cierra el menú.

**Nuevo: DLC de Xbox 360.** Botón **Importar DLC** en Inicio, junto a "Importar carpeta". Elige los paquetes de los DLC
que tienes (los archivos de la carpeta `Content/0000000000000000/4B4E080A/00000002` de la consola). El juego los instala
la próxima vez que se abre y la lista muestra cada uno como instalado, con botón para quitarlo. Por ahora solo funcionan los trajes en el juego (White, Inferno, Commando y Ninja Armor, Cyborg Ninja, MGS4 Raiden).
Sam's y Wolf's Side Story y la VR Mission Expansion se instalan, pero aún no aparecen en el juego. Un paquete dañado ya
no cierra el juego: aparece en rojo en la lista, para quitarlo e importar otra copia.

**Nuevo: launcher reorganizado.** 5 pestañas con nombre (Inicio, Gráficos, Controles, Drivers, Más), barra lateral en
horizontal, Inicio con el resumen de tu configuración y la leyenda de los controles en cuadrícula. En vertical, los
textos y las opciones ya no se cortan ni se superponen.

**APK más pequeño:** de 224 MB a 77 MB. Sigue siendo un solo APK, con el juego para CPUs ARMv8.2 y ARMv8.0, elegido
solo.

Correcciones
- Mali-G615 (Dimensity 8300/8350): cielo negro en Nativo (#24, #35).
- Peleas atascadas en 20 fps los primeros 40 a 90 segundos: Android no recibía el tiempo de GPU del Nativo y no subía
  el reloj. En el POCO X6 Pro: 50–55 fps desde el inicio (antes 20).
- Cinemáticas y vídeos a tirones en pantallas de 90/120 Hz: ahora con ritmo regular (95% de los cuadros a tiempo,
  antes 69%).
- Ultrawide: parpadeos en 16:9 durante el juego e imagen repetida cerca de A/B en los menús después de Start.
- Saltar los logos vuelve a funcionar en Nativo (pantalla de título en 9 s).
- Nativo con vsync desactivado ya no se queda en 30 fps.
- CPU: los hilos del juego solo se despiertan cuando hay trabajo para ellos.
- Nombres de proporción más claros: **Widescreen 16:9** (la imagen original) y **Ultrawide** con la proporción real de
  tu pantalla (p. ej. 20:9).

Probado en el AYN Odin 2 Mini (Snapdragon 8 Gen 2, 1080p, Alto, Nativo): se instala encima de la 1.2.2 manteniendo juego
y ajustes; 59–60 fps en la primera pelea con el driver del sistema y con Turnip MrPurple T30 (GPU cerca de un 30% más
rápida con Turnip), sin ningún crash en 6 cargas. Mismo resultado que la 1.2.2 en el mismo aparato.

## Problemas conhecidos · Known issues · Problemas conocidos

- Ultrawide: emenda a 4/5 da largura em alguns menus · a seam at 4/5 of the width in some menus · una costura a 4/5
  del ancho en algunos menús.
- DLC: só os trajes funcionam · only the costumes work · solo funcionan los trajes.
- Mods do PC ainda não funcionam · PC mods don't work yet · los mods de PC aún no funcionan.
- Adreno 650 + Turnip: hang raro da GPU · rare GPU hang · cuelgue raro de la GPU.

Lista completa · Full list · Lista completa: [docs/KNOWN_ISSUES.md](KNOWN_ISSUES.md)
