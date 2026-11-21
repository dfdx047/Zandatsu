# MGR PC — manhã de 21/11/2026 (07:40–08:25, Lisboa)

Resposta ao vídeo de 07:36 (Odin 2 Mini). Branch `pc/perf-665` (com push).

| Item | Estado | Resultado |
|---|---|---|
| Desempenho ruim | corrigido depois | ~~O perfil sd480p foi a causa~~ **Errado:** o perfil sd480p do Odin estava ligado de propósito, para simular um SoC fraco (correção do desenvolvedor, 21/11). Os testes passaram para um SD665 real (Mi A3), ver `MGR_PC_MIA3_2026-11-21.md` |
| Personagens a flutuar | causa achada | Sombra desligada no GraphicOption (índice 3 = 0 desde 19/11): sem sombra dinâmica nada toca o chão. Com sombra = 1 voltam as sombras (op-shadow: 26,5 → 25,7 fps, GPU 75 → 90%) |
| Opções da launcher sem efeito | corrigido na app nova | O GraphicOption não mudava desde 19/11 20:14. A app nova grava, lê de volta e mostra "Gravado e verificado" (testado: FILE no cartão SD). Os fps quase não mudavam com a resolução porque a luta é limitada pela CPU (960x540 27,3; nativa 26,5) |
| 60 fps congela | não reproduzido | Limite 60 / 30 / 90 em luta: 26,5 / 25,9 / 28,0 fps, 0 crashes, 0 stalls (`op-cap*`). Fps do jogo acima de 60 precisa de destravar a lógica do jogo (está fixa em 60): pendência |
| App só de PC | feita | Módulo `pcapp/` (`com.dfdx047.zandatsu.pc`, "Zandatsu PC", 59 MB); a app do 360 não muda. Separadores Jogo / Gráficos / Controles / Sistema / Sobre; perfis, slider de resolução 25–100% (passos de 5%), fps 30/45/60/90/120, FSR 1/CAS + nitidez, geração 30→60, opções do jogo, driver, Multithread, idioma |
| Toque com os dedos | feito | `MGR_TOUCH_MODE`: Botões / Gestos / Mouse. Gestos: esquerda arrasta = stick, toque = A; direita toque = X, longo = Y, arrasta = câmara; dois dedos seguros = Modo Katana, toque com dois = B, três = START. Testado no título: toque = A, arrasto = cursor |

## Pendências

- ~~Reiniciar o Odin e medir sem o perfil~~ (o perfil era intencional; testes seguem no Mi A3).
- Toque A no título pode contar duas vezes (passou do "Press any key" direto para a lista de saves).
- Testar Gestos/Mouse numa luta (multitoque não dá por `adb input`).
- Fps acima de 60 e FSR 3: a lógica do MGR está presa a 60; sem isso 90/120 só limitam.
- Menu dentro do jogo na app nova: só Continuar / Editar controles / Sair.

## Commits

`289b0ae` modos de toque · `c78e6dd` app `pcapp` · `1a93ec7` merge.
