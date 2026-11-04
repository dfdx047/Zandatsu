# Zandatsu v1.2.1

Metal Gear Rising: Revengeance (Xbox 360) no Android · on Android · en Android.
Nenhum arquivo do jogo incluído · No game files included · No incluye archivos del juego.

**APK:** `Zandatsu-v1.2.1.apk` (1.2.1, versionCode 4)
**SHA-256:** `1cdfb33cf960f3cf95ce1242c7e6b930fddef2b71b0e04d927382017fe98a934`

---

## Português (Brasil)

Quem tem a v1.2, a v1.1 ou a v1.0 instala por cima e mantém saves, jogo e ajustes.

**Novo: teste de GPU** (Sistema ▸ Diagnóstico ▸ Rodar o teste de GPU). Cerca de 3 minutos (até 10 se o jogo não segura 30
fps): testa o driver de vídeo, os shaders do jogo, a CPU e o desempenho, e cria um arquivo zip. **Se o jogo tem problema no seu aparelho, rode o teste e mande o
arquivo** (botões Partilhar, Abrir issue no GitHub e Enviar no Discord). O zip só tem dados do aparelho, do Vulkan e
dos testes: nenhum save, foto ou conta. Em GPUs com problema conhecido (Mali-G615, G710, G720, G925, G57 MC2, PowerVR,
Adreno 7xx com o driver do sistema, Xclipse) o app sugere o teste ao abrir.

Correções
- Mali-G710/G720/G925 (Pixel 7, POCO X7 Pro, Dimensity 9000 a 9400): tela cortada no canto e minimapa na tela inteira
  no Nativo (#13, #23, #36, #37). Confirmado na POCO X7 Pro.
- Mali-G615 (Dimensity 8300/8350): crash a meio do jogo no Nativo (#17, #22, #24, #29). O app anota o shader que
  derruba o driver, passa a criar pipelines um de cada vez e, se cair 2 vezes no mesmo, salta esse. Pode fechar mais
  1 ou 2 vezes no mesmo lugar; depois deve passar.
- PowerVR BXM-8-256 (Dimensity 7025/7050): crash ao abrir o Nativo (#27). Os pipelines são criados um de cada vez. O
  primeiro carregamento pode demorar mais.
- Emulado: quedas para 0 fps ao entrar em luta ou numa área nova (#41). Os efeitos novos compilam em paralelo, sem
  parar o jogo. No RG Cube, com a cache apagada: engasgos de 37 s para 26 s no mesmo caminho, e o tempo parado a
  compilar de 8,6 s para 0,3 s.
- Controle na tela: o B some com o Blade Mode em Alterna (#45).
- Nova opção de resolução igual à da tela e aspecto automático pela proporção da tela.
- Mali: drivers PanVK oferecidos como opção. O driver do sistema continua o padrão.
- Desempenho: até 8 threads do jogo pedem prioridade ao Android (ADPF), reescolhidas a cada 30 s.
- Logs: a versão do driver Vulkan e, nos aparelhos sem ADPF, uma linha a dizer isso.

## English

If you have v1.2, v1.1 or v1.0, install over it: saves, game and settings stay.

**New: GPU test** (System ▸ Diagnostics ▸ Run GPU test). About 3 minutes (up to 10 if the game can't hold 30
fps): it checks the graphics driver, the game's shaders, the CPU and performance, and makes a zip file. **If the game has problems on your device, run the test and send the
file** (Share, Open issue on GitHub and Send on Discord buttons). The zip only has device, Vulkan and test data: no
saves, photos or accounts. On GPUs with known problems (Mali-G615, G710, G720, G925, G57 MC2, PowerVR, Adreno 7xx on the
system driver, Xclipse) the app suggests the test at startup.

Fixes
- Mali-G710/G720/G925 (Pixel 7, POCO X7 Pro, Dimensity 9000 to 9400): image cut to a corner and minimap over the whole
  screen on Native (#13, #23, #36, #37). Confirmed on the POCO X7 Pro.
- Mali-G615 (Dimensity 8300/8350): crash mid-game on Native (#17, #22, #24, #29). The app records the shader that
  crashes the driver, creates pipelines one at a time and, after 2 crashes on the same one, skips it. It may close once
  or twice more at the same spot; then it should go through.
- PowerVR BXM-8-256 (Dimensity 7025/7050): crash when Native starts (#27). Pipelines are created one at a time. The
  first load may take longer.
- Emulated: drops to 0 fps when a fight starts or in a new area (#41). New effects compile in the background without
  stopping the game. On the RG Cube with the cache cleared: hitches went from 37 s to 26 s on the same path, and time
  stalled compiling from 8.6 s to 0.3 s.
- Touch controls: B disappeared with Blade Mode set to Toggle (#45).
- New resolution option matching the screen, and automatic aspect from the screen's shape.
- Mali: PanVK drivers offered as an option. The system driver stays the default.
- Performance: up to 8 game threads ask Android for priority (ADPF), picked again every 30 s.
- Logs: the Vulkan driver version and, on devices without ADPF, a line saying so.

## Español

Si tienes la v1.2, v1.1 o v1.0, instala encima: se mantienen partidas, juego y ajustes.

**Nuevo: prueba de GPU** (Sistema ▸ Diagnóstico ▸ Ejecutar la prueba de GPU). Unos 3 minutos (hasta 10 si el juego no mantiene 30
fps): prueba el driver de vídeo, los shaders del juego, la CPU y el rendimiento, y crea un archivo zip. **Si el juego tiene problemas en tu dispositivo,
ejecuta la prueba y envía el archivo** (botones Compartir, Abrir issue en GitHub y Enviar por Discord). El zip solo
tiene datos del dispositivo, de Vulkan y de las pruebas: ni partidas, ni fotos, ni cuentas. En GPUs con problemas
conocidos (Mali-G615, G710, G720, G925, G57 MC2, PowerVR, Adreno 7xx con el driver del sistema, Xclipse) la app sugiere
la prueba al abrir.

Correcciones
- Mali-G710/G720/G925 (Pixel 7, POCO X7 Pro, Dimensity 9000 a 9400): imagen cortada en una esquina y minimapa en toda
  la pantalla en Nativo (#13, #23, #36, #37). Confirmado en el POCO X7 Pro.
- Mali-G615 (Dimensity 8300/8350): cierre a mitad del juego en Nativo (#17, #22, #24, #29). La app anota el shader que
  tumba el driver, crea los pipelines uno a la vez y, tras 2 cierres en el mismo, lo salta. Puede cerrarse 1 o 2 veces
  más en el mismo lugar; después debería pasar.
- PowerVR BXM-8-256 (Dimensity 7025/7050): cierre al abrir Nativo (#27). Los pipelines se crean uno a la vez. La
  primera carga puede tardar más.
- Emulado: caídas a 0 fps al empezar una pelea o en una zona nueva (#41). Los efectos nuevos se compilan en segundo
  plano sin detener el juego. En el RG Cube, con la caché borrada: tirones de 37 s a 26 s en el mismo recorrido, y el
  tiempo parado compilando de 8,6 s a 0,3 s.
- Controles táctiles: B desaparecía con el Blade Mode en Alternar (#45).
- Nueva opción de resolución igual a la de la pantalla y aspecto automático según la forma de la pantalla.
- Mali: drivers PanVK ofrecidos como opción. El driver del sistema sigue siendo el predeterminado.
- Rendimiento: hasta 8 hilos del juego piden prioridad a Android (ADPF), elegidos de nuevo cada 30 s.
- Logs: la versión del driver Vulkan y, en dispositivos sin ADPF, una línea que lo dice.
