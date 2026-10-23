# Renut no Android: renderizador nativo do mgr-nx-deko

Objetivo: trocar a emulação da GPU do Xbox (Xenia: EDRAM, resolves, cópias) pelo renderizador nativo de
Direct3D do [mgr-nx-deko](https://github.com/tuancapoo/mgr-nx-deko) (reNut + plume, Vulkan), como opção
escolhida no launcher. O Xenia fica como alternativa. Análise completa, com ficheiros e linhas:
[renut-port-analysis.md](renut-port-analysis.md).

- Upstream: `third_party/mgr-nx-deko` (submódulo, commit `ba1a837`), GPL-3.0. Mesma `default.xex` (USA/Europe)
  e mesma base ReXGlue v0.10.0.
- **Licença:** autorização dos autores em 23/10/2026 para usar à vontade, mantendo os créditos
  ([CREDITS.md](../CREDITS.md)) e o código aberto (GPL-3.0).
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

### M2: abre até ao título (≈1 semana)
1. `native_window.cpp`: ramo `__ANDROID__` com `SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER` (como o nosso
   `window_sdl.cpp:466-474`); sem o `setenv(SDL_VIDEO_DRIVER=x11)`.
2. plume: `volkInitializeCustom` com o driver do adrenotools (Turnip).
3. O swap do Renut (`sub_826B3510`) chama o nosso `rex_vdswap_hook` (pausa, watchdog, contagem de frames).
4. Verificação de funções Vulkan (buffer device address, int64, descriptor indexing com 8192 texturas, BC1–BC5,
   D32S8): se faltar, volta ao Xenia com uma linha no log.
5. Superfície perdida ao ir para segundo plano e rotação (`preTransform`).

### M3: gameplay (semanas)
- Shaders que faltem (captura em tempo real), caminhos sem geometry shader (retângulos e point sprites como o
  nosso 0154), Mali (sem BC: descompressor) como segundo port, desempenho no Adreno (tiling), arquivo de shaders
  distribuível gerado das cópias do jogador.
- Funções que só existem no Xenia e deixam de funcionar no modo Renut: FSR do presenter, interpolação de
  frames, imagem congelada, `swordman_skip_ps`, captura do AutoPilot. A refazer no Renut ou deixar de fora.

## Como trabalhar
O ciclo compilar/corrigir/testar é no PC (NDK + arquivos do jogo + RP5 no adb), com o Claude Code do terminal no
branch `renut-android`. Cada marco fecha com um zip de logs do RP5.
