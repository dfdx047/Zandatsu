# MGR PC — sessão de 20/11/2026 (13:15 → 19:00 Lisboa)

Branch `pc/perf-665` (sem push). Só o Odin 2 Mini (192.168.1.206), perfil sd480p com pin. Turnip v27.
Medida = 3 × 30 s no combate do CONTINUE do prólogo, GraphicOption "mobile", agora com chamadas D3D/quadro.

## Resumo

| # | Item | Resultado | Estado |
|---|---|---|---|
| 0 | 26,0 ontem × 17,2 hoje | ontem a CPU estava acima do teto sd480p (antes do keeper HL_PIN); 17 é o número certo | resolvido, medida corrigida |
| 1 | Launcher | jogo PC abre pela launcher Zandatsu, perfis + gráficos, controles 360 sempre visíveis, toque = clique, pausa, editor | feito, testado no Odin |
| 1 | APK | 222,5 → 172,1 MB (libmgrpc ZDT3 em xz) | feito |
| 2 | DXVK 2.7.1 nativo | abre título e combate; A/B 17,6 × 17,7 fps (1.9.2b), CPU igual, GPU 47 × 50% | empata, 1.9.2b fica |
| 3 | Esperas dos jobs / afinidade / ADPF | giro 50 µs 14,3; giro + handoff 14,5 + 1 crash; ADPF + núcleos grandes 20,0 (base 20,3) | pioram ou empatam: desfeito/desligado |
| — | Crash do CONTINUE | guardas F40EA0 + F41B10 (lift `guards.txt`) + página NULL do guest só de leitura: launcher → combate 5/5 (antes 1/3) | mitigado |

## Fase 0 — 26,0 × 17,2 fps

- Causa numa linha: **ontem a medida correu com a CPU acima do teto sd480p** (pservice da AYN, antes do keeper
  `HL_PIN`); hoje, com ou sem pin e com `performance_mode 1`, os relógios ficam no teto e dá ~17.
- O build antigo dá os mesmos 17,1 / 24,7 / 18,2: não é `-ffp-contract=off` nem 0018/0019.
- O mesmo combate é **bimodal por carga de cena**: ~8k ou 13–16k chamadas D3D/quadro. As corridas de 24–25 fps são
  a cena leve. `m4run.sh` regista agora `calls N` em cada corrida e `m4x3.sh` a lista.

## Fase 1 — launcher

| Item | Como | Prova |
|---|---|---|
| Abre pela mesma launcher | aba "PC (beta)" a seguir ao Início; JOGAR (PC) → `PcGameActivity` | fotos l1, g1 |
| Aba Gráficos | todas as opções do GraphicOption + resolução interna (Nativa/720p/540p/480p), limite de fps (60/45/30), perfis Fraco (SD665/480+) / Baixo / Médio / Alto / Personalizado; FSR e interpolação ficam "em breve" | fotos l2, l3 |
| Jogo já configurado | perfil aplicado no 1.º abrir (SoC → Fraco no Odin sd480p), exe gravado logo | log `render size 854x480`, cap 30,0 fps |
| Controles sempre visíveis | **janela-painel** (TYPE_APPLICATION_PANEL, sem foco) por cima da NativeActivity: a superfície da janela é do jogo e as Views dela não aparecem | foto g1–g6 |
| Toque = clique nos menus | toque curto fora dos controles → `nativePointer` → `setGuestPointer` | slot e CONTINUAR escolhidos por toque (g2, g3) |
| Combate | A, X (ataque), Blade Mode (LT + arrastar, linhas vermelhas) | g4, g5, g6 |
| Menu lateral | só Pausa + Controles (o resto é o menu OPTIONS do jogo) | g7 |
| Pausa | menu aberto → `setGuestPaused`; o loop principal desligava a pausa a cada volta (corrigido) | 2 capturas com 4 s de intervalo: diferença 0 |
| Editor | barra numa linha (antes "Salvar" partia na vertical) | g8 |
| fps pela launcher | perfil Fraco, cap 30 | 18,0 / 17,9 / 17,8 = **17,9** |

### APK: por que 327 MB e o que mudou

| Entrada | Antes | Depois |
|---|---|---|
| APK debug (classes.dex 42 MB) | 327 MB | — |
| APK release (R8) | 222,5 MB | **172,1 MB** |
| `libmgrpc.zdat` (PC, .so de 360 MB, .text 341 MB) | 143,7 MB (zlib-9, blocos 1 MiB) | **93,2 MB** (ZDT3: xz, blocos 8 MiB) |
| `libgame.zdat` + `libgame-v80.zdat` (360, ARMv8.1 e ARMv8.0) | 65,7 MB | igual |

- ZDT3 desempacota em paralelo (um bloco por núcleo, pwrite no memfd): **7,9 s** do arranque do processo até a lib
  carregada no Odin. O app continua a ler ZDT2.
- Testado e descartado: `--icf=all` no link (−0,02%). zstd-19 daria 106,9 MB.

### Crash no carregamento do CONTINUE (antigo, intermitente)

- `[0x01B788C0+0x1E70]` fica NULL: `sub_00F44440` cria o objeto, `sub_00F42590` destrói-o (escreve 0) e os jobs
  (`sub_009CEFE0` → 0x009D5FAE, `sub_00A50700`) ainda chamam `F40EA0`/`F41B10`. O chamador só testa
  `[0x1B7886C] == 2`.
- Guarda em `F40EA0` (override `pc/runtime/guest_guards.c`) e guarda à entrada de `F41B10` (mecanismo novo do lift:
  `pc/config/guards.txt`, o corpo fica `sub_X_body`): 4 de 5 corridas vivas; o 3.º sítio foi `00A7C8A0`.
- Página NULL do guest (0..0xFFFF) legível como zeros (`MGR_NULL_READ=0` desliga; escritas continuam a falhar):
  **5 de 5** corridas launcher → combate vivas.

## Fase 2 — DXVK 2.7.1 nativo (NDK, WSI ANativeWindow mínimo)

| Corrida | fps | chamadas/quadro | CPU | GPU |
|---|---|---|---|---|
| 2.7.1 · 1 | 16,7 | 13.700 | 138% | 53% |
| 1.9.2b · 1 | 17,0 | 11.681 | 138% | 53% |
| 2.7.1 · 2 | 18,0 | 9.387 | 145% | 45% |
| 1.9.2b · 2 | 18,7 | 8.892 | 147% | 47% |
| 2.7.1 · 3 | 18,2 | 8.366 | 146% | 43% |
| 1.9.2b · 3 | 17,4 | 11.357 | 146% | 51% |
| **Média** | **17,6 × 17,7** | 10,5k × 10,6k | 143 × 144% | 47 × 50% |

- Abriu à primeira (título e combate). Combate preso na CPU: trocar de DXVK não mexe no fps.
- Falta portar do 1.9: pass timing, cache de pipelines, query_check, fallbacks Mali/PowerVR/A610. 1.9.2b fica padrão.

## Fase 3 — esperas dos jobs, afinidade, ADPF (DXVK 1.9.2b, 3 × 30 s)

| Variante | 1 | 2 | 3 | CPU | Decisão |
|---|---|---|---|---|---|
| base | 25,4 (8,0k) | 16,9 (11,7k) | 18,5 (8,3k) | 144% | — |
| `MGR_SIGNAL_SPIN_US=50` | 14,4 (8,2k) | 14,9 (8,9k) | 13,5 (13,6k) | 167% | pior, desligado |
| + `MGR_SIGNAL_HANDOFF=1` | crash | 14,9 (8,7k) | 14,1 (11,1k) | 170% | pior, revertido (`d42ae12`) |
| `MGR_ADPF_ALL=1 MGR_AFFINITY_BIG=1` | 25,2 (8,1k) | 16,5 (14,1k) | 18,3 (10,9k) | 148% | igual, desligado |

- O wake já era direto por handle. O custo é a máquina única: o acordado só corre quando o sinalizador a solta.
- Girar gasta os 4 núcleos fracos que o thread do jogo precisava (CPU 144 → 167%, fps −30%).
- O crash do handoff foi o do CONTINUE (F41B10), não do handoff.

## Extra (17:00–17:30) — x86diff completo, para o bug da física

- 5187 funções float × 3 entradas, PC (C do lift) contra x86 (Unicorn), em lotes de 600 com timeout de 10 min.
  7 de 9 lotes terminaram (06 e 07 estouraram o tempo).
- 152 funções com alguma diferença: 113 só em `st0`–`st7`. São restos de registos x87 já vazios depois de uma pilha
  equilibrada (o Unicorn mostra o valor antigo, o lift mostra 0): ruído do harness.
- Ficam 37 com diferenças em memória/registos. A maioria começa a meio de outra função (`fnstsw`, `fxch`, `ja` com
  flags do chamador), por isso as entradas são lixo.
- Verificado à mão: `ucomiss`/`comiss` → `lahf` → `test ah, 0x44` → `jp/jnp` (teste de igualdade de floats do MSVC)
  dá as flags e a paridade certas.
- Nenhum erro de lift novo confirmado. Candidatos por ver: `sub_00DB6C80` (bit 7 diferente numa escrita, 1 semente),
  `sub_00CE7288/7344/7390`, `sub_00DDABA9`, `sub_01220181`, `sub_01228FAF`.

## Odin no fim

- O perfil sd480p **continua ativo**: a app helper `com.dfdx047.hlprofile` já não está instalada (os broadcasts
  "completam" sem ninguém os receber) e o keeper corre como root (pid 17200). Repor exige a helper ou um reboot; não
  reiniciei para não perder o adb por Wi-Fi.

## Pendências

1. Causa raiz do crash do CONTINUE (estado `[0x1B7886C]` ainda 2 quando `sub_00F42590` anula `+0x1E70`): mitigado, não resolvido.
2. Bug da física: x86diff completo sem erro de lift novo confirmado (ver acima); altura do Raiden pelo ponteiro do jogador por fazer.
3. FSR 1 e interpolação 30→60: "em breve" na aba Gráficos.
4. CPU: reduzir a serialização da máquina única (jobs puros fora da máquina), já que girar/ADPF não ajudam.
5. DXVK 2.7.1: portar pass timing / cache de pipelines / fallbacks antes de trocar.
6. Toque = clique: no aviso de autosave o 1.º toque só realçou o OK.
8. Odin ainda no perfil sd480p (helper hlprofile ausente, keeper root pid 17200).
7. Disco do PC: `/home` a 100% por um `.part` de 141,9 GB em `~/Downloads` (não mexi).

## Commits (pc/perf-665, sem push)

`d56d029` launcher/controles/knobs · `3abd09c` DXVK 2.7.1 · `0081b38` janela-painel, pausa, ZDT3, guarda 00F40EA0 ·
`68f5f78` handoff → `d42ae12` revertido · `ec05370` guardas do lift + página NULL · PCPorts `c5ffa50` + fecho.

Relatório: https://claude.ai/artifact/QcBYPSaMbWAqrLTYUfLYvh
