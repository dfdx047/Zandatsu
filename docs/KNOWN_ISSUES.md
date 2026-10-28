# Problemas conhecidos · Known issues · Problemas conocidos

Zandatsu v1.1 (28/10/2026).

## Português (Brasil)

1. **Vibração ainda instável.** A vibração do controle às vezes falha ou fica presa; a causa ainda não foi
   encontrada. Para desligar: launcher ▸ **Controles ▸ Vibração**.
2. **Widescreen: pequenas piscadas em alguns aparelhos (experimental, só Renut).** A cena fica certa (hor+), mas em
   alguns aparelhos a imagem pisca de vez em quando. O HUD continua no layout 16:9 encostado à esquerda e logos e
   vídeos ficam 16:9 com barras. Se piscar no seu aparelho, mande o log:
   1. **Sem fechar o jogo**, logo depois da piscada, toque em Voltar para abrir o menu do jogo.
   2. **Sistema ▸ Exportar logs.** O app grava `Zandatsu-logs-AAAAMMDD-HHMMSS.zip` na pasta **Download** do aparelho.
      (Se o jogo já fechou: launcher ▸ **Sobre ▸ Exportar logs**, que leva o log da última sessão.)
   3. Anexe o zip numa [issue no GitHub](https://github.com/dfdx047/Zandatsu/issues) ou no canal de logs do
      [Discord](https://discord.gg/nXSANX9V6k), com o modelo do aparelho e a proporção da tela.
3. **Adreno 650 (Retroid Pocket 5) com o Turnip MrPurple: hang raro da GPU.** Cerca de 1 a cada 2–3 h de jogo, em
   qualquer resolução. Desde a v1.1 o jogo para de desenhar e volta ao launcher com "A GPU reiniciou" em vez de fechar
   com erro; basta tocar em **JOGAR**. O save é o do último checkpoint.
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

## English

1. **Vibration is still unstable.** Controller vibration sometimes fails or gets stuck; the cause has not been found
   yet. To turn it off: launcher ▸ **Controls ▸ Vibration**.
2. **Widescreen: brief flickers on some devices (experimental, Renut only).** The scene is right (hor+), but on some
   devices the picture flickers now and then. The HUD stays in the 16:9 layout against the left edge, and logos and
   movies stay 16:9 with bars. If it flickers on your device, please send the log:
   1. **Without closing the game**, right after the flicker, press Back to open the game menu.
   2. **System ▸ Export logs.** The app saves `Zandatsu-logs-YYYYMMDD-HHMMSS.zip` to the device's **Download** folder.
      (If the game already closed: launcher ▸ **About ▸ Export logs**, which includes the last session's log.)
   3. Attach the zip to a [GitHub issue](https://github.com/dfdx047/Zandatsu/issues) or the logs channel on
      [Discord](https://discord.gg/nXSANX9V6k), with your device model and screen aspect ratio.
3. **Adreno 650 (Retroid Pocket 5) with Turnip MrPurple: rare GPU hang.** About once every 2–3 h of play, at any
   resolution. Since v1.1 the game stops drawing and goes back to the launcher with "The GPU restarted" instead of
   closing with an error; just tap **PLAY**. You continue from the last checkpoint.
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

## Español

1. **La vibración aún es inestable.** La vibración del mando a veces falla o se queda pegada; todavía no se encontró la
   causa. Para desactivarla: launcher ▸ **Controles ▸ Vibración**.
2. **Panorámica: pequeños parpadeos en algunos dispositivos (experimental, solo Renut).** La escena queda bien (hor+),
   pero en algunos dispositivos la imagen parpadea de vez en cuando. El HUD sigue en el diseño 16:9 pegado a la
   izquierda, y los logos y los vídeos quedan en 16:9 con barras. Si parpadea en tu dispositivo, envía el log:
   1. **Sin cerrar el juego**, justo después del parpadeo, pulsa Atrás para abrir el menú del juego.
   2. **Sistema ▸ Exportar logs.** La app guarda `Zandatsu-logs-AAAAMMDD-HHMMSS.zip` en la carpeta **Download** del
      dispositivo. (Si el juego ya se cerró: launcher ▸ **Acerca de ▸ Exportar logs**, que incluye el log de la última
      sesión.)
   3. Adjunta el zip en una [issue de GitHub](https://github.com/dfdx047/Zandatsu/issues) o en el canal de logs de
      [Discord](https://discord.gg/nXSANX9V6k), con el modelo del dispositivo y la proporción de la pantalla.
3. **Adreno 650 (Retroid Pocket 5) con Turnip MrPurple: cuelgue raro de la GPU.** Más o menos 1 cada 2–3 h de juego, a
   cualquier resolución. Desde la v1.1 el juego deja de dibujar y vuelve al launcher con "La GPU se reinició" en vez de
   cerrarse con error; basta con tocar **JUGAR**. Sigues desde el último punto de control.
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
