# MGR PC — otimização no perfil 480+ (19/11/2026, 19:35–21:00 Lisboa)

Odin 2 Mini no perfil sd480p, Turnip v27, combate do prólogo (cena CONTINUE). Cada valor é a média de 3 corridas de
30 s (`build/lab/pc/m3x3.sh`). Relatório com fotos (Artifact privado): https://claude.ai/artifact/LCsQnoeSb2Q62P45x6BPht

## Resumo por item

| # | Item | Estado | Antes → depois |
|---|---|---|---|
| 1 | CPU: -O2 + `RECOMP_LOCAL_REGS` validado por difftest | feito | 22,0 → **26,0 fps** (+18%); difftest 246/246 no Odin; função a função 0 divergências (amostra 200 × 2) |
| 2 | Calibrar a GPU do sd480p | medido, não calibrável por frequência | Wild Life 2468 no perfil × 982 de um SD480+ real: GPU ~2,5× uma 619 |
| 3 | GPU: preset de celular + `dxvk.conf` | parcial | padrão 20,0 → celular 26,9 → + dxvk.conf 26,2 (ruído, não adotado) fps; resolução interna + FSR não feito |
| 4 | DXVK 2.x no Turnip | não feito | não há build Android do DXVK 2.x (WSI só SDL2/SDL3/GLFW) |
| 5 | Tempo de GPU por passe | não feito | DXVK 1.9.2b sem timestamps por passe |
| 6 | 360 × PC | preparado | Odin com 26 GB livres; app 360+PC instalado; ISO no SD; falta importar e medir |
| 7 | DRM | à espera do exe da Steam | `pc/tools/exe-check.py`; opções para `SteamAPI_Init` em PCPorts `drm-legal/` |

## 1. CPU

| Build | Corridas | Média | GPU | CPU |
|---|---|---|---|---|
| -O1, registos globais | 20,3 · 22,9 · 22,9 | 22,0 | 66% | 142% |
| -O2 + LOCAL_REGS + X87_WINDOW + MEM_QUAL vazio | 27,3 · 25,3 · 25,4 | **26,0** | 79% | 139% |

Mesmo `GraphicOption` nos dois lados: (2,0,1,1,5,0,1,0,1,1).
- `pc/tools/difftest-android.py`: casos do difftest do pcrecomp compilados com o clang do NDK e as flags reais, corridos
  no Odin: 246/246 (-O1 e -O2+LOCAL_REGS).
- `pc/tools/funcdiff.py`: funções folha reais do lift (27.280), -O0 × config rápida no Odin, hash de registos, flags,
  x87, XMM e páginas escritas. 200 × 2 seeds: 393 iguais, 0 diferentes, 7 timeout dos dois lados. Corrida 3000 × 3:
  lado -O0 feito (9000), lado -O2 por correr (`--reuse ref`).

## 2. Calibragem

| Medida | Wild Life |
|---|---|
| Odin sd480p, GPU 124,8 MHz | 2468 (14,78 fps) |
| idem, slice LLC da GPU desligado | 2474 |
| SD480+ real (Moto G53 5G, mediana UL) | 982 (Unlimited 973) |

124,8 MHz é o degrau mínimo da Adreno 740. Fps preso na GPU no perfil ≈ 2,5× um 480+ real (× 0,40).
Anotado em `tools/device-profile.sh`.

## 3. GPU, configuração (build -O2+LOCAL_REGS)

| Configuração | Corridas | Média |
|---|---|---|
| Padrão do jogo (2,2,0,1,5,0,1,1,1,1) | 19,9 · 20,3 · 19,9 | 20,0 (GPU 99%) |
| Celular (2,0,1,0,5,0,1,0,0,0) | 27,0 · 26,8 (3.ª presa no arranque) | 26,9 |
| Celular + `dxvk.conf` (forceSwapchainMSAA 0, maxFrameLatency 1, samplerAnisotropy 0) | 27,0 · 26,3 · 25,3 | 26,2 (ruído, não adotado) |

Correção: os 15,4 fps de ontem ("padrão") não eram o padrão; o `GraphicOption` no SD estava em (2,0,1,1,5,0,1,0,1,1).

## Pendências

- funcdiff 3000 × 3 lado -O2; travão de GPU de laboratório (emular a 619); resolução interna + FSR na bridge e
  preset de celular como padrão do launcher; DXVK 2.x com WSI Android; tempo por efeito (skip por hash de PS);
  360 × PC; teste do exe da Steam.
- 1 arranque em 12 ficou sem nenhum quadro durante 2,5 min (corrida g-mobile-3); por investigar.
