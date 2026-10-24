# Zandatsu

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
| `app/` | App Android: launcher, `GameActivity` (SDL), `libmain.so` (leitor do XEX) e `libgame.so` (o jogo, só encriptado no APK) |
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

5. Abrir a pasta `Zandatsu` no Android Studio e compilar a variante **release**
   (Build ▸ Select Build Variant ▸ `release`), ou na linha de comandos:

   ```fish
   ./gradlew assembleRelease
   adb install -r app/build/outputs/apk/release/app-release.apk
   ```

   `-Psm.uiOnly=true` compila só o launcher (rápido, sem o jogo).

## Primeiro teste no aparelho

1. Abrir o app uma vez (cria a pasta `Android/data/com.dfdx047.zandatsu`).
2. Copiar o jogo (~8 GB):

   ```fish
   bash tools/push-game.sh
   ```

3. Desligar as otimizações de GPU para a primeira rodada (primeiro a imagem certa):

   ```fish
   adb push docs/cvars-sem-otimizacoes.txt /sdcard/Android/data/com.dfdx047.zandatsu/files/cvars.txt
   ```

4. Jogar, voltar ao launcher e tocar em **Exportar logs** (`Download/Zandatsu-logs-*.zip`).
5. No PC:

   ```fish
   cd ~/Optxbox && python3 -m optxbox report --adb --adb-pattern "Zandatsu-logs-*.zip"
   ```

Captura de 2 frames durante o jogo:

```fish
adb shell 'echo 2 > /sdcard/Android/data/com.dfdx047.zandatsu/files/capture.req'
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

### O APK não traz o jogo

O código recompilado, os hooks, o Renut e os shaders traduzidos ficam no `libgame.so`, que só entra no APK
encriptado (`assets/libgame.zdat`, AES-256-GCM, `tools/pack-libgame.py`). A chave é o SHA-256 da imagem PE do
`default.xex` (desencriptada e descomprimida por `src/xex_image.cpp`), por isso só existe no aparelho de quem
importa o próprio disco. No fim do import a app lê o `default.xex` do jogador, confere o SHA-256 da chave
(`assets/libgame.json`), desencripta o `libgame.so` para `getFilesDir()` e o `GameActivity` carrega-o de lá. Outro
executável dá "versão do jogo não suportada" com o title ID e o SHA-1 esperados. O libmain.so do APK é só o leitor
do XEX.

### A ISO é lida no lugar

"Importar .iso" não extrai nada: confere o `default.xex` da ISO (SHA-1), copia só ele (7 MB) e desencripta o
libgame (0,4 s no RP5; antes copiava 7,4 GB em 49 s). A app guarda a permissão persistente da ISO
(`IsoSource`, `game/.zandatsu-iso`); o `GameActivity` abre-a a cada arranque e passa o descritor ao nativo
(`ZANDATSU_ISO_FD`, `src/mgr_app.h`), que a monta como disco (`DiscImageDevice`, patch 0179 do SDK). A ISO não
pode ser apagada nem movida. Sem permissão persistente do fornecedor extrai-se como antes; "Importar pasta"
continua a copiar. Teste: `bash tools/import-iso-test.sh <serial> <nome da iso>`.

Antes de publicar um APK: `python3 tools/check-clean.py <apk> <libgame.so em claro> assets/default.xex`. Procura
nomes do código gerado e milhares de pedaços do código do jogo e da imagem PE no APK, no repositório e no
histórico do git.

## Licença e créditos

Desde 23/10/2026 o Zandatsu é GPL-3.0 (`LICENSE`), com o código-fonte aberto. O renderizador nativo e as
ferramentas vêm do [mgr-nx-deko](https://github.com/tuancapoo/mgr-nx-deko) (GPL-3.0), com a autorização dos autores
(23/10/2026) para usar à vontade mantendo os créditos e o código aberto. O overlay do SDK continua MIT
(`sdk/overlay-LICENSE`) e o ReXGlue SDK BSD-3-Clause. Créditos completos: [CREDITS.md](CREDITS.md).
