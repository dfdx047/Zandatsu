# MGR PC — manhã de 21/11/2026 (07:40–08:25, Lisboa)

Resposta ao vídeo de 07:36 (Odin 2 Mini). Branch `pc/perf-665` (com push).

| Item | Estado | Resultado |
|---|---|---|
| Desempenho ruim | causa achada | O Odin estava com o perfil **sd480p** da bancada (`tools/device-profile.sh`, desde 20/11 07:51, com o laço root `hl-keep` a repor os limites): GPU 124,8 MHz de 692, núcleos médios 1,78 de 2,8 GHz. O vídeo foi gravado assim. O helper `hlprofile` já não está instalado, por isso o `restore` não corre: **reiniciar o Odin** limpa (o laço não arranca no boot). Depois do boot, o adb por Wi-Fi precisa de `adb tcpip 5555` outra vez |
| Personagens a flutuar | causa achada | Sombra desligada no GraphicOption (índice 3 = 0 desde 19/11): sem sombra dinâmica nada toca o chão. Com sombra = 1 voltam as sombras (op-shadow: 26,5 → 25,7 fps, GPU 75 → 90%) |
| Opções da launcher sem efeito | corrigido na app nova | O GraphicOption não mudava desde 19/11 20:14. A app nova grava, lê de volta e mostra "Gravado e verificado" (testado: FILE no cartão SD). Os fps não mudavam com a resolução porque, com o perfil sd480p, a luta fica a ~27 fps em qualquer resolução (960x540 27,3; nativa 26,5) |
| 60 fps congela | não reproduzido | Limite 60 / 30 / 90 em luta: 26,5 / 25,9 / 28,0 fps, 0 crashes, 0 stalls (`op-cap*`). Fps do jogo acima de 60 precisa de destravar a lógica do jogo (está fixa em 60): pendência |
| App só de PC | feita | Módulo `pcapp/` (`com.dfdx047.zandatsu.pc`, "Zandatsu PC", 59 MB); a app do 360 não muda. Separadores Jogo / Gráficos / Controles / Sistema / Sobre; perfis, slider de resolução 25–100% (passos de 5%), fps 30/45/60/90/120, FSR 1/CAS + nitidez, geração 30→60, opções do jogo, driver, Multithread, idioma |
| Toque com os dedos | feito | `MGR_TOUCH_MODE`: Botões / Gestos / Mouse. Gestos: esquerda arrasta = stick, toque = A; direita toque = X, longo = Y, arrasta = câmara; dois dedos seguros = Modo Katana, toque com dois = B, três = START. Testado no título: toque = A, arrasto = cursor |

## Pendências

- Reiniciar o Odin e medir a luta sem o perfil (os números de 20–21/11 são todos com o perfil sd480p).
- Toque A no título pode contar duas vezes (passou do "Press any key" direto para a lista de saves).
- Testar Gestos/Mouse numa luta (multitoque não dá por `adb input`).
- Fps acima de 60 e FSR 3: a lógica do MGR está presa a 60; sem isso 90/120 só limitam.
- Menu dentro do jogo na app nova: só Continuar / Editar controles / Sair.

## Commits

`289b0ae` modos de toque · `c78e6dd` app `pcapp` · `1a93ec7` merge.
