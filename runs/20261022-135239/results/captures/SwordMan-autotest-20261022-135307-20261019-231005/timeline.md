# Linha do tempo: /tmp/tmp.VvEiu2qLz0/SwordMan-autotest-20261022-135307/rp5-base-logs/capture/20261019-231005.dfc

78 shaders, 178.3 MB de memoria capturada, tempo por draw: sim. dbz3 capture frames=2 resolve_readback=1

Legenda: `P` passe (draws seguidos no mesmo RT), `R` resolve, `S` swap. Etiquetas: A/B/C/D padroes (docs/en/PATTERNS.md, docs/pt-BR/PADROES.md), D* = resolve lido so por restauros, fx:* efeito conhecido. ms = tempo por draw da captura (cada draw no seu passe: inflacionado, serve para comparar).

## Frame 0 — 651 draws, 23 resolves, 27 passes (minimo), 13 trocas de RT

```text
P1   p640 4x C@0 D@0             1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.13  @draw 0 [A]
P2   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.03  @draw 1
 R   color0 1280x720  -> 0B670000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum
P3   p1280 1x C@1328 D@0        29 draws  vs=5 ps=2 tex=4  scissor 0,0-1280,720  ms 0.95  @draw 3
 R   depth  1280x720  -> 0B246000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 33
P4   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 1.26  @draw 33
 R   color0 1280x720  -> 0CA3E000 32_FLOAT de p1280 1x C@1328 D@0  leitores: 88,142,143,144,145,146…(+99)
P5   p1040 1x C@0 D@1024         1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.02  @draw 35
P6   p520 4x C@0 D@1024          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.07  @draw 36 [A]
P7   p1040 1x C@1024 D@1024     24 draws  vs=3 ps=2 tex=1  scissor 0,0-1024,1024  ms 0.58  @draw 37
 R   depth  1024x1024 -> 0AA46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: 88
 R   depth  1024x1024 -> 0AE46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: nenhum [D]
P8   p640 4x C@1328 D@1328      25 draws  vs=2 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 63 [A]
P9   p1280 1x C@1328 D@0        25 draws  vs=2 ps=2 tex=2  scissor 0,0-1280,720  ms 4.03  @draw 88
 R   color0 1280x720  -> 0BBDE000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 119,120,121,122,125,127…(+11)
P10  p640 4x C@1328 D@1328       1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 114 [A]
P11  p1280 1x C@1328 D@0        26 draws  vs=8 ps=9 tex=29  scissor 0,0-1280,720  ms 5.32  @draw 115
 R   color0 1280x720  -> 0CDD6000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 415,416,417
P12  p1280 1x C@1328 D@0       326 draws  vs=9 ps=10 tex=30  scissor 0,0-1280,720  ms 9.75  @draw 142
 R   color0 1280x720  -> 0DC36000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum [D]
P13  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.07  @draw 469
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 471
P14  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.03  @draw 471
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 473
P15  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.04  @draw 473
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 475,481,482,488
P16  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.02  @draw 475
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 477
P17  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.02  @draw 477
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 479
P18  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 479
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 489
P19  p320 1x C@1024 D@0          2 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.03  @draw 481
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 484
P20  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 484
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 486
P21  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 486
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 490
P22  p320 1x C@1024 D@0          3 draws  vs=1 ps=1 tex=3  scissor 0,0-320,176  ms 0.05  @draw 488
 R   color0  320x176  -> 0E785000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 517 [D*]
P23  p320 1x C@1024 D@0         24 draws  vs=1 ps=1 tex=0  scissor 0,0-16,16  ms 0.00  @draw 492
 R   color0 1280x720  -> 0DC36000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 517,675,677,678,1142
P24  p1280 1x C@0 D@0           39 draws  vs=3 ps=3 tex=8  scissor 0,0-1280,720  ms 1.21  @draw 517 [B]
 R   color0 1280x720  -> 0D506000 8_8_8_8  de p1280 1x C@0 D@0  leitores: 557,559
P25  p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.79  @draw 557
 R   color0 1280x720  -> 0C30E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 560 [D*]
P26  p1280 1x C@0 D@0            2 draws  vs=2 ps=2 tex=2  scissor 0,0-1280,720  ms 0.97  @draw 559 [B]
 R   color0 1280x720  -> 0C30E000 8_8_8_8  de p1280 1x C@0 D@0  leitores: 562 [D*]
P27  p1280 1x C@0 D@0          111 draws  vs=7 ps=7 tex=8  scissor 0,0-1280,720  ms 1.91  @draw 562 [B]
 R   color0 1280x720  -> 0E366000 8_8_8_8  de p1280 1x C@0 D@0  leitores: swap
 S   swap frontbuffer=0E366000 1280x720
```

## Frame 1 — 650 draws, 23 resolves, 27 passes (minimo), 13 trocas de RT

```text
P1   p640 4x C@0 D@0             1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.15  @draw 674 [A]
P2   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.03  @draw 675
 R   color0 1280x720  -> 0B66F000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum
P3   p1280 1x C@1328 D@0        29 draws  vs=5 ps=2 tex=4  scissor 0,0-1280,720  ms 1.07  @draw 677
 R   depth  1280x720  -> 0B246000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 707
P4   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 1.30  @draw 707
 R   color0 1280x720  -> 0CA3E000 32_FLOAT de p1280 1x C@1328 D@0  leitores: 762,816,817,818,819,820…(+98)
P5   p1040 1x C@0 D@1024         1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.04  @draw 709
P6   p520 4x C@0 D@1024          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 710 [A]
P7   p1040 1x C@1024 D@1024     24 draws  vs=3 ps=2 tex=1  scissor 0,0-1024,1024  ms 0.67  @draw 711
 R   depth  1024x1024 -> 0AA46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: 762
 R   depth  1024x1024 -> 0AE46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: nenhum
P8   p640 4x C@1328 D@1328      25 draws  vs=2 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 737 [A]
P9   p1280 1x C@1328 D@0        25 draws  vs=2 ps=2 tex=2  scissor 0,0-1280,720  ms 4.27  @draw 762
 R   color0 1280x720  -> 0BBDE000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 793,794,795,796,799,801…(+11)
P10  p640 4x C@1328 D@1328       1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 788 [A]
P11  p1280 1x C@1328 D@0        26 draws  vs=8 ps=9 tex=29  scissor 0,0-1280,720  ms 4.73  @draw 789
 R   color0 1280x720  -> 0CDD6000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 1088,1089,1090
P12  p1280 1x C@1328 D@0       325 draws  vs=9 ps=10 tex=30  scissor 0,0-1280,720  ms 11.70  @draw 816
 R   color0 1280x720  -> 0D89E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum [D]
P13  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.11  @draw 1142
 R   color0  320x176  -> 0B68A000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1144
P14  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.03  @draw 1144
 R   color0  320x176  -> 0B68A000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1146
P15  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.04  @draw 1146
 R   color0  320x176  -> 0B68A000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1148,1154,1155,1161
P16  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.02  @draw 1148
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1150
P17  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 1150
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1152
P18  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 1152
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1162
P19  p320 1x C@1024 D@0          2 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.03  @draw 1154
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1157
P20  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 1157
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1159
P21  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 1159
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1163
P22  p320 1x C@1024 D@0          3 draws  vs=1 ps=1 tex=3  scissor 0,0-320,176  ms 0.06  @draw 1161
 R   color0  320x176  -> 0E785000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1190 [D*]
P23  p320 1x C@1024 D@0         24 draws  vs=1 ps=1 tex=0  scissor 0,0-16,16  ms 0.00  @draw 1165
 R   color0 1280x720  -> 0D89E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 1190 [D*]
P24  p1280 1x C@0 D@0           39 draws  vs=3 ps=3 tex=8  scissor 0,0-1280,720  ms 1.33  @draw 1190 [B]
 R   color0 1280x720  -> 0D506000 8_8_8_8  de p1280 1x C@0 D@0  leitores: 1230,1232
P25  p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.86  @draw 1230
 R   color0 1280x720  -> 0C30E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 1233 [D*]
P26  p1280 1x C@0 D@0            2 draws  vs=2 ps=2 tex=2  scissor 0,0-1280,720  ms 1.07  @draw 1232 [B]
 R   color0 1280x720  -> 0C30E000 8_8_8_8  de p1280 1x C@0 D@0  leitores: 1235 [D*]
P27  p1280 1x C@0 D@0          111 draws  vs=7 ps=7 tex=8  scissor 0,0-1280,720  ms 2.16  @draw 1235 [B]
 R   color0 1280x720  -> 0DFCE000 8_8_8_8  de p1280 1x C@0 D@0  leitores: swap
 S   swap frontbuffer=0DFCE000 1280x720
```
