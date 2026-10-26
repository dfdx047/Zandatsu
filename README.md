<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/zandatsu-logo-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="docs/assets/zandatsu-logo-light.svg">
    <img alt="Zandatsu" src="docs/assets/zandatsu-logo-light.svg" width="600">
  </picture>
</p>

<h1 align="center">Zandatsu v1.0</h1>

<p align="center">
  <b>Metal Gear Rising: Revengeance</b> (Xbox 360) no Android · on Android · en Android
</p>

[Português (Brasil)](#português-brasil) · [English](#english) · [Español](#español)

> Nenhum arquivo do jogo está incluído. Projeto de fã, sem ligação com a Konami, a PlatinumGames ou a Microsoft.
> No game files are included. Fan project, not affiliated with Konami, PlatinumGames or Microsoft.
> No se incluye ningún archivo del juego. Proyecto de fans, sin relación con Konami, PlatinumGames ni Microsoft.

---

## Português (Brasil)

### O que é
O Zandatsu é um port de Metal Gear Rising: Revengeance para Android, por recompilação estática do executável do Xbox
360 ([ReXGlue SDK](https://github.com/rexglue/rexglue-sdk)). Não é um emulador do console inteiro: o código do jogo
corre nativo em ARM64. Tem dois renderizadores:

- **Nativo (Renut):** o Direct3D do jogo traduzido para Vulkan (do [mgr-nx-deko](https://github.com/tuancapoo/mgr-nx-deko)). Mais
  rápido e mais nítido. Adreno com o driver Turnip, ou Mali G57 e mais novas com o driver do sistema.
- **Emulado (Xenia):** a GPU do Xbox 360 emulada. Corre em mais aparelhos, mais lento.

O APK não traz o jogo: o código do jogo vai encriptado e só abre com o `default.xex` do seu próprio disco.

### Requisitos
- A sua ISO de **Metal Gear Rising: Revengeance (USA, Europe)** do Xbox 360 (Title ID `4B4E080A`).
- Android 11 ou mais novo, ARM64, Vulkan 1.3.
- Recomendado: Adreno 6xx com o driver **Turnip** (o app baixa e ativa na aba Drivers), ou Mali G57 e mais novas.
- ~8 GB livres para a ISO.

### Instalar e importar a ISO
1. Baixe `Zandatsu-v1.0.apk` da página de releases e instale.
2. Abra o app e escolha um perfil gráfico (Mínimo a Ultra; cada um mostra o fps medido).
3. Em **Início ▸ Arquivos do jogo**, toque em **Importar .iso** e escolha a ISO. O app confere o `default.xex` e lê
   a ISO no lugar, sem extrair (menos de 1 s). Não apague nem mova a ISO depois.
4. Adreno 6xx: na aba **Drivers**, baixe um Turnip e ative-o. Com o driver do sistema o jogo corre no modo emulado.
5. Toque em **JOGAR**.

Se já extraiu o jogo numa versão antiga, o botão **Apagar arquivos extraídos antigos** libera o espaço.

### Opções gráficas
- **Perfis:** Mínimo, Baixo, Médio, Alto, Ultra, ou Personalizado.
- **Renut:** resolução 360p, 540p, 720p (console), 1080p\*, 2K\* e 4K\* (\* = experimental); filtro final
  bilinear, AMD FSR 1 ou AMD CAS; sombras; brilho (bloom); anisotrópico até 16x.
- **Xenia:** resolução 360p a 4K, AMD FSR, FXAA, 30/60 FPS ou 120 FPS com interpolação, sombras, brilho.
- Tudo também no menu do jogo (botão Voltar), com o jogo a correr.

### Novidades desde a v1.0
- **Renut na Mali:** as texturas BC são descomprimidas na GPU. Nas Mali G57 e mais novas o Renut é o padrão; passou
  a regressão completa no RG Cube (título, menus, save, luta, game over, cutscene).
- **FSR mais leve:** o mesmo FSR 1 custa cerca de 4× menos GPU (meia precisão): no RP5 com a GPU no mínimo
  (305 MHz), de 18,7 para 4,6 ms por frame.
- **1080p no RP5:** de 44 para 47 fps na luta (continua experimental, ver os problemas conhecidos).
- **Rotação:** o jogo não fica mais em retrato quando o aparelho está com a rotação travada (#1).
- **Atualização:** o launcher não pede mais para atualizar sem parar (#1).

### Aparelhos testados
fps de jogo na luta do prólogo (26/10/2026).

| Aparelho | GPU | Modo | Resultado |
|---|---|---|---|
| Retroid Pocket 5 | Adreno 650 (Turnip) | Renut 720p bilinear | 57 fps (GPU 6,8 ms) |
| Retroid Pocket 5 | Adreno 650 (Turnip) | Renut 540p FSR | 56 fps |
| Retroid Pocket 5 | Adreno 650 (Turnip) | Renut 1080p\* | 47 fps |
| RG Cube | Mali-G57 | Renut 540p | 36–40 fps |
| RG Cube | Mali-G57 | Xenia, perfil Baixo (25/10) | 33–35 fps |
| AYN Thor | Adreno 740 (Turnip) | Renut 1080p FSR | 60 fps (24/10) |
| AYN Odin 3 | Adreno 830 (Turnip) | Renut 1440p (2K) FSR | 60 fps (24/10) |

Problemas conhecidos: [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md).

---

## English

### What it is
Zandatsu is a port of Metal Gear Rising: Revengeance to Android, made by statically recompiling the Xbox 360
executable ([ReXGlue SDK](https://github.com/rexglue/rexglue-sdk)). It is not a whole-console emulator: the game code
runs natively on ARM64. There are two renderers:

- **Native (Renut):** the game's Direct3D translated to Vulkan (from [mgr-nx-deko](https://github.com/tuancapoo/mgr-nx-deko)).
  Faster and sharper. Adreno with the Turnip driver, or Mali G57 and newer with the system driver.
- **Emulated (Xenia):** the Xbox 360 GPU, emulated. Runs on more devices, but slower.

The APK does not contain the game: the game code ships encrypted and only opens with the `default.xex` from your own
disc.

### Requirements
- Your own Xbox 360 ISO of **Metal Gear Rising: Revengeance (USA, Europe)** (Title ID `4B4E080A`).
- Android 11 or newer, ARM64, Vulkan 1.3.
- Recommended: Adreno 6xx with the **Turnip** driver (the app downloads and enables it in the Drivers tab), or Mali
  G57 and newer.
- ~8 GB free for the ISO.

### Install and import the ISO
1. Download `Zandatsu-v1.0.apk` from the releases page and install it.
2. Open the app and pick a graphics profile (Minimum to Ultra; each one shows the measured fps).
3. In **Home ▸ Game files**, tap **Import .iso** and pick the ISO. The app checks the `default.xex` and reads the ISO
   in place, without extracting (under 1 s). Don't delete or move the ISO afterwards.
4. Adreno 6xx: in the **Drivers** tab, download a Turnip build and enable it. With the system driver the game runs in
   emulated mode.
5. Tap **PLAY**.

If you extracted the game with an older version, the **Delete old extracted files** button frees that space.

### Graphics options
- **Profiles:** Minimum, Low, Medium, High, Ultra, or Custom.
- **Renut:** resolution 360p, 540p, 720p (console), 1080p\*, 2K\* and 4K\* (\* = experimental); final filter
  bilinear, AMD FSR 1 or AMD CAS; shadows; bloom; anisotropic up to 16x.
- **Xenia:** resolution 360p to 4K, AMD FSR, FXAA, 30/60 FPS or 120 FPS with interpolation, shadows, bloom.
- All of it is also in the in-game menu (Back button), while the game runs.

### What's new since v1.0
- **Renut on Mali:** BC textures are decompressed on the GPU. On Mali G57 and newer Renut is the default; it passed
  the full regression on the RG Cube (title, menus, save, combat, game over, cutscene).
- **Lighter FSR:** the same FSR 1 costs about 4× less GPU time (half precision): on the RP5 with the GPU at its
  minimum (305 MHz), from 18.7 to 4.6 ms a frame.
- **1080p on the RP5:** from 44 to 47 fps in combat (still experimental, see the known issues).
- **Rotation:** the game no longer ends up in portrait when the device's rotation is locked (#1).
- **Updates:** the launcher no longer asks to update over and over (#1).

### Tested devices
Game fps in the prologue fight (2026-10-26).

| Device | GPU | Mode | Result |
|---|---|---|---|
| Retroid Pocket 5 | Adreno 650 (Turnip) | Renut 720p bilinear | 57 fps (GPU 6.8 ms) |
| Retroid Pocket 5 | Adreno 650 (Turnip) | Renut 540p FSR | 56 fps |
| Retroid Pocket 5 | Adreno 650 (Turnip) | Renut 1080p\* | 47 fps |
| RG Cube | Mali-G57 | Renut 540p | 36–40 fps |
| RG Cube | Mali-G57 | Xenia, Low profile (2026-10-25) | 33–35 fps |
| AYN Thor | Adreno 740 (Turnip) | Renut 1080p FSR | 60 fps (2026-10-24) |
| AYN Odin 3 | Adreno 830 (Turnip) | Renut 1440p (2K) FSR | 60 fps (2026-10-24) |

Known issues: [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md).

---

## Español

### Qué es
Zandatsu es un port de Metal Gear Rising: Revengeance para Android, hecho por recompilación estática del ejecutable
de Xbox 360 ([ReXGlue SDK](https://github.com/rexglue/rexglue-sdk)). No es un emulador de toda la consola: el código
del juego corre nativo en ARM64. Tiene dos renderizadores:

- **Nativo (Renut):** el Direct3D del juego traducido a Vulkan (de [mgr-nx-deko](https://github.com/tuancapoo/mgr-nx-deko)).
  Más rápido y más nítido. Adreno con el driver Turnip, o Mali G57 y más nuevas con el driver del sistema.
- **Emulado (Xenia):** la GPU de Xbox 360 emulada. Funciona en más dispositivos, pero más lento.

El APK no trae el juego: el código del juego va cifrado y solo se abre con el `default.xex` de tu propio disco.

### Requisitos
- Tu propia ISO de Xbox 360 de **Metal Gear Rising: Revengeance (USA, Europe)** (Title ID `4B4E080A`).
- Android 11 o más reciente, ARM64, Vulkan 1.3.
- Recomendado: Adreno 6xx con el driver **Turnip** (la app lo descarga y lo activa en la pestaña Drivers), o Mali
  G57 y más nuevas.
- ~8 GB libres para la ISO.

### Instalar e importar la ISO
1. Descarga `Zandatsu-v1.0.apk` de la página de releases e instálalo.
2. Abre la app y elige un perfil gráfico (Mínimo a Ultra; cada uno muestra los fps medidos).
3. En **Inicio ▸ Archivos del juego**, toca **Importar .iso** y elige la ISO. La app comprueba el `default.xex` y lee
   la ISO en su sitio, sin extraer (menos de 1 s). No borres ni muevas la ISO después.
4. Adreno 6xx: en la pestaña **Drivers**, descarga un Turnip y actívalo. Con el driver del sistema el juego corre en
   modo emulado.
5. Toca **JUGAR**.

Si extrajiste el juego con una versión anterior, el botón **Borrar archivos extraídos antiguos** libera ese espacio.

### Opciones gráficas
- **Perfiles:** Mínimo, Bajo, Medio, Alto, Ultra o Personalizado.
- **Renut:** resolución 360p, 540p, 720p (consola), 1080p\*, 2K\* y 4K\* (\* = experimental); filtro final
  bilineal, AMD FSR 1 o AMD CAS; sombras; brillo (bloom); anisotrópico hasta 16x.
- **Xenia:** resolución de 360p a 4K, AMD FSR, FXAA, 30/60 FPS o 120 FPS con interpolación, sombras, brillo.
- Todo también está en el menú del juego (botón Atrás), con el juego en marcha.

### Novedades desde la v1.0
- **Renut en Mali:** las texturas BC se descomprimen en la GPU. En las Mali G57 y más nuevas Renut es el predeterminado;
  pasó la regresión completa en la RG Cube (título, menús, partida guardada, combate, game over, cinemática).
- **FSR más ligero:** el mismo FSR 1 cuesta unas 4 veces menos GPU (media precisión): en la RP5 con la GPU al mínimo
  (305 MHz), de 18,7 a 4,6 ms por fotograma.
- **1080p en la RP5:** de 44 a 47 fps en combate (sigue siendo experimental, ver los problemas conocidos).
- **Rotación:** el juego ya no queda en vertical cuando la rotación del dispositivo está bloqueada (#1).
- **Actualización:** el launcher ya no pide actualizar sin parar (#1).

### Dispositivos probados
fps de juego en el combate del prólogo (26/10/2026).

| Dispositivo | GPU | Modo | Resultado |
|---|---|---|---|
| Retroid Pocket 5 | Adreno 650 (Turnip) | Renut 720p bilineal | 57 fps (GPU 6,8 ms) |
| Retroid Pocket 5 | Adreno 650 (Turnip) | Renut 540p FSR | 56 fps |
| Retroid Pocket 5 | Adreno 650 (Turnip) | Renut 1080p\* | 47 fps |
| RG Cube | Mali-G57 | Renut 540p | 36–40 fps |
| RG Cube | Mali-G57 | Xenia, perfil Bajo (25/10) | 33–35 fps |
| AYN Thor | Adreno 740 (Turnip) | Renut 1080p FSR | 60 fps (24/10) |
| AYN Odin 3 | Adreno 830 (Turnip) | Renut 1440p (2K) FSR | 60 fps (24/10) |

Problemas conocidos: [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md).

---

## Créditos · Credits · Créditos

Lista completa: [CREDITS.md](CREDITS.md).

- [mgr-nx-deko](https://github.com/tuancapoo/mgr-nx-deko) (tuancapoo) e **reNut** (masterspike52): renderizador nativo /
  native renderer / renderizador nativo.
- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) (Tom Clay) e/and/y [Xenia](https://xenia.jp): recompilador,
  runtime, GPU emulada / recompiler, runtime, emulated GPU.
- [XenDroid](https://github.com/rfandango/XenDroid) e/and/y [XenDroid Plus](https://github.com/phforner0/Xendroid-Plus):
  correções de GPU e Turnip / GPU and Turnip fixes.
- [ARMSX2](https://github.com/ARMSX2/ARMSX2): banco de bugs de driver, cache de pipelines / driver bug database,
  pipeline cache.
- **Turnip / Mesa** (K11MCH1, Mr. Purple, StevenMXZ, ARMSX2) e/and/y [libadrenotools](https://github.com/bylaws/libadrenotools).
- AMD FidelityFX FSR 1 / CAS, plume, XenosRecomp.

## Licença · License · Licencia

[GPL-3.0](LICENSE). O código-fonte é aberto / The source code is open / El código fuente es abierto.
Como compilar / How to build / Cómo compilar: [docs/BUILDING.md](docs/BUILDING.md) (pt-BR).

Nenhum arquivo do jogo está incluído; é preciso ter o disco original. Sem ligação com a Konami.
No game files are included; you need your own original disc. Not affiliated with Konami.
No se incluye ningún archivo del juego; necesitas tu disco original. Sin relación con Konami.
