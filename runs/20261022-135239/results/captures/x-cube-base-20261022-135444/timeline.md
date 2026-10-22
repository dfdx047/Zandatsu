# Linha do tempo: /tmp/tmp.VvEiu2qLz0/x-cube-base/logs/capture/20261022-135444.dfc

104 shaders, 191.8 MB de memoria capturada, tempo por draw: sim. dbz3 capture frames=2 resolve_readback=1

Legenda: `P` passe (draws seguidos no mesmo RT), `R` resolve, `S` swap. Etiquetas: A/B/C/D padroes (docs/en/PATTERNS.md, docs/pt-BR/PADROES.md), D* = resolve lido so por restauros, fx:* efeito conhecido. ms = tempo por draw da captura (cada draw no seu passe: inflacionado, serve para comparar).

## Frame 0 — 613 draws, 22 resolves, 28 passes (minimo), 14 trocas de RT

```text
P1   p640 4x C@0 D@0             1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.92  @draw 0 [A]
P2   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.29  @draw 1
 R   color0 1280x720  -> 0B66E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum
P3   p1280 1x C@1328 D@0       155 draws  vs=7 ps=2 tex=16  scissor 0,0-1280,720  ms 89.28  @draw 3
 R   depth  1280x720  -> 0B246000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 159
P4   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.28  @draw 159
 R   color0 1280x720  -> 0CA3E000 32_FLOAT de p1280 1x C@1328 D@0  leitores: 249,396,397,416,417,418…(+23)
P5   p1040 1x C@0 D@1024         1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 1.44  @draw 161
P6   p520 4x C@0 D@1024          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.65  @draw 162 [A]
P7   p1040 1x C@1024 D@1024     19 draws  vs=3 ps=1 tex=0  scissor 0,0-1024,1024  ms 10.72  @draw 163
 R   depth  1024x1024 -> 0AA46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: nenhum [D]
P8   p1040 1x C@0 D@1024         1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 1.50  @draw 183
P9   p520 4x C@0 D@1024          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.65  @draw 184 [A]
P10  p1040 1x C@1024 D@1024     38 draws  vs=3 ps=1 tex=0  scissor 0,0-1024,1024  ms 20.62  @draw 185
 R   depth  1024x1024 -> 0AE46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: nenhum [D]
P11  p640 4x C@1328 D@1328      25 draws  vs=2 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.55  @draw 224 [A]
P12  p1280 1x C@1328 D@0        25 draws  vs=2 ps=2 tex=1  scissor 0,0-1280,720  ms 2.55  @draw 249
 R   color0 1280x720  -> 0BBDE000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 276,277,278,279,280,281…(+151)
P13  p640 4x C@1328 D@1328       1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 1.07  @draw 275 [A]
P14  p1280 1x C@1328 D@0       118 draws  vs=15 ps=25 tex=67  scissor 0,0-1280,720  ms 69.48  @draw 276
 R   color0 1280x720  -> 0CDD6000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 395,451,453,454,455,456…(+2)
P15  p1280 1x C@1328 D@0        36 draws  vs=3 ps=4 tex=6  scissor 0,0-1280,720  ms 19.62  @draw 395
 R   color0 1280x720  -> 0D506000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 432
P16  p1280 1x C@1328 D@0        79 draws  vs=13 ps=15 tex=37  scissor 0,0-1280,720  ms 49.75  @draw 432
 R   color0 1280x720  -> 0D16E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 1085,1087
 R   color0 1280x720  -> 0DC36000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum [D]
P17  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.27  @draw 513
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 515
P18  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 515
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 517
P19  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 517
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 519,525,526,532
P20  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.52  @draw 519
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 521
P21  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 521
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 523
P22  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 523
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 533
P23  p320 1x C@1024 D@0          2 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.31  @draw 525
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 528
P24  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 528
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 530
P25  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 530
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 534
P26  p320 1x C@1024 D@0          3 draws  vs=1 ps=1 tex=3  scissor 0,0-320,176  ms 0.29  @draw 532
 R   color0  320x176  -> 0E785000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 561 [D*]
P27  p320 1x C@1024 D@0         24 draws  vs=1 ps=1 tex=0  scissor 0,0-16,16  ms 0.00  @draw 536
 R   color0 1280x720  -> 0DC36000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 561 [D*]
P28  p1280 1x C@0 D@0           73 draws  vs=6 ps=6 tex=9  scissor 0,0-1280,720  ms 11.26  @draw 561 [B]
 R   color0 1280x720  -> 0E366000 8_8_8_8  de p1280 1x C@0 D@0  leitores: swap
 S   swap frontbuffer=0E366000 1280x720
```

## Frame 1 — 612 draws, 22 resolves, 28 passes (minimo), 14 trocas de RT

```text
P1   p640 4x C@0 D@0             1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 1.05  @draw 635 [A]
P2   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.87  @draw 636
 R   color0 1280x720  -> 0B670000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum
P3   p1280 1x C@1328 D@0       155 draws  vs=7 ps=2 tex=16  scissor 0,0-1280,720  ms 90.54  @draw 638
 R   depth  1280x720  -> 0B246000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 794
P4   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.45  @draw 794
 R   color0 1280x720  -> 0CA3E000 32_FLOAT de p1280 1x C@1328 D@0  leitores: 884,1031,1032,1051,1052,1053…(+23)
P5   p1040 1x C@0 D@1024         1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.90  @draw 796
P6   p520 4x C@0 D@1024          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 1.15  @draw 797 [A]
P7   p1040 1x C@1024 D@1024     19 draws  vs=3 ps=1 tex=0  scissor 0,0-1024,1024  ms 10.49  @draw 798
 R   depth  1024x1024 -> 0AA46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: nenhum
P8   p1040 1x C@0 D@1024         1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.87  @draw 818
P9   p520 4x C@0 D@1024          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.59  @draw 819 [A]
P10  p1040 1x C@1024 D@1024     38 draws  vs=3 ps=1 tex=0  scissor 0,0-1024,1024  ms 21.14  @draw 820
 R   depth  1024x1024 -> 0AE46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: nenhum
P11  p640 4x C@1328 D@1328      25 draws  vs=2 ps=1 tex=0  scissor 0,0-8192,8192  ms 1.38  @draw 859 [A]
P12  p1280 1x C@1328 D@0        25 draws  vs=2 ps=2 tex=1  scissor 0,0-1280,720  ms 2.03  @draw 884
 R   color0 1280x720  -> 0BBDE000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 911,912,913,914,915,916…(+151)
P13  p640 4x C@1328 D@1328       1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 1.15  @draw 910 [A]
P14  p1280 1x C@1328 D@0       118 draws  vs=15 ps=25 tex=67  scissor 0,0-1280,720  ms 68.25  @draw 911
 R   color0 1280x720  -> 0CDD6000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 1030,1089,1090,1091,1092,1093…(+2)
P15  p1280 1x C@1328 D@0        36 draws  vs=3 ps=4 tex=6  scissor 0,0-1280,720  ms 19.34  @draw 1030
 R   color0 1280x720  -> 0D506000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 1067
P16  p1280 1x C@1328 D@0        78 draws  vs=13 ps=15 tex=35  scissor 0,0-1280,720  ms 43.73  @draw 1067
 R   color0 1280x720  -> 0D16E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum
 R   color0 1280x720  -> 0DC36000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum [D]
P17  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.34  @draw 1147
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1149
P18  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 1149
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1151
P19  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 1151
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1153,1159,1160,1166
P20  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.19  @draw 1153
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1155
P21  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 1155
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1157
P22  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 1157
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1167
P23  p320 1x C@1024 D@0          2 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.34  @draw 1159
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1162
P24  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 1162
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1164
P25  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 1164
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1168
P26  p320 1x C@1024 D@0          3 draws  vs=1 ps=1 tex=3  scissor 0,0-320,176  ms 0.41  @draw 1166
 R   color0  320x176  -> 0E785000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1195 [D*]
P27  p320 1x C@1024 D@0         24 draws  vs=1 ps=1 tex=0  scissor 0,0-16,16  ms 0.00  @draw 1170
 R   color0 1280x720  -> 0DC36000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 1195 [D*]
P28  p1280 1x C@0 D@0           73 draws  vs=6 ps=6 tex=9  scissor 0,0-1280,720  ms 8.18  @draw 1195 [B]
 R   color0 1280x720  -> 0E366000 8_8_8_8  de p1280 1x C@0 D@0  leitores: swap
 S   swap frontbuffer=0E366000 1280x720
```
