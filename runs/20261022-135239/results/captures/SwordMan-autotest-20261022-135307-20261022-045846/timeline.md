# Linha do tempo: /tmp/tmp.VvEiu2qLz0/SwordMan-autotest-20261022-135307/rp5-base-logs/capture/20261022-045846.dfc

154 shaders, 234.7 MB de memoria capturada, tempo por draw: sim. dbz3 capture frames=2 resolve_readback=1

Legenda: `P` passe (draws seguidos no mesmo RT), `R` resolve, `S` swap. Etiquetas: A/B/C/D padroes (docs/en/PATTERNS.md, docs/pt-BR/PADROES.md), D* = resolve lido so por restauros, fx:* efeito conhecido. ms = tempo por draw da captura (cada draw no seu passe: inflacionado, serve para comparar).

## Frame 0 — 1581 draws, 21 resolves, 30 passes (minimo), 17 trocas de RT

```text
P1   p640 4x C@0 D@0             1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.16  @draw 0 [A]
P2   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.03  @draw 1
 R   color0 1280x720  -> 0B671000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum
P3   p320 1x C@1328 D@0          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.02  @draw 3
P4   p160 4x C@1328 D@0          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.02  @draw 4 [A]
P5   p320 1x C@1328 D@0         40 draws  vs=1 ps=1 tex=1  scissor 0,0-256,256  ms 0.55  @draw 5
 R   color0  256x256  -> 0E704000 8_8_8_8  de p320 1x C@1328 D@0  leitores: 1400
P6   p1280 1x C@1328 D@0       430 draws  vs=6 ps=2 tex=29  scissor 0,0-1280,720  ms 5.52  @draw 46
 R   depth  1280x720  -> 0B246000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 477
P7   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.15  @draw 477
 R   color0 1280x720  -> 0CA3E000 32_FLOAT de p1280 1x C@1328 D@0  leitores: 750,1177,1180,1181,1182,1183…(+102)
P8   p1040 1x C@0 D@1024         1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.03  @draw 479
P9   p520 4x C@0 D@1024          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.07  @draw 480 [A]
P10  p1040 1x C@1024 D@1024     60 draws  vs=4 ps=1 tex=4  scissor 0,0-1024,1024  ms 0.78  @draw 481
 R   depth  1024x1024 -> 0AA46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: nenhum [D]
P11  p1040 1x C@0 D@1024         1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.03  @draw 542
P12  p520 4x C@0 D@1024          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.07  @draw 543 [A]
P13  p1040 1x C@1024 D@1024    180 draws  vs=5 ps=1 tex=8  scissor 0,0-1024,1024  ms 2.25  @draw 544
 R   depth  1024x1024 -> 0AE46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: nenhum [D]
P14  p640 4x C@1328 D@1328      25 draws  vs=2 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 725 [A]
P15  p1280 1x C@1328 D@0        25 draws  vs=2 ps=2 tex=1  scissor 0,0-1280,720  ms 3.04  @draw 750
 R   color0 1280x720  -> 0BBDE000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 777,778,779,780,781,782…(+437)
P16  p640 4x C@1328 D@1328       1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 776 [A]
P17  p1280 1x C@1328 D@0       399 draws  vs=25 ps=54 tex=219  scissor 0,0-1280,720  ms 8.68  @draw 777
 R   color0 1280x720  -> 0CDD6000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 1177,1178,1179,1190,1241,1262…(+13)
P18  p1280 1x C@1328 D@0       224 draws  vs=16 ps=22 tex=101  scissor 0,0-1280,720  ms 3.57  @draw 1177
 R   color0 1280x720  -> 0DC36000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum [D]
P19  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.12  @draw 1402
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1404
P20  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.04  @draw 1404
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1406
P21  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.05  @draw 1406
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1408,1414,1415,1421
P22  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.02  @draw 1408
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1410
P23  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.02  @draw 1410
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1412
P24  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.02  @draw 1412
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1422
P25  p320 1x C@1024 D@0          2 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.03  @draw 1414
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1417
P26  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 1417
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1419
P27  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 1419
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1423
P28  p320 1x C@1024 D@0          3 draws  vs=1 ps=1 tex=3  scissor 0,0-320,176  ms 0.05  @draw 1421
 R   color0  320x176  -> 0E785000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1450 [D*]
P29  p320 1x C@1024 D@0         24 draws  vs=1 ps=1 tex=0  scissor 0,0-16,16  ms 0.00  @draw 1425
 R   color0 1280x720  -> 0DC36000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 1450,1603,3003
P30  p1280 1x C@0 D@0          151 draws  vs=7 ps=7 tex=12  scissor 0,0-1280,720  ms 2.83  @draw 1450 [B]
 R   color0 1280x720  -> 0E366000 8_8_8_8  de p1280 1x C@0 D@0  leitores: swap
 S   swap frontbuffer=0E366000 1280x720
```

## Frame 1 — 1580 draws, 21 resolves, 30 passes (minimo), 17 trocas de RT

```text
P1   p640 4x C@0 D@0             1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.16  @draw 1602 [A]
P2   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.03  @draw 1603
 R   color0 1280x720  -> 0B670000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum
P3   p320 1x C@1328 D@0          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.02  @draw 1605
P4   p160 4x C@1328 D@0          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.02  @draw 1606 [A]
P5   p320 1x C@1328 D@0         40 draws  vs=1 ps=1 tex=1  scissor 0,0-256,256  ms 0.62  @draw 1607
 R   color0  256x256  -> 0E704000 8_8_8_8  de p320 1x C@1328 D@0  leitores: 3001
P6   p1280 1x C@1328 D@0       430 draws  vs=6 ps=2 tex=29  scissor 0,0-1280,720  ms 6.16  @draw 1648
 R   depth  1280x720  -> 0B246000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 2079
P7   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.15  @draw 2079
 R   color0 1280x720  -> 0CA3E000 32_FLOAT de p1280 1x C@1328 D@0  leitores: 2352,2779,2782,2783,2784,2785…(+101)
P8   p1040 1x C@0 D@1024         1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.02  @draw 2081
P9   p520 4x C@0 D@1024          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 2082 [A]
P10  p1040 1x C@1024 D@1024     60 draws  vs=4 ps=1 tex=4  scissor 0,0-1024,1024  ms 0.87  @draw 2083
 R   depth  1024x1024 -> 0AA46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: nenhum
P11  p1040 1x C@0 D@1024         1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.02  @draw 2144
P12  p520 4x C@0 D@1024          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 2145 [A]
P13  p1040 1x C@1024 D@1024    180 draws  vs=5 ps=1 tex=8  scissor 0,0-1024,1024  ms 3.57  @draw 2146
 R   depth  1024x1024 -> 0AE46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: nenhum
P14  p640 4x C@1328 D@1328      25 draws  vs=2 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.09  @draw 2327 [A]
P15  p1280 1x C@1328 D@0        25 draws  vs=2 ps=2 tex=1  scissor 0,0-1280,720  ms 2.13  @draw 2352
 R   color0 1280x720  -> 0BBDE000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 2379,2380,2381,2382,2383,2384…(+437)
P16  p640 4x C@1328 D@1328       1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 2378 [A]
P17  p1280 1x C@1328 D@0       399 draws  vs=25 ps=54 tex=219  scissor 0,0-1280,720  ms 9.62  @draw 2379
 R   color0 1280x720  -> 0CDD6000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 2779,2780,2781,2792,2843,2864…(+13)
P18  p1280 1x C@1328 D@0       223 draws  vs=16 ps=22 tex=97  scissor 0,0-1280,720  ms 4.63  @draw 2779
 R   color0 1280x720  -> 0D89E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum [D]
P19  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.10  @draw 3003
 R   color0  320x176  -> 0B68A000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3005
P20  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.04  @draw 3005
 R   color0  320x176  -> 0B68A000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3007
P21  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.04  @draw 3007
 R   color0  320x176  -> 0B68A000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3009,3015,3016,3022
P22  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.02  @draw 3009
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3011
P23  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 3011
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3013
P24  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 3013
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3023
P25  p320 1x C@1024 D@0          2 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.03  @draw 3015
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3018
P26  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 3018
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3020
P27  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 3020
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3024
P28  p320 1x C@1024 D@0          3 draws  vs=1 ps=1 tex=3  scissor 0,0-320,176  ms 0.05  @draw 3022
 R   color0  320x176  -> 0E785000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3051 [D*]
P29  p320 1x C@1024 D@0         24 draws  vs=1 ps=1 tex=0  scissor 0,0-16,16  ms 0.00  @draw 3026
 R   color0 1280x720  -> 0D89E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 3051 [D*]
P30  p1280 1x C@0 D@0          151 draws  vs=7 ps=7 tex=12  scissor 0,0-1280,720  ms 2.78  @draw 3051 [B]
 R   color0 1280x720  -> 0DFCE000 8_8_8_8  de p1280 1x C@0 D@0  leitores: swap
 S   swap frontbuffer=0DFCE000 1280x720
```
