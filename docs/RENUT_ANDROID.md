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

## Limite de CPU no Renut (~20 ms de Swap a Swap, 23/10)

### 1. Medição (simpleperf -g 20 s na luta do fight-bench, Renut 720p, 52 fps durante a gravação)
- Nenhuma thread está no limite: a 2000 Hz, 100% de um núcleo seriam 40 000 amostras. A mais ocupada tem ~16 400.
  - Thread principal do jogo (`Main XThread`, a que chama o Swap): ~18% de um núcleo. Perfil off-CPU: 98% do tempo
    bloqueada em `sub_828BE6C0` → `NtWaitForSingleObjectEx`, chamada do laço principal (`sub_82BCB128`): espera
    que os workers do escalonador de tarefas do MGR acabem o frame.
  - Thread de render do Renut: ~23%.
  - Os 5 workers do escalonador (`XThread568E1…`, `574EC…`, `548DB…`, `528D5…`, `508CF…`): 24–41% cada. São o
    caminho crítico. 14% do tempo deles é `rex::thread::atomic_cas` (o spin do `RtlEnterCriticalSection` do SDK,
    `absolute × 256` voltas sem pausa) e 4,5% é `sub_828BED28` (espera ativa do jogo num lock tipo leitor/escritor:
    `while (*obj > 0)` com `lwsync`); 9% é kernel (futex).
- 15 funções mais quentes do processo (ciclos): kernel (escalonador/futex) 8,3%; `rex::thread::atomic_cas` 7,3%;
  `@plt` de libmain 3,2%; `sub_828BED28` 2,3%; `__memcpy` 1,7%; kernel 1,7%; `XXH3_hashLong_64b_withSeed` 1,6%
  (hash de streams do Renut); `renut::gpu::Capture…` 1,2%; `__aarch64_cas2_acq` 1,1%; `@plt` de librexruntime 1,0%;
  `__aarch64_swp2_rel` 0,9%; `__imp__sub_8279FD90` 0,9%; `sub_822F67E0` 0,9%; `sub_825DD228` 0,8%;
  `renut::gpu::Bind…` 0,8%.

### 2. `sub_826B2FC0` e as esperas ativas
- `sub_826B2FC0` / `sub_8278E640` não aparecem no perfil (0,00%): com o Renut as esperas de fence do D3D estão
  substituídas e esta espera não gira (no PC do mgr-nx-deko girava com a GPU emulada). Nada a trocar.
- As esperas ativas reais (workers) foram trocadas atrás de cvars, medidas e deixadas desligadas:
  `swordman_cs_spin` (SDK 0177, limita o spin do `RtlEnterCriticalSection` com `yield`): **pior** (46,4–47,5 fps
  contra 52–53); `swordman_rwlock_wait` (`app/src/main/cpp/mgr_cpu_hooks.cpp`, `sub_828BED28` com `yield`/
  `sched_yield`): neutro. Os locks são curtos e o spin é útil; dormir atrasa os workers.

### 3. Ritmo
Off-CPU da thread principal: 97,7% em `sub_828BE6C0` (espera pelos workers); o Swap do Renut com o pacing
(`PaceSwap` → `sleep_for`) 0,6%; a fila da thread de render 0,07%; o hook do `SynchronizeToPresentationInterval`
não espera. O log do Renut confirma: `0,12 ms a frame paced`, quase todos os frames "late" (libertados logo). Não é
ritmo: é CPU dos workers do jogo.

### 4. Afinidade (`swordman_renut_cores`, patch renut/0011 + `src/renut_glue.cpp`)
Núcleos grandes lidos do cpufreq. Fight-bench 720p, Turnip v27, medianas (com golpes; "parado" = `ATTACK=0`):

| | fps de jogo | GPU ms | Swap a Swap |
|---|---|---|---|
| base de antes | 51,6 | 10,9 | ~19 |
| `cores = 0` (padrão; 0177 desligado) | 52,2 / 53,3 | 10,0 / 9,9 | 19,0 / 18,5 |
| `cores = 1` (as duas nos 4–7) | 49,9 / 52,8 | 11,5 / 10,3 | 19,9 / 18,8 |
| `cores = 2` (principal no 7, render nos 4–6) | 57,7 / 56,7 | 11,7 / 10,4 | 16,3 / 17,1 |
| 0177 ligado (`swordman_cs_spin = 64`) | 46,4 / 47,5 | 10,8 / 10,4 | 21,5 / 21,1 |
| parado, `cores = 0` | 51,0 / 52,9 / 50,7 / 51,6 | ~10,4 | 19,3 |
| parado, `cores = 2` | 54,2 / 56,5 | 10,4 / 9,9 | 18,3 / 17,4 |

`cores = 2` ganha ~3 fps (~1 ms por frame), mas nenhum modo passa de 58 fps: o padrão continua 0. No limite
continuam os workers do escalonador de tarefas do MGR; com `cores = 2` faltam ~0,1 ms para 58 fps (17,2 ms) e
~0,6 ms para 60 (16,7 ms); com `cores = 0`, ~2 ms.

### 5. Perfil dos workers (caminho crítico; `cores = 2`, 20 s com `-g`)
Os workers mais ocupados ficam a 35–43% de um núcleo cada (e a thread de render ~51%). 15 funções mais quentes:
kernel/futex 7,1%; `atomic_cas` (spin das secções críticas) 6,2%; `@plt` de libmain 4,8% (chamadas entre
bibliotecas); `renut::gpu::CaptureDelta` 2,9% (o Renut grava os draws na thread do jogo que os emite);
`sub_8278BFC8` 1,9%; kernel 1,8%; `sub_822F67E0` 1,7%; `sub_8279FD90` 1,6%; `__memcpy` 1,1%;
`__aarch64_cas2_acq` 1,0%; `sub_8278B798` 1,0%; `@plt` de librexruntime 1,0%; `__aarch64_swp2_rel` 0,9%;
`sub_828BED28` 0,8%; `sub_822F4110` 0,8%. Sem função dominante: o próximo passo é o codegen (opções do SDK deles,
PGO), medido com o fight-bench.

`tools/fight-bench.sh`: medianas, GPU ms impossíveis descartados (o Renut às vezes reporta uma janela errada),
Swap a Swap no resumo, `ATTACK=0` para comparar mudanças pequenas (±1 fps em vez de ±3).

## Fase codegen/CPU (24/10; RP5, Turnip v27, fight-bench 720p, `cores = 2`, Raiden parado)
Corridas com `device lost` excluídas das médias (ver abaixo). "Principal" = CPU da thread principal do jogo.

| Passo | fps de jogo | Swap a Swap | Principal | Resultado |
|---|---|---|---|---|
| base (PLT entre funções `sub_*`) | 57,1 / 57,9 / 55,1 | 17,0 / 16,6 / 17,9 ms | 12/11/13% | referência |
| 1. `-Bsymbolic-functions` | 57,8 / 59,5 | 16,7 / 15,4 ms | 12/11% | ~0,8–1 ms com a mesma carga; **ligado** |
| 2. ThinLTO (código gerado + libmain) | 56,7 / 58,7 | 17,4 / 15,8 ms | 14/12% | igual ao 1; opção `-Pdf.thinlto=true`, **desligado** |
| V0 = SDK 0178 com tudo desligado | mediana 57,3 (7 corridas) | 16,8 ms | 12–13% | igual; não validado |

- PLT: 25 334 entradas `sub_*` → 0 com `-Bsymbolic-functions` (ficam 194 imports do kernel, que estão no
  librexruntime, partilhado com o plugin do Xenia: ligar o SDK estático não é possível). O código gerado já era `-O3`.
- ThinLTO: build completo numa pasta `.cxx` à parte em 293 s, relink 19 s.
- SDK 0178: as opções de codegen do mgr-nx-deko (o patch deles, só codegen/ppc/templates, aplica limpo) e um
  `rex/sys_counters.h` sem contagens. Com tudo desligado já muda o código: `sync`/`lwsync`/`eieio` passam a
  `std::atomic_thread_fence` (~490; antes não geravam nada, incorreto no ARM64) e uma tabela de `switch` perde
  uma entrada falsa. `mgr_codegen.toml` (incluído no manifesto e no carimbo do codegen) liga as opções.
- V1–V4 compilam (fragmentos TOML: V1 `cr/ctr/xer/reserved_as_local` + `skip_msr`; V2 + `non_volatile_as_local`;
  V3 + `non_argument_as_local` com a lista `keep` de 12 085 funções do mgr-nx-deko; V4 + as reduções "diet").
  V5 (`args_in_registers`) precisa de `mgr_args_exclude.txt` (as 25 funções com hook). **Ainda não medidas nem
  validadas.** Build de cada variante: ~5 min.
- Validação: `tools/validate-run.sh` + `tools/validate-compare.py` (título e menus, save do slot 3, 3 min de luta
  com Blade Mode por `seq` do AutoPilot, game over + continue, cutscene do prólogo comparada com a base).

### Bloqueio: `VK_ERROR_DEVICE_LOST` intermitente no Renut (Turnip v27)
A validação da base congelou a imagem aos ~2 min de luta: `Vulkan device lost`, e no `dmesg`
`kgsl: MISC: GPU hang detected ... status 00E59005` (um hang, não uma falha de página; o mesmo status sempre).
O jogo continua a fazer Swaps, por isso o fight-bench não o via (as janelas de "GPU ms" absurdos eram isto).
Frequência por corrida de 2,5 min de luta: base 9/~42; `TU_DEBUG=...,sysmem` 1/12; V0 3/10; V0 sem geometry
shaders (`swordman_renut_no_gs`, patch renut/0012) 1/4 (teste interrompido). Os laços dos shaders traduzidos são
todos limitados (constantes literais). Causa ainda por encontrar; até lá nenhuma variante passa "nada pode travar".

## Hang da GPU no Renut (24/10; RP5, V0, 720p, `cores = 2`)

### 1. Testes de causa sem código novo (3 corridas do fight-bench de 5 min de luta, `FIGHT_S=300`, com golpes)
Cada corrida dura ~6,5 min desde o arranque (título, menus, carregamento, 5 min de luta). fps e GPU ms: medianas
das janelas antes do hang (as corridas com hang antes da luta não têm números). Cada hang no log tem o seu
`kgsl: MISC: GPU hang detected` no `dmesg` à mesma hora, sempre com `status 00E59005` (uma vez `00E51005`).

| Driver / modo | Hangs em 3 | Quando | fps de jogo | GPU ms |
|---|---|---|---|---|
| a) Turnip v27 (base) | 1 | 4,2 min de luta | 59,4 / 59,4 / 58,8 | 10,7 / 9,6 / 10,3 |
| b) Turnip do ARMSX2 (`turnip-axfl2-003`) | 2 | 4,8 min de luta; 34 s depois do arranque (título) | 59,0 / 58,5 | 11,4 / 11,0 |
| c) v27 + `TU_DEBUG=push_consts_per_stage,sysmem` | 2 | 74 s e 81 s depois do arranque (menus/carregamento) | 56,0 (c2, 1,5 min) | 9,6 |
| d) v27 + profundidade D24S8 (`swordman_renut_d24s8`, patch renut/0013) | 1 | 60 s depois do arranque | 59,2 / 59,6 | 9,7 / 8,7 |

- Nenhum zera os hangs: 6 em 12 corridas (~78 min), um a cada ~13 min, em qualquer modo. O sysmem já tinha dado
  1/8 num teste anterior; o D24S8 não muda nada (o hang do ARMSX2 com D32S8 + discard não é este).
- Os hangs não são só da luta: título, menus e carregamento também. O fight-bench antes só contava os que vinham
  depois do início da luta (4 destes 6 passavam como "sem hang", com números de uma GPU que já não desenhava);
  agora conta-os ("HANG antes da luta, N s depois do arranque").
- c2: depois de 1,5 min a luta passou a 78 draws/frame (fora do combate) e o resto não conta.
