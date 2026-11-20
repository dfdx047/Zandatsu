# MGR PC — noite 19→20/11/2026 (22:55–07:00 Lisboa)

Branch `pc/perf-665`. Odin 2 Mini, perfil sd480p (reposto no fim: "no profile active"), Turnip v27.

## Resumo honesto

| # | Item | Estado |
|---|---|---|
| 1 | Bug da física (personagens no ar, corpo dobrado) | **divergência real do lift corrigida (fdivp/fdivrp por zero); NÃO confirmado que seja a causa do bug** |
| 2 | Otimização da noite (fases 1–6) | **não avançou** (parada às 22:55 pela prioridade; nada retomado) |
| 3 | 23:45–06:18 | **tempo perdido**: a corrida grande do x86diff travou no Unicorn sem limite por função e não foi verificada |

## Bug da física

### Reprodução (`build/lab/pc/physbug/repro.sh`)
CONTINUE do prólogo → combate (minimapa amarelo) → 6 fotos paradas → pilot de caminhada → vídeo 14 s (4 quadros/s).

| Build | Bug | Nota |
|---|---|---|
| e) -O2 + LOCAL_REGS + X87_WINDOW (atual) | sim | pés ~0,5 m acima da sombra a correr |
| b) -O1, registos globais | provável | quadros com os pés no ar; ambíguo |
| a) -O0 | inconclusivo | o jogo corre lento demais, Raiden quase não anda no vídeo |
| e) com `MGR_QPC_SCALE=0.25` (lógica no modo 60) | ambíguo | pés mais perto do chão, ainda com quadros no ar |
| c), d) | não compilados | a foto não decidia; troquei de abordagem (regra das 3 falhas) |
| f) `-ffp-contract=off` | já está em todos os builds | flags reais confirmadas, sem fast-math |

### Divergência encontrada (candidata a causa)
`pc/tools/x86diff.py` (novo): funções float folha do lift executadas no Unicorn (bytes x86 do exe = verdade) e no C
gerado, com as mesmas entradas realistas (posições, vetores unitários, quaterniões, 0, 1, dt).
1200 funções × 2 entradas: 2148 iguais, 45 diferentes.

Diferença real: o lift32 do pcrecomp gerava, para `fdivp`/`fdivrp`,
`st = divisor == 0.0 ? dividendo : dividendo / divisor`. No x87 (ZE mascarado) `x/0 = ±inf`. O jogo usa esse `inf`
(raio com componente 0 = não acerta); o lift dava um valor finito. 1530 divisões no MGR.
Ex.: `sub_00F46062`, `sub_00F46EA0` escreviam 1,0 onde o x86 escreve inf.

### Correção
- `pc/patches/pcrecomp/0018-lift32-fdivp-fdivrp-divide-by-zero-gives-inf-like-x87.patch`: divisão IEEE pura.
- 8 casos novos no difftest (`x87.fdivp/fdivrp-zero-*`): 23/23 iguais ao hardware no filtro `zero`.
- C gerado atualizado com a mesma transformação (1530 linhas; 3 restantes são do `fxam`, corretas).

### Confirmação
Build -O2 com a correção (`build/lab/pc/runs/fix-fdiv/`, 06:49): no vídeo ainda há quadros a meio da passada com os
pés longe da sombra e outros com os pés no chão — o mesmo grau de ambiguidade de antes. **O teste visual não confirma
nem descarta.** Falta uma medida numérica (altura do Raiden × altura do chão lida da memória do jogo) ou o mesmo teste
na versão 360/PC original como referência.

### Ainda por fazer
- Repetir o x86diff nas 5187 funções float × 3, lados ARM (-O0, -O2) incluídos.
- Ver as outras 44 diferenças: maioria artefacto do pré-carregamento x87 da ferramenta; `sub_01440166` (sinal do NaN:
  x86 7FC00000, lift FFC00000) e `sub_00DA6880` por analisar.
- Medir fps de novo (3 × 30 s) com a correção: não feito.

## Commits (pc/perf-665, sem push)
- `8c17305` lab: relógio do guest, limite de fps, estatística/skip por PS, lista de modos, tempo de GPU por passe no DXVK
- `247f2eb` device-profile: HL_PIN
- `f395b50` x86diff
- `e8322c7` pcrecomp 0018 + difftest
- PCPorts `2e16abf`: PROBLEMAS, DIARIO, DECISOES, gargalos (lição: validação de float precisa de entradas reais)
