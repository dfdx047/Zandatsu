# Renut no Android: renderizador nativo do mgr-nx-deko

Objetivo: trocar a emulação da GPU do Xbox (Xenia: EDRAM, resolves, cópias) pelo renderizador nativo de
Direct3D do [mgr-nx-deko](https://github.com/tuancapoo/mgr-nx-deko) (reNut + plume, Vulkan), como opção
escolhida no launcher. O Xenia fica como alternativa. Análise completa, com ficheiros e linhas:
[renut-port-analysis.md](renut-port-analysis.md).

- Upstream: `third_party/mgr-nx-deko` (submódulo, commit `ba1a837`), GPL-3.0. Mesma `default.xex` (USA/Europe)
  e mesma base ReXGlue v0.10.0.
- **Licença:** o núcleo reNut não tem licença no projeto original (ver `THIRD_PARTY_NOTICES.md` deles). Testar
  pode; **distribuir só com a permissão do autor (masterspike52) e avisando o tuancapoo.**
- **Os shaders traduzidos são dados do jogo:** nunca no git (`out/renut/` está no `.gitignore`).

## Marcos

### M1: compila para arm64, Renut escolhido por cvar (2–4 dias)
1. Shaders no PC (Linux): `tools/renut-shaders.sh` (novo, chamado pelo `build.sh` com carimbo):
   `media/data000.cpk` → `cpk_toc.py` + `cpk_shader_scan.cpp` (`extract_mgr_cpk_shaders.py`) → XenosRecomp do
   host + DXC Linux → `out/renut/shader_cache.cpp`. Entra no build por `banjo_renut_shader_cache_override()`.
2. `cmake/renut.cmake` (novo, a partir de `app/cmake/mgr_renut_gpu.cmake` deles): biblioteca OBJECT ligada ao
   `libmain.so` (os hooks substituem os `sub_X` fracos); sem deko3d/NVK/uam/SPIRV-Cross/LTO; sem X11
   (`gpu.cmake:330-334`); sem o ImGui próprio (usar o ramo do Switch, `gpu.cmake:312-323`); XenosRecomp nunca
   compilado com o NDK (`gpu.cmake:152-169`).
3. Guardas nos ficheiros de diagnóstico do SA2 (`sys_counters.h`, `diet_ops.h`, `switch_saltynx.h`,
   `sa2_render_mode.cpp:15`); `rex::input::SetReadHook` desligado (late input) ou patch pequeno no SDK.
4. `MgrApp`: cvar `swordman_renderer = xenos|renut` (padrão `xenos`); no modo renut não forçar
   `gpu_plugin = xenos` e chamar o `ConfigureRuntime` do Renut; padrão do `sa2_renderer` deles = o nosso.
5. Verificar os 29 endereços dos hooks no nosso `generated/default` (laço de grep na análise, secção 3); o que
   faltar vai para o `mgr_overrides.toml`.

**Estado (23/10, feito):** `PULL=0 bash tools/build.sh none` passa com o Renut no `libmain.so` (APK 133 MB).
- Shaders: `tools/renut-shaders.sh` (carimbo em `out/renut/.stamp`, ~32 s do zero) compila o XenosRecomp e o
  `cpk_shader_scan` com o compilador do PC (`tools/renut-host/CMakeLists.txt`, em `out/host/build`) e usa o DXC
  oficial Linux v1.9.2609 (baixado para `out/host/dxc`, sha256 fixo). Resultado: 1202 contentores → 1202 shaders
  (SPIR-V 846 KB comprimido) em `out/renut/shader_cache.cpp` + 12 shaders do host em `out/renut/host_shaders/`.
  Sem a passagem de early alpha.
- Alterações ao submódulo: `patches/renut/NNNN-*.patch`, aplicados por `tools/apply-renut-patches.sh` (o
  `build.sh` chama-o; o CMake recusa configurar sem eles). 0001 `swordman_renderer`, 0002 `ANativeWindow` no
  `native_window.cpp`, 0003 sem `std::atomic_ref` (o libc++ do NDK 28 não tem), 0004 late input só com o read hook
  do SDK (G1: o nosso não tem, fica desligado), 0005 sem o overlay ImGui próprio do Renut.
- Os ficheiros de diagnóstico do SA2 (G3–G6: `sa2_enhancements.cpp`, `sa2_codegen_checks.cpp`,
  `sa2_forms_selfcheck.cpp`, `sa2_render_mode.cpp`, `sa2_capture.cpp`) não são compilados: o pouco que o Renut chama
  está em `src/renut_glue.cpp` (mestres de melhorias ligados, `Docked() = false`, altura por
  `sa2_render_height_handheld` = 720, sem captura em lockstep), junto com a cvar `swordman_renderer` e os hooks
  partilhados (Swap, DrawVertices, DrawIndexedVertices).
- Endereços: os 24 hooks ficam fortes (`T`) no `libmain.so` e os 3 chamados (`826C0C48`, `826C0278`, `82715A90`)
  existem no código gerado. Nada a acrescentar ao `mgr_overrides.toml`.
- `MgrApp` (`src/mgr_app.h`): com `swordman_renderer = renut`, `ConfigureRuntime` no `OnPreSetup`, `StartRenderer`
  no `OnFinalizePaths` (se falhar: linha no log e segue com o Xenia), drawer do ImGui pelo plume, resize, shutdown.
- Ainda não testado no aparelho (é o M2).

### M2: abre até ao título (≈1 semana)
1. `native_window.cpp`: ramo `__ANDROID__` com `SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER` (como o nosso
   `window_sdl.cpp:466-474`); sem o `setenv(SDL_VIDEO_DRIVER=x11)`.
2. plume: `volkInitializeCustom` com o driver do adrenotools (Turnip).
3. O swap do Renut (`sub_826B3510`) chama o nosso `rex_vdswap_hook` (pausa, watchdog, contagem de frames).
4. Verificação de funções Vulkan (buffer device address, int64, descriptor indexing com 8192 texturas, BC1–BC5,
   D32S8): se faltar, volta ao Xenia com uma linha no log.
5. Superfície perdida ao ir para segundo plano e rotação (`preTransform`).

**Estado (23/10, feito no RP5 com Turnip v27.0.0-R1):** título, START e menu de saves no ecrã pelo Renut; o modo
`xenos` (padrão) continua igual. Logs e capturas: `~/Downloads/SwordMan-renut-M2-rp5-20261023-2009.zip`.
- Driver: patch 0006 (o plume abre o driver do app pelo adrenotools; `plume_android_vulkan_library` em
  `src/renut_glue.cpp`, aberto uma só vez e partilhado com a verificação).
- Duas causas de falha encontradas no aparelho:
  - falha de memória do guest ~27 s após o arranque (lê `0x23` em `sub_827E2C00`): o MGR indexa o escalonador de
    tarefas pela CPU lógica do PCR e, sem a GPU emulada, a thread principal caía num slot não registado (-1).
    Corrigido como no mgr-nx-deko: `SetAffinity(1)` no `OnPostLaunchModule`;
  - ecrã preto com a imagem certa (a captura `swordman_renut_shot_s` mostrava o título): o gralloc da Adreno
    recusa a swapchain `B8G8R8A8` (`AdrenoUtils: Memory Layout input parameter validation failed`). Patch 0007:
    `R8G8B8A8_UNORM` no Android.
- Swap: chama o `rex_vdswap_hook`; a pausa do menu do app e a retoma funcionam.
- Verificação de funções no `OnPreSetup` (onde o SDK decide se carrega o plugin): BDA, int64, descriptor indexing
  com 8192 texturas, BC, vertex stores, D32S8. Se faltar, fica o Xenia e o log diz o quê. O Turnip passa (Vulkan 1.3,
  com geometry shader). Uma falha no `StartRenderer` já não pode voltar ao Xenia e para com erro claro.
- Segundo plano: patch 0008 recria a swapchain para a nova `ANativeWindow`; sem janela não apresenta.
- Rotação: o painel do RP5 é nativo em paisagem (camada `ROT_0`, composição `DEVICE`), `IDENTITY` serve.
- Números (título, 1280x720 nativo): jogo a 30 fps (o título do MGR é 30), GPU do Renut ~11,3 ms por frame,
  ~500 draws por frame; logos a 60 fps com GPU ~3 ms.
- A fazer no M3: o overlay de depuração do SDK mostra estatísticas do Xenia (FPS "--", "AMD FSR 1"); a sonda de
  latência e o late input precisam do read hook do SDK; testar o driver Qualcomm e o RG Cube (Mali sem BC deve
  cair para o Xenia pela verificação).

### M3: gameplay (semanas)
- Shaders que faltem (captura em tempo real), caminhos sem geometry shader (retângulos e point sprites como o
  nosso 0154), Mali (sem BC: descompressor) como segundo port, desempenho no Adreno (tiling), arquivo de shaders
  distribuível gerado das cópias do jogador.
- Funções que só existem no Xenia e deixam de funcionar no modo Renut: FSR do presenter, interpolação de
  frames, imagem congelada, `swordman_skip_ps`, captura do AutoPilot. A refazer no Renut ou deixar de fora.

**Estado (23/10, RP5, Turnip v27):**
- Gameplay: jogo novo no slot 3 (Normal, sem tutorial); o "Continue" do slot 3 abre na primeira luta do prólogo.
  `tools/fight-bench.sh` repete essa luta (golpes X 45 s) e resume fps de jogo, GPU ms e draws.
- Números na luta (depois da correção da thread de UI; antes: Renut 720p 47,8 fps, 540p 46,2):

  | | fps de jogo | GPU ms/frame |
  |---|---|---|
  | Renut 720p | 51,6 | 10,9 |
  | Xenia 720p | 45,9 | 17,5 |
  | Renut 540p | 52,8 | 7,8 |
  | Xenia 540p | 50,9 | 12,2 |

  O Renut gasta 35–45% menos GPU, mas o jogo fica preso na CPU (~19–20 ms de Swap a Swap) nos dois modos.
- 540p no Renut: patch 0009 (código de escala 4 = 3/4 no Android, no `render_scale.h` e no `renutTextureScale`).
- Thread de UI a 99% de um núcleo no modo Renut: o `SDL_WaitEvent` do Android põe um sentinela a cada volta e o
  `SDL_PeepEvents` tira-o logo (acorda-se a si mesmo); no Xenia escondia-se atrás da pintura bloqueante.
  `SDL_POLL_SENTINEL=0` no `GameActivity` (o SDK não usa `SDL_PollEvent`). Sem alterações ao SDL (o `CLAUDE.md` dele
  proíbe código de IA).
- Imagem: nenhum shader em falta (título, menus, prólogo, 3 min de luta; `shader_capture/` nunca criado). As cores
  do HUD/logo diferem entre os modos (amarelo↔ciano, ALERT vermelho↔azul): o Renut está certo; o Xenia troca R/B
  nessas texturas com o Turnip v27 (com o driver Qualcomm o Xenia mostra as cores do Renut).
- Título (11,1 ms de GPU a 30 fps), por eliminação com `swordman_renut_skip_ps` (patch 0010; o perfil por shader
  engana no Adreno, que acumula o pass inteiro no rótulo ativo quando o tile é despejado): partículas/chuva
  `86BDA36CCDF9E60F` + `1E9E7E4145103CBD` (258 draws com blending) ~3,5 ms; `BDB9D2846A954DDF` (12 draws) ~1,4 ms;
  o fundo `2173EC4790622AC5` (vídeo) ~0,3 ms; custo fixo do Renut (logos, 6 draws) ~3 ms, dos quais depth
  resolves 0,7, resolves 0,5 e present 0,4.
- Overlay de depuração: números do Renut (SDK 0175, `SetUpscalerOverride`). Read hook do SDK (0176): a sonda de
  latência mede (título: entrada → present 34 ms).
- Driver Qualcomm do sistema no RP5: Vulkan 1.1 sem `shaderInt64` → a verificação mantém o Xenia. O v891.7 pede API 36.

## Como trabalhar
O ciclo compilar/corrigir/testar é no PC (NDK + arquivos do jogo + RP5 no adb), com o Claude Code do terminal no
branch `renut-android`. Cada marco fecha com um zip de logs do RP5.
