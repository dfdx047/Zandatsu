# Problemas conhecidos · Known issues · Problemas conocidos

Zandatsu v1.0 e branch de desenvolvimento (26/10/2026).

## Português (Brasil)

1. **Adreno 650 (Retroid Pocket 5) com Turnip: travamentos curtos da GPU e, às vezes, o jogo fecha.** O hardware
   detecta um travamento da GPU (`kgsl: GPU hang detected`) uma a três vezes a cada ~3 min de luta, em qualquer
   resolução, com ou sem sombras e FSR. Na maioria das vezes é só um engasgo (um frame de ~40 ms). Às vezes, logo a
   seguir, o driver Turnip fecha o jogo (visto 3 vezes em 26/10) ou a imagem congela. Feche e reabra o jogo. A causa
   ainda está a ser investigada. Teste de drivers no RP5 (26/10, 15 min de luta cada, 720p): Turnip MrPurple T19 =
   60 fps e 2 hangs; driver do sistema = 0 hangs, mas sem Renut (modo emulado, 32 fps). O Turnip continua o
   recomendado. O Turnip do ARMSX2 não foi testado.
2. **1080p, 2K e 4K continuam experimentais (`*`).** No RP5, 1080p dá 47 fps na luta (abaixo de 60) e sofre os mesmos
   travamentos do item 1. Para jogar, use 720p ou 540p com FSR.
3. **Mali antes da G57 (G52, G72, G76): Renut experimental.** O padrão é o modo emulado (Xenia); o Renut pode ser
   escolhido à mão. Só a Mali-G57 (RG Cube) foi testada.
4. **Samsung Xclipse: Renut e drivers RADV nunca testados num aparelho.** Os dois aparecem como experimentais. Com o
   RADV o aparelho pode reiniciar se a GPU travar.
5. **Adreno 6xx com o driver do sistema: sem Renut.** O driver Qualcomm do Adreno 650 não tem `shaderInt64`; o jogo
   corre no modo emulado (Xenia), mais lento. Baixe e ative o Turnip na aba Drivers (o app mostra um aviso grande
   com o driver recomendado e um atalho para a aba). O driver Qualcomm
   v615.77 (issue #6) importa, mas trava a GPU em ~36 s.
6. **O som some (#3, #5): ainda não reproduzido.** Se acontecer com você, use **Exportar logs** logo depois de sair do
   jogo e mande o zip na issue.
7. **A ISO não pode ser apagada nem movida** depois de importada: o jogo lê-a no lugar. Se a mover, importe de novo.
8. **Widescreen (experimental, só Renut).** A cena fica certa (hor+), mas o HUD continua no layout 16:9 encostado à
   esquerda (o mapa fica a meio da tela) e o título 3D tem o texto à esquerda; logos e vídeos ficam 16:9 com barras.
   No RP5 o fecho do jogo pelo Turnip (item 1) apareceu mais vezes em widescreen (4 em ~14 arranques).

## English

1. **Adreno 650 (Retroid Pocket 5) with Turnip: short GPU hangs, and sometimes the game closes.** The hardware detects
   a GPU hang (`kgsl: GPU hang detected`) one to three times every ~3 minutes of combat, at any resolution, with or
   without shadows and FSR. Most of the time it is just a hitch (one ~40 ms frame). Sometimes, right after one, the
   Turnip driver closes the game (seen 3 times on 2026-10-26) or the picture freezes. Close and reopen the game. The
   cause is still being investigated. Driver test on the RP5 (2026-10-26, 15 min of combat each, 720p): Turnip
   MrPurple T19 = 60 fps and 2 hangs; system driver = 0 hangs, but no Renut (emulated mode, 32 fps). Turnip stays
   the recommended driver. The ARMSX2 Turnip was not tested.
2. **1080p, 2K and 4K are still experimental (`*`).** On the RP5, 1080p runs at 47 fps in combat (below 60) and has
   the same hangs as item 1. To play, use 720p, or 540p with FSR.
3. **Mali older than the G57 (G52, G72, G76): Renut is experimental.** The default is emulated mode (Xenia); you can
   pick Renut by hand. Only the Mali-G57 (RG Cube) has been tested.
4. **Samsung Xclipse: Renut and the RADV drivers have never been tested on a device.** Both are shown as
   experimental. With RADV the device may reboot if the GPU hangs.
5. **Adreno 6xx with the system driver: no Renut.** The Qualcomm driver for the Adreno 650 lacks `shaderInt64`; the
   game runs in emulated mode (Xenia), which is slower. Download and enable Turnip in the Drivers tab (the app warns
   you with a large notice, the recommended driver and a shortcut to the tab). The Qualcomm v615.77 driver
   (issue #6) imports, but hangs the GPU after ~36 s.
6. **Sound disappears (#3, #5): not reproduced yet.** If it happens to you, tap **Export logs** right after leaving
   the game and attach the zip to the issue.
7. **The ISO can't be deleted or moved** after importing: the game reads it in place. If you move it, import again.
8. **Widescreen (experimental, Renut only).** The scene is right (hor+), but the HUD stays in the 16:9 layout against
   the left edge (the map ends up mid-screen) and the 3D title has its text on the left; logos and videos stay 16:9
   with bars. On the RP5 the Turnip crash (item 1) showed up more often in widescreen (4 in ~14 launches).

## Español

1. **Adreno 650 (Retroid Pocket 5) con Turnip: cuelgues cortos de la GPU y, a veces, el juego se cierra.** El hardware
   detecta un cuelgue de la GPU (`kgsl: GPU hang detected`) de una a tres veces cada ~3 min de combate, a cualquier
   resolución, con o sin sombras y FSR. Casi siempre es solo un tirón (un fotograma de ~40 ms). A veces, justo después,
   el driver Turnip cierra el juego (visto 3 veces el 26/10) o la imagen se congela. Cierra y vuelve a abrir el juego.
   La causa todavía se está investigando. Prueba de drivers en la RP5 (26/10, 15 min de combate cada uno, 720p):
   Turnip MrPurple T19 = 60 fps y 2 cuelgues; driver del sistema = 0 cuelgues, pero sin Renut (modo emulado, 32 fps).
   Turnip sigue siendo el recomendado. El Turnip de ARMSX2 no se probó.
2. **1080p, 2K y 4K siguen siendo experimentales (`*`).** En la RP5, 1080p da 47 fps en combate (por debajo de 60) y
   tiene los mismos cuelgues del punto 1. Para jugar, usa 720p o 540p con FSR.
3. **Mali anteriores a la G57 (G52, G72, G76): Renut experimental.** El modo predeterminado es el emulado (Xenia); Renut
   se puede elegir a mano. Solo se probó la Mali-G57 (RG Cube).
4. **Samsung Xclipse: Renut y los drivers RADV nunca se probaron en un dispositivo.** Los dos aparecen como
   experimentales. Con RADV el dispositivo puede reiniciarse si la GPU se cuelga.
5. **Adreno 6xx con el driver del sistema: sin Renut.** El driver de Qualcomm del Adreno 650 no tiene `shaderInt64`;
   el juego corre en modo emulado (Xenia), más lento. Descarga y activa Turnip en la pestaña Drivers (la app muestra un
   aviso grande con el driver recomendado y un acceso directo a la pestaña).
   El driver de Qualcomm v615.77 (issue #6) se importa, pero cuelga la GPU a los ~36 s.
6. **El sonido desaparece (#3, #5): aún no reproducido.** Si te pasa, toca **Exportar logs** justo después de
   salir del juego y adjunta el zip en la issue.
7. **La ISO no se puede borrar ni mover** después de importarla: el juego la lee en su sitio. Si la mueves, impórtala
   de nuevo.
8. **Panorámica (experimental, solo Renut).** La escena queda bien (hor+), pero el HUD sigue en el diseño 16:9 pegado
   a la izquierda (el mapa queda en medio de la pantalla) y el título 3D tiene el texto a la izquierda; los logos y
   los vídeos quedan en 16:9 con barras. En la RP5 el cierre del juego por Turnip (punto 1) apareció más veces en
   panorámica (4 en ~14 arranques).
