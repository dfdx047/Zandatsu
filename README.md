# Project SwordMan

Port para Android de **Metal Gear Rising: Revengeance** (Xbox 360), recompilado estaticamente com o
[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) v0.10.0 e com a GPU emulada (Vulkan, derivada
do Xenia). Usa a mesma base do Dragon Fury (DBZ3-Android): o overlay do SDK e os patches Android
0001–0048, e o método de otimização do [OptXbox](https://github.com/dfdx047/Optxbox).

O repositório não tem o jogo nem o código gerado. É preciso ter o disco.

## Estrutura

| Caminho | Conteúdo |
|---|---|
| `mgr_manifest.toml` | Manifest do ReXGlue (XEX de entrada, pasta do código gerado) |
| `mgr_overrides.toml` | Correções do codegen: `[functions]`, `[[switch_tables]]`, `[[midasm_hook]]` |
| `src/` | `main.cpp` e `mgr_app.h` (o mesmo código no PC e no Android) |
| `generated/default/` | Código recompilado (fora do git, `tools/codegen.sh`) |
| `assets/` | Jogo extraído da ISO: `default.xex` + `media/` (fora do git) |
| `sdk/overlay/` | Overlay do SDK do projeto DBZ3 HD Collection (MIT, `sdk/overlay-LICENSE`) |
| `patches/` | Patches Android do SDK (0001–0048 do Dragon Fury; os do MGR começam no 0100) |
| `external/rexglue-sdk-0.10/` | SDK preparado para o Android (fora do git, `tools/setup-sdk.sh`) |
| `app/` | App Android: launcher, `GameActivity` (SDL), build nativo `libmain.so` |
| `tools/` | `versions.env`, `setup-sdk.sh`, `apply-sdk-patches.sh`, `codegen.sh`, `push-game.sh` |
| `docs/` | `cvars-sem-otimizacoes.txt` (primeiro teste) |

## Versões fixadas

- ReXGlue SDK `v0.10.0` (`f5337cd`).
- `default.xex` do disco *Metal Gear Rising - Revengeance (USA, Europe)* (Redump): SHA-1
  `2dffd4d46e727d4a64c331d54251c4d9a0a26edf`, Title ID `4B4E080A`, versão 0.0.0.4.
- Overlay do SDK: DBZ-Budokai-3-HD-Collection `a0ceaeb`.

## Compilar

Ferramentas no PC (CachyOS): clang 20+, cmake 3.25+, ninja, Android Studio com o **NDK
28.2.13676358**, e o `rexglue` v0.10.0 no PATH (o SDK compilado em `../rexglue-sdk`).

1. Extrair o jogo para `assets/`:

   ```fish
   extract-xiso -x "/caminho/Metal Gear Rising - Revengeance (USA, Europe) (En,Ja,Fr,De,Es,It,Pt).iso" -d assets
   ```

2. Gerar o código recompilado:

   ```fish
   bash tools/codegen.sh
   ```

3. Preparar o SDK para o Android (copia `../rexglue-sdk` se existir, senão clona) e aplicar os patches:

   ```fish
   bash tools/setup-sdk.sh
   bash tools/apply-sdk-patches.sh
   ```

4. Em `local.properties` (o Android Studio cria o ficheiro), usar o CMake do sistema:

   ```
   cmake.dir=/usr
   ```

5. Abrir a pasta `ProjectSwordMan` no Android Studio e compilar a variante **release**
   (Build ▸ Select Build Variant ▸ `release`), ou na linha de comandos:

   ```fish
   ./gradlew assembleRelease
   adb install -r app/build/outputs/apk/release/app-release.apk
   ```

   `-Psm.uiOnly=true` compila só o launcher (rápido, sem o jogo).

## Primeiro teste no aparelho

1. Abrir o app uma vez (cria a pasta `Android/data/com.dfdx047.swordman`).
2. Copiar o jogo (~8 GB):

   ```fish
   bash tools/push-game.sh
   ```

3. Desligar as otimizações de GPU para a primeira rodada (primeiro a imagem certa):

   ```fish
   adb push docs/cvars-sem-otimizacoes.txt /sdcard/Android/data/com.dfdx047.swordman/files/cvars.txt
   ```

4. Jogar, voltar ao launcher e tocar em **Exportar logs** (`Download/SwordMan-logs-*.zip`).
5. No PC:

   ```fish
   cd ~/Optxbox && python3 -m optxbox report --adb --adb-pattern "SwordMan-logs-*.zip"
   ```

Captura de 2 frames durante o jogo:

```fish
adb shell 'echo 2 > /sdcard/Android/data/com.dfdx047.swordman/files/capture.req'
```

A captura fica em `logs/capture/*.dfc` e entra no próximo **Exportar logs**.

## Build no PC (para depurar mais rápido)

```fish
cmake --preset linux-amd64-release
cmake --build out/build/linux-amd64-release
./out/build/linux-amd64-release/mgr --game_data_root=assets
```

## Avisos

Projeto de fã, sem ligação com a Konami, a PlatinumGames ou a Microsoft. Nenhum dado do jogo
protegido por direitos autorais é distribuído.

## Licença e créditos

Desde 23/10/2026 o Project SwordMan é GPL-3.0 (`LICENSE`), com o código-fonte aberto. O renderizador nativo e as
ferramentas vêm do [mgr-nx-deko](https://github.com/tuancapoo/mgr-nx-deko) (GPL-3.0), com a autorização dos autores
(23/10/2026) para usar à vontade mantendo os créditos e o código aberto. O overlay do SDK continua MIT
(`sdk/overlay-LICENSE`) e o ReXGlue SDK BSD-3-Clause. Créditos completos: [CREDITS.md](CREDITS.md).
