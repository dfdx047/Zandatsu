# Problemas conhecidos · Known issues · Problemas conocidos

Zandatsu v1.0 (24/10/2026).

## Português (Brasil)

1. **Retroid Pocket 5 / Adreno 650: 1080p ou mais, ou FSR, podem travar.** Em lutas de 2 min no RP5 (Turnip), 1080p,
   2K e 4K travaram a GPU (`kgsl: GPU hang detected`, "device lost"); o FSR travou uma vez aos 118 s. Não é falta de
   memória (sempre mais de 3,5 GB livres). **Use 720p sem FSR** (bilinear ou CAS). No menu, 1080p, 2K e 4K estão
   marcadas com `*` (experimental).
2. **Mali (ex.: RG Cube, Mali-G57): só o modo emulado (Xenia), com desempenho baixo.** O driver não tem texturas BC, por
   isso o renderizador nativo (Renut) não corre. No RG Cube: 36–42 fps nos perfis Mínimo/Baixo (30 FPS), 7–15 fps do
   Médio ao Ultra.
3. **DEVICE_LOST intermitente no Turnip.** A GPU pode travar de vez em quando também em 540p e 720p (visto 1 vez em
   cada, em ~15 corridas de 1–2 min). A imagem congela; feche e reabra o jogo.
4. **Adreno 6xx com o driver do sistema: sem Renut.** O driver Qualcomm do Adreno 650 não tem `shaderInt64`; o jogo
   corre no modo emulado (Xenia), mais lento. Baixe e ative o Turnip na aba Drivers (o app avisa).
5. **A ISO não pode ser apagada nem movida** depois de importada: o jogo lê-a no lugar. Se a mover, importe de novo.

## English

1. **Retroid Pocket 5 / Adreno 650: 1080p and above, or FSR, may hang.** In 2-minute fights on the RP5 (Turnip),
   1080p, 2K and 4K hung the GPU (`kgsl: GPU hang detected`, "device lost"); FSR hung once at 118 s. It is not memory
   (always more than 3.5 GB free). **Use 720p without FSR** (bilinear or CAS). The menu marks 1080p, 2K and 4K with
   `*` (experimental).
2. **Mali (e.g. RG Cube, Mali-G57): emulated mode (Xenia) only, low performance.** The driver has no BC textures, so
   the native renderer (Renut) can't run. On the RG Cube: 36–42 fps with the Minimum/Low profiles (30 FPS), 7–15 fps
   from Medium to Ultra.
3. **Intermittent DEVICE_LOST on Turnip.** The GPU can occasionally hang at 540p and 720p too (seen once each in ~15
   runs of 1–2 min). The picture freezes; close and reopen the game.
4. **Adreno 6xx with the system driver: no Renut.** The Qualcomm driver for the Adreno 650 lacks `shaderInt64`; the
   game runs in emulated mode (Xenia), which is slower. Download and enable Turnip in the Drivers tab (the app warns
   you).
5. **The ISO can't be deleted or moved** after importing: the game reads it in place. If you move it, import again.

## Español

1. **Retroid Pocket 5 / Adreno 650: 1080p o más, o FSR, pueden colgarse.** En combates de 2 min en la RP5 (Turnip),
   1080p, 2K y 4K colgaron la GPU (`kgsl: GPU hang detected`, "device lost"); FSR se colgó una vez a los 118 s. No es
   falta de memoria (siempre más de 3,5 GB libres). **Usa 720p sin FSR** (bilineal o CAS). El menú marca 1080p, 2K y
   4K con `*` (experimental).
2. **Mali (p. ej. RG Cube, Mali-G57): solo modo emulado (Xenia), rendimiento bajo.** El driver no tiene texturas BC,
   así que el renderizador nativo (Renut) no funciona. En la RG Cube: 36–42 fps con los perfiles Mínimo/Bajo (30 FPS),
   7–15 fps de Medio a Ultra.
3. **DEVICE_LOST intermitente en Turnip.** La GPU puede colgarse de vez en cuando también a 540p y 720p (visto una vez
   en cada una, en ~15 pruebas de 1–2 min). La imagen se congela; cierra y vuelve a abrir el juego.
4. **Adreno 6xx con el driver del sistema: sin Renut.** El driver de Qualcomm del Adreno 650 no tiene `shaderInt64`;
   el juego corre en modo emulado (Xenia), más lento. Descarga y activa Turnip en la pestaña Drivers (la app avisa).
5. **La ISO no se puede borrar ni mover** después de importarla: el juego la lee en su sitio. Si la mueves, impórtala
   de nuevo.
