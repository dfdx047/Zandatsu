# MGR PC — noite 19→20/11/2026 (22:55 → 09:10 Lisboa, prazo era 07:00)

Branch `pc/perf-665` (sem push). Só o Odin 2 Mini (192.168.1.206), perfil sd480p com pin; reposto no fim.
Turnip v27, GraphicOption "mobile" nos dois lados de todo A/B. Medida = 3 × 30 s no combate do CONTINUE do prólogo.

## Resumo honesto

| # | Item | Estado |
|---|---|---|
| 1 | Bug da física (no ar, corpo dobrado) | 2 divergências reais do lift corrigidas (0018, 0019); **não confirmado que eram a causa** |
| 2 | Linha de base com as correções | 17,3 e 16,9 fps (antes 17,2): as correções não custam nada |
| 3 | Fase 1: resolução interna 854×480 | 18,1 fps (+5%): o combate está preso na CPU (GPU 48–55%) |
| 4 | Fase 4: CPU (handoffs da máquina) | spin 300 µs = 17,4 (igual); mapa de quem solta a máquina feito |
| 5 | Fases 2, 3, 5, 6 | **não feitas** (prazo) |
| 6 | 23:45–06:18 | tempo perdido: corrida do x86diff travou sem limite por função |

## Parte 1 — bug da física

### Reprodução e bissecção
| Build | Bug | Nota |
|---|---|---|
| e) -O2 + LOCAL_REGS + X87_WINDOW | sim | pés ~0,5 m acima da sombra a correr |
| b) -O1 | provável, ambíguo | |
| a) -O0 | inconclusivo | lento demais para julgar |
| e) + `MGR_QPC_SCALE=0.25` (modo 60) | ambíguo | |
| f) `-ffp-contract=off` | já em todos os builds | sem fast-math |

### Causas encontradas (x86diff: Unicorn x86 = verdade, entradas de jogo realistas)
- **0018** `fdivp/fdivrp` por zero: o lift devolvia o dividendo; o x87 dá ±inf. 1530 divisões no MGR.
- **0019** `fist/fistp` de NaN/inf/fora de alcance: o x87 grava o "inteiro indefinido" (0x8000/0x80000000/1<<63);
  o cast em C é UB e o ARM satura. 79 locais + helper i64. No Odin, código antigo falha **14/24** casos; com 0019 **24/24**.
- difftest: 8 + 24 casos novos.

### Confirmação
- A/B visual com o mesmo pilot (`android-o2-old` sem 0018/0019 × `android-o2`): quadros com e sem pés no ar dos dois
  lados. **O vídeo não decide.** Foto: `build/lab/pc/physbug/ab-antes-depois.png`.
- Medida numérica da posição na memória: 4 tentativas falhadas (2 crashes com memcpy; `process_vm_readv` no
  próprio pid não lê quase nada). Abandonada pela regra das 3 falhas.
- Crash `sub_00F40EA0` (SEGV ~70 s): **antigo e intermitente**. Também acontece no build sem 0018/0019 e voltou numa
  corrida de hoje (08:50). Não foi causado pelo 0018.

### Lição
**Validação de float precisa de entradas reais de jogo, não só amostra aleatória**: zeros, infinitos e valores fora de
alcance que o jogo produz de propósito (raios paralelos a um eixo, inf → fistp) não aparecem em amostras aleatórias.

## Parte 2 — otimização

| Fase | Mudança | Antes | Depois | Nota |
|---|---|---|---|---|
| base | 0018+0019, -ffp-contract=off | 17,2 | 17,3 / 16,9 | é o "antes" dos A/B |
| 1 | `MGR_RENDER_SIZE=854x480` | 17,3 | 18,1 | GPU 52→48%, 124,8 MHz |
| 4 | `MGR_SPIN_US=300` | 17,3 | 17,4 | sem ganho, descartado |
| 4 | `MGR_YIELD_STATS` (diagnóstico) | — | — | ~11.700 solturas da máquina/s |

Onde a CPU vai: ~90% das solturas são esperas de sinal (`waitGuestSignal`) no sistema de jobs do jogo:
`00DD81B0` 4.700/s, `01499BED` 2.150/s (WaitForSingleObject), `00DD79A0` 1.100/s, `00DD8DA0` 880/s (ReleaseSemaphore).
Duas corridas a 24–25 fps tinham o Raiden sem inimigo à vista: variação de cena, não ganho.

### Lições de APKs de terceiros (PCPorts/referencias)
- PES 2013 (360 recomp): FSR 3 FI + optical flow sem motion vectors — caminho para 30→60.
- Naruto Storm 4 (exe x64 recomp): núcleos por papel + ADPF (sem root), giro/WFE antes de dormir, ganchos por
  endereço para desligar sombra/partículas, registos passados como argumentos entre funções.

## Pendências
1. Medir a altura do Raiden de outra forma (ler pelo ponteiro do objeto do jogador a partir de uma função conhecida).
2. Jobs: acordar direto o waiter do semáforo/evento com giro curto (modelo Naruto) — alvo os 4 locais acima.
3. Afinidade por papel + ADPF; FSR 3 FI na saída do DXVK.
4. Fases 2, 3, 5, 6; crash `sub_00F40EA0`; x86diff completo (5187 × 3); `sub_01440166` (sinal do NaN), `sub_00DA6880`.

## Commits (pc/perf-665, sem push)
`8c17305` lab · `247f2eb` HL_PIN · `f395b50` x86diff · `e8322c7` pcrecomp 0018 · `5f01fb9` pcrecomp 0019 ·
`57f02ad` probe/pilot · `74aae00` nota crash · `51e05f7` probe por process_vm_readv + MGR_YIELD_STATS.
PCPorts: `2e16abf`, `e528cb0`, `c42f731` (PES), `7a6b2d3` (Naruto).
