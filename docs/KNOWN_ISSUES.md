# Problemas conhecidos · Known issues · Problemas conocidos

Zandatsu v1.3.

## Português (Brasil)

1. **Vibração presa: corrigida na v1.2 (#8, #28).** O jogo deixava as rampas num valor baixo e ele era reenviado para sempre; agora a
   vibração termina em cerca de 1 s. Se ainda ficar presa, mande o log como no item 2. Para desligar: launcher ▸ **Controles ▸ Vibração**.
2. **Ultrawide (experimental, só Nativo).** As piscadas em 16:9 durante o jogo foram corrigidas na v1.3. Ainda há uma
   emenda a 4/5 da largura em alguns menus (escolher o save, pausa do jogo, MISSION FAILED). O HUD continua no layout
   16:9 encostado à esquerda e logos e vídeos ficam 16:9 com barras. Se algo piscar no seu aparelho, mande o log:
   1. **Sem fechar o jogo**, logo depois da piscada, toque em Voltar para abrir o menu do jogo.
   2. **Sistema ▸ Exportar logs.** O app grava `Zandatsu-logs-AAAAMMDD-HHMMSS.zip` na pasta **Download** do aparelho.
      (Se o jogo já fechou: launcher ▸ **Mais ▸ Sobre ▸ Exportar logs**, que leva o log da última sessão.)
   3. Anexe o zip numa [issue no GitHub](https://github.com/dfdx047/Zandatsu/issues) ou no canal de logs do
      [Discord](https://discord.gg/nXSANX9V6k), com o modelo do aparelho e a proporção da tela.
3. **Adreno 650 (Retroid Pocket 5) com o Turnip MrPurple: hang raro da GPU.** Cerca de 1 a cada 2–3 h de jogo, em
   qualquer resolução. Desde a v1.1 o jogo para de desenhar e volta ao launcher com "A GPU reiniciou" em vez de fechar
   com erro; basta tocar em **JOGAR**. O save é o do último checkpoint.
   Desde a v1.2, depois do primeiro hang o app liga sozinho um modo mais estável e avisa. Trocar de driver desfaz. A causa do hang segue
   desconhecida.
4. **Som: corrigido na v1.1.** Os sons dos inimigos e os golpes dos combos que sumiam nas lutas (#3, #5, #8, #9, #11)
   voltam a tocar: cada som XMA saía um frame mais curto e, uns 12 s depois do início da luta, não havia mais vozes
   livres. Se ainda faltar algum som, mande o log como no item 2.
5. **1080p, 2K e 4K continuam experimentais (`*`).** No RP5, 1080p dá 56 fps na luta. Para 60 estáveis no RP5, use
   720p ou 540p com FSR.
6. **Adreno 6xx com o driver do sistema: sem Renut.** O driver Qualcomm do Adreno 650 não tem `shaderInt64`; o jogo
   corre no modo emulado (Xenia), mais lento. O app avisa e indica o Turnip na aba Drivers.
7. **Adreno 700–729 com o driver do sistema: só o modo Xenia.** O driver do sistema fecha o jogo no Renut (#9); o app
   cai para o Xenia sozinho.
8. **Mali antes da G57 (G52, G72, G76): Renut experimental.** O padrão é o modo emulado (Xenia). Só a Mali-G57 (RG
   Cube) foi testada.
9. **Samsung Xclipse: Renut e drivers RADV nunca testados num aparelho.** Com o RADV o aparelho pode reiniciar se a
   GPU travar.
10. **A ISO não pode ser apagada nem movida** depois de importada: o jogo lê-a no lugar. Se a mover, importe de novo.
11. **Mali-G615 (#22, #29):** em alguns aparelhos o jogo fecha ao carregar o som. Causa não encontrada.
12. **Mali-G57 MC2, Helio G99/G100 (#16, #20, #25):** o nativo perde o dispositivo cerca de 4 s depois de abrir. O modo automático passa ao
    Emulado; escolher o nativo à mão continua possível. Causa não encontrada.
13. **Correções ainda sem teste nos aparelhos afetados:** crash ao cortar o Sundowner (#16, #18, #31), imagem cortada em Mali-G710/G720 (#13,
    #23) e fechamento com o driver do sistema em Snapdragon recentes. Se continuar, mande o log.
14. **Tamanho dos botões:** a partir de cerca de 150%, os botões na tela se sobrepõem.
15. **Mods do PC (Nexus Mods) não funcionam.** Os arquivos dos mods do PC estão no formato do PC (texturas DDS, dados
    little-endian); o jogo do Xbox 360 não os lê. Precisam de conversão, ainda não feita.
16. **PanVK na Mali-G615:** 24 a 30 fps e a GPU pode reiniciar (DEVICE_LOST). Use o driver do sistema (o padrão).
17. **DLC: só os trajes funcionam.** O jogo lê os trajes (White, Inferno, Commando e Ninja Armor, Cyborg Ninja, MGS4
    Raiden). Sam's Side Story, Wolf's Side Story e VR Mission Expansion instalam, mas o jogo não as usa: STORY não mostra
    os capítulos extra e VR MISSIONS continua com 20. Devem precisar de uma atualização do jogo (title update) que o
    Zandatsu ainda não aplica. Um pacote danificado aparece a vermelho na lista; remova e importe outra cópia.
18. **Otimização de CPU: travamento raro.** Em cerca de 1 de 25 cargas de luta (visto no Odin 2 Mini) o jogo parou ao
    entrar na luta com ela ligada. A launcher desliga-a sozinha depois disso; pode ligá-la de novo em Gráficos.

## English

1. **Stuck vibration: fixed in v1.2 (#8, #28).** The game left its ramps at a low value and it was resent forever; vibration now ends after
   about 1 s. If it still gets stuck, send the log as in item 2. To turn it off: launcher ▸ **Controls ▸ Vibration**.
2. **Ultrawide (experimental, Native only).** The 16:9 flashes during gameplay were fixed in v1.3. Some menus still
   show a seam at 4/5 of the width (save select, the game's pause, MISSION FAILED). The HUD stays in the 16:9 layout
   against the left edge, and logos and
   movies stay 16:9 with bars. If it flickers on your device, please send the log:
   1. **Without closing the game**, right after the flicker, press Back to open the game menu.
   2. **System ▸ Export logs.** The app saves `Zandatsu-logs-YYYYMMDD-HHMMSS.zip` to the device's **Download** folder.
      (If the game already closed: launcher ▸ **More ▸ About ▸ Export logs**, which includes the last session's log.)
   3. Attach the zip to a [GitHub issue](https://github.com/dfdx047/Zandatsu/issues) or the logs channel on
      [Discord](https://discord.gg/nXSANX9V6k), with your device model and screen aspect ratio.
3. **Adreno 650 (Retroid Pocket 5) with Turnip MrPurple: rare GPU hang.** About once every 2–3 h of play, at any
   resolution. Since v1.1 the game stops drawing and goes back to the launcher with "The GPU restarted" instead of
   closing with an error; just tap **PLAY**. You continue from the last checkpoint.
   Since v1.2, after the first hang the app turns on a more stable mode and tells you. Changing the driver undoes it. The cause of the hang is
   still unknown.
4. **Sound: fixed in v1.1.** Enemy sounds and combo hits that went missing in fights (#3, #5, #8, #9, #11) play again:
   every XMA sound came out one frame short and, about 12 s into a fight, no voices were left. If a sound is still
   missing, send the log as in item 2.
5. **1080p, 2K and 4K are still experimental (`*`).** On the RP5, 1080p runs at 56 fps in combat. For a steady 60 on
   the RP5, use 720p, or 540p with FSR.
6. **Adreno 6xx on the system driver: no Renut.** The Qualcomm driver for the Adreno 650 lacks `shaderInt64`; the game
   runs in emulated mode (Xenia), which is slower. The app warns you and names the Turnip build in the Drivers tab.
7. **Adreno 700–729 on the system driver: Xenia mode only.** The system driver closes the game on Renut (#9); the app
   falls back to Xenia by itself.
8. **Mali older than the G57 (G52, G72, G76): Renut is experimental.** The default is emulated mode (Xenia). Only the
   Mali-G57 (RG Cube) has been tested.
9. **Samsung Xclipse: Renut and the RADV drivers have never been tested on a device.** With RADV the device may reboot
   if the GPU hangs.
10. **The ISO can't be deleted or moved** after importing: the game reads it in place. If you move it, import again.
11. **Mali-G615 (#22, #29):** on some devices the game closes while loading sound. Cause not found.
12. **Mali-G57 MC2, Helio G99/G100 (#16, #20, #25):** Native loses the device about 4 s after opening. Automatic mode moves to Emulated; you
    can still choose Native by hand. Cause not found.
13. **Fixes not tested yet on the affected devices:** the crash when cutting Sundowner (#16, #18, #31), the cut-off picture on Mali-G710/G720
    (#13, #23) and closing on the system driver on recent Snapdragons. If it still happens, send the log.
14. **Button size:** from about 150%, the on-screen buttons overlap.
15. **PC mods (Nexus Mods) don't work.** PC mod files are in the PC format (DDS textures, little-endian data); the Xbox
    360 game can't read them. They need converting, which isn't done yet.
16. **PanVK on Mali-G615:** 24 to 30 fps and the GPU may reset (DEVICE_LOST). Use the system driver (the default).
17. **DLC: only the costumes work.** The game reads the costumes (White, Inferno, Commando and Ninja Armor, Cyborg
    Ninja, MGS4 Raiden). Sam's Side Story, Wolf's Side Story and VR Mission Expansion install, but the game doesn't use
    them: STORY shows no extra chapters and VR MISSIONS stays at 20. They likely need a game title update that Zandatsu
    doesn't apply yet. A damaged package shows in red in the list; remove it and import another copy.
18. **CPU optimization: rare freeze.** In about 1 of 25 fight loads (seen on the Odin 2 Mini) the game froze entering
    the fight with it on. The launcher turns it off by itself afterwards; you can turn it back on in Graphics.

## Español

1. **Vibración sin parar: corregida en la v1.2 (#8, #28).** El juego dejaba sus rampas en un valor bajo y se reenviaba para siempre; ahora la
   vibración termina en cerca de 1 s. Si sigue sin parar, envía el log como en el punto 2. Para desactivarla: launcher ▸ **Controles ▸ Vibración**.
2. **Ultrawide (experimental, solo Nativo).** Los parpadeos en 16:9 durante el juego se corrigieron en la v1.3. Algunos
   menús aún muestran una costura a 4/5 del ancho (elegir partida, la pausa del juego, MISSION FAILED). El HUD sigue en
   el diseño 16:9 pegado a la
   izquierda, y los logos y los vídeos quedan en 16:9 con barras. Si parpadea en tu dispositivo, envía el log:
   1. **Sin cerrar el juego**, justo después del parpadeo, pulsa Atrás para abrir el menú del juego.
   2. **Sistema ▸ Exportar logs.** La app guarda `Zandatsu-logs-AAAAMMDD-HHMMSS.zip` en la carpeta **Download** del
      dispositivo. (Si el juego ya se cerró: launcher ▸ **Más ▸ Acerca de ▸ Exportar logs**, que incluye el log de la última
      sesión.)
   3. Adjunta el zip en una [issue de GitHub](https://github.com/dfdx047/Zandatsu/issues) o en el canal de logs de
      [Discord](https://discord.gg/nXSANX9V6k), con el modelo del dispositivo y la proporción de la pantalla.
3. **Adreno 650 (Retroid Pocket 5) con Turnip MrPurple: cuelgue raro de la GPU.** Más o menos 1 cada 2–3 h de juego, a
   cualquier resolución. Desde la v1.1 el juego deja de dibujar y vuelve al launcher con "La GPU se reinició" en vez de
   cerrarse con error; basta con tocar **JUGAR**. Sigues desde el último punto de control.
   Desde la v1.2, después del primer cuelgue la app activa un modo más estable y avisa. Cambiar de driver lo deshace. La causa del cuelgue sigue
   sin conocerse.
4. **Sonido: corregido en la v1.1.** Los sonidos de los enemigos y los golpes de los combos que desaparecían en los
   combates (#3, #5, #8, #9, #11) vuelven a sonar: cada sonido XMA salía un frame más corto y, unos 12 s después de
   empezar el combate, ya no quedaban voces libres. Si aún falta algún sonido, envía el log como en el punto 2.
5. **1080p, 2K y 4K siguen siendo experimentales (`*`).** En la RP5, 1080p da 56 fps en combate. Para 60 estables en la
   RP5, usa 720p o 540p con FSR.
6. **Adreno 6xx con el driver del sistema: sin Renut.** El driver de Qualcomm del Adreno 650 no tiene `shaderInt64`; el
   juego corre en modo emulado (Xenia), más lento. La app avisa e indica el Turnip en la pestaña Drivers.
7. **Adreno 700–729 con el driver del sistema: solo modo Xenia.** El driver del sistema cierra el juego en Renut (#9);
   la app pasa a Xenia sola.
8. **Mali anteriores a la G57 (G52, G72, G76): Renut experimental.** El modo predeterminado es el emulado (Xenia). Solo
   se probó la Mali-G57 (RG Cube).
9. **Samsung Xclipse: Renut y los drivers RADV nunca se probaron en un dispositivo.** Con RADV el dispositivo puede
   reiniciarse si la GPU se cuelga.
10. **La ISO no se puede borrar ni mover** después de importarla: el juego la lee en su sitio. Si la mueves,
    impórtala de nuevo.
11. **Mali-G615 (#22, #29):** en algunos dispositivos el juego se cierra al cargar el sonido. Causa no encontrada.
12. **Mali-G57 MC2, Helio G99/G100 (#16, #20, #25):** el nativo pierde el dispositivo unos 4 s después de abrir. El modo automático pasa al
    Emulado; aún puedes elegir el nativo a mano. Causa no encontrada.
13. **Correcciones aún sin probar en los dispositivos afectados:** el cierre al cortar a Sundowner (#16, #18, #31), la imagen cortada en
    Mali-G710/G720 (#13, #23) y el cierre con el driver del sistema en Snapdragon recientes. Si sigue pasando, envía el log.
14. **Tamaño de los botones:** a partir de cerca de 150%, los botones en pantalla se superponen.
15. **Los mods de PC (Nexus Mods) no funcionan.** Los archivos de los mods de PC están en el formato de PC (texturas DDS,
    datos little-endian); el juego de Xbox 360 no los lee. Necesitan conversión, que aún no está hecha.
16. **PanVK en Mali-G615:** de 24 a 30 fps y la GPU puede reiniciarse (DEVICE_LOST). Usa el driver del sistema (el
    predeterminado).
17. **DLC: solo funcionan los trajes.** El juego lee los trajes (White, Inferno, Commando y Ninja Armor, Cyborg Ninja,
    MGS4 Raiden). Sam's Side Story, Wolf's Side Story y VR Mission Expansion se instalan, pero el juego no las usa: STORY
    no muestra los capítulos extra y VR MISSIONS sigue con 20. Probablemente necesitan una actualización del juego (title
    update) que Zandatsu aún no aplica. Un paquete dañado aparece en rojo en la lista; quítalo e importa otra copia.
18. **Optimización de CPU: cuelgue raro.** En cerca de 1 de cada 25 cargas de pelea (visto en el Odin 2 Mini) el juego
    se colgó al entrar en la pelea con ella activada. El launcher la desactiva solo después; puedes volver a activarla
    en Gráficos.
