# Zandatsu v1.2.2

Metal Gear Rising: Revengeance (Xbox 360) no Android · on Android · en Android.
Nenhum arquivo do jogo incluído · No game files included · No incluye archivos del juego.

**APK:** `Zandatsu-v1.2.2.apk` (1.2.2, versionCode 5)
**SHA-256:** `4439b2fc6c95f41b451819f453054bd90932f651d83e23a9de4d5a8373145c92`

---

## Português (Brasil)

Quem tem a v1.2, a v1.1 ou a v1.0 instala por cima e mantém saves, jogo e ajustes.

**Novo: CPUs ARMv8.0** (Snapdragon 625, 630, 636, 660, 665 e 835, Helio P60/P70 e outros só com Cortex-A53/A72/A73). Antes o
jogo fechava ao abrir nesses aparelhos. O APK é um só: ele traz o jogo compilado duas vezes e escolhe sozinho a versão
da sua CPU na primeira vez que abre. Quem tem CPU ARMv8.2 ou mais nova continua com a mesma versão de antes, sem perder
desempenho. Testado num Xiaomi Mi A3 (Snapdragon 665): abre e joga. Nesses chips o jogo é pesado: use o perfil Mínimo e
o Turnip, se o app sugerir.

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
- Teste de GPU: mede também a CPU (núcleos e uso por thread) e descarta janelas com pausa.
- Logs: a versão do driver Vulkan e, nos aparelhos sem ADPF, uma linha a dizer isso.

## English

If you have v1.2, v1.1 or v1.0, install over it: saves, game and settings stay.

**New: ARMv8.0 CPUs** (Snapdragon 625, 630, 636, 660, 665 and 835, Helio P60/P70 and others with only Cortex-A53/A72/A73 cores). Before,
the game closed at startup on these devices. There is still one APK: it carries the game built twice and picks the
version for your CPU the first time it opens. ARMv8.2 and newer CPUs keep the same build as before, with no performance
loss. Tested on a Xiaomi Mi A3 (Snapdragon 665): it starts and plays. The game is heavy on these chips: use the Minimum
profile, and Turnip if the app suggests it.

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
- GPU test: also measures the CPU (cores and per-thread use) and drops windows with a pause.
- Logs: the Vulkan driver version and, on devices without ADPF, a line saying so.

## Español

Si tienes la v1.2, v1.1 o v1.0, instala encima: se mantienen partidas, juego y ajustes.

**Nuevo: CPUs ARMv8.0** (Snapdragon 625, 630, 636, 660, 665 y 835, Helio P60/P70 y otros solo con Cortex-A53/A72/A73). Antes el
juego se cerraba al abrir en estos dispositivos. Sigue siendo un solo APK: trae el juego compilado dos veces y elige la
versión de tu CPU la primera vez que se abre. Las CPUs ARMv8.2 y más nuevas siguen con la misma versión de antes, sin
perder rendimiento. Probado en un Xiaomi Mi A3 (Snapdragon 665): abre y se juega. En estos chips el juego es pesado:
usa el perfil Mínimo, y Turnip si la app lo sugiere.

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
- Prueba de GPU: mide también la CPU (núcleos y uso por hilo) y descarta ventanas con pausa.
- Logs: la versión del driver Vulkan y, en dispositivos sin ADPF, una línea que lo dice.
