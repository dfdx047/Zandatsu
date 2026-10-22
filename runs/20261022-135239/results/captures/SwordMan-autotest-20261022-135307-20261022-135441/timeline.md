# Linha do tempo: /tmp/tmp.VvEiu2qLz0/SwordMan-autotest-20261022-135307/rp5-base-logs/capture/20261022-135441.dfc

151 shaders, 253.1 MB de memoria capturada, tempo por draw: sim. dbz3 capture frames=2 resolve_readback=1

Legenda: `P` passe (draws seguidos no mesmo RT), `R` resolve, `S` swap. Etiquetas: A/B/C/D padroes (docs/en/PATTERNS.md, docs/pt-BR/PADROES.md), D* = resolve lido so por restauros, fx:* efeito conhecido. ms = tempo por draw da captura (cada draw no seu passe: inflacionado, serve para comparar).

## Frame 0 — 1930 draws, 23 resolves, 32 passes (minimo), 17 trocas de RT

```text
P1   p640 4x C@0 D@0             1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.15  @draw 0 [A]
P2   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.03  @draw 1
 R   color0 1280x720  -> 0B671000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum
P3   p320 1x C@1328 D@0          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.02  @draw 3
P4   p160 4x C@1328 D@0          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.02  @draw 4 [A]
P5   p320 1x C@1328 D@0         66 draws  vs=1 ps=1 tex=1  scissor 0,0-256,256  ms 1.06  @draw 5
 R   color0  256x256  -> 0E704000 8_8_8_8  de p320 1x C@1328 D@0  leitores: 1743
P6   p1280 1x C@1328 D@0       550 draws  vs=6 ps=2 tex=25  scissor 0,0-1280,720  ms 7.75  @draw 72
 R   depth  1280x720  -> 0B246000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 623
P7   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.18  @draw 623
 R   color0 1280x720  -> 0CA3E000 32_FLOAT de p1280 1x C@1328 D@0  leitores: 816,1363,1367,1368,1369,1370…(+163)
P8   p1040 1x C@0 D@1024         1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.03  @draw 625
P9   p520 4x C@0 D@1024          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 626 [A]
P10  p1040 1x C@1024 D@1024     51 draws  vs=3 ps=2 tex=1  scissor 0,0-1024,1024  ms 0.77  @draw 627
 R   depth  1024x1024 -> 0AA46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: nenhum [D]
P11  p1040 1x C@0 D@1024         1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.03  @draw 679
P12  p520 4x C@0 D@1024          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 680 [A]
P13  p1040 1x C@1024 D@1024    109 draws  vs=5 ps=1 tex=6  scissor 0,0-1024,1024  ms 1.54  @draw 681
 R   depth  1024x1024 -> 0AE46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: nenhum [D]
P14  p640 4x C@1328 D@1328      25 draws  vs=2 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.09  @draw 791 [A]
P15  p1280 1x C@1328 D@0        25 draws  vs=2 ps=2 tex=1  scissor 0,0-1280,720  ms 2.07  @draw 816
 R   color0 1280x720  -> 0BBDE000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 843,844,845,846,847,848…(+550)
P16  p640 4x C@1328 D@1328       1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.09  @draw 842 [A]
P17  p1280 1x C@1328 D@0       519 draws  vs=22 ps=50 tex=197  scissor 0,0-1280,720  ms 16.91  @draw 843
 R   color0 1280x720  -> 0CDD6000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 1363,1364,1365,1378,1401,1661…(+5)
P18  p1280 1x C@1328 D@0       147 draws  vs=10 ps=19 tex=69  scissor 0,0-1280,720  ms 2.92  @draw 1363
 R   color0 1280x720  -> 0D506000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 1511
P19  p1280 1x C@1328 D@0       229 draws  vs=12 ps=21 tex=68  scissor 0,0-1280,720  ms 7.37  @draw 1511
 R   color0 1280x720  -> 0D16E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 3588
P20  p1280 1x C@1328 D@0         3 draws  vs=1 ps=2 tex=2  scissor 0,0-1280,720  ms 0.27  @draw 1741
 R   color0 1280x720  -> 0DC36000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum [D]
P21  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.12  @draw 1745
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1747
P22  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 1747
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1749
P23  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 1749
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1751,1757,1758,1764
P24  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.03  @draw 1751
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1753
P25  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 1753
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1755
P26  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 1755
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1765
P27  p320 1x C@1024 D@0          2 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.03  @draw 1757
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1760
P28  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 1760
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1762
P29  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 1762
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1766
P30  p320 1x C@1024 D@0          3 draws  vs=1 ps=1 tex=3  scissor 0,0-320,176  ms 0.05  @draw 1764
 R   color0  320x176  -> 0E785000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1793 [D*]
P31  p320 1x C@1024 D@0         24 draws  vs=1 ps=1 tex=0  scissor 0,0-16,16  ms 0.00  @draw 1768
 R   color0 1280x720  -> 0DC36000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 1793,1954,3684
P32  p1280 1x C@0 D@0          159 draws  vs=6 ps=6 tex=12  scissor 0,0-1280,720  ms 2.88  @draw 1793 [B]
 R   color0 1280x720  -> 0E366000 8_8_8_8  de p1280 1x C@0 D@0  leitores: swap
 S   swap frontbuffer=0E366000 1280x720
```

## Frame 1 — 1916 draws, 23 resolves, 32 passes (minimo), 17 trocas de RT

```text
P1   p640 4x C@0 D@0             1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.15  @draw 1953 [A]
P2   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.03  @draw 1954
 R   color0 1280x720  -> 0B670000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum
P3   p320 1x C@1328 D@0          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.01  @draw 1956
P4   p160 4x C@1328 D@0          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.02  @draw 1957 [A]
P5   p320 1x C@1328 D@0         66 draws  vs=1 ps=1 tex=1  scissor 0,0-256,256  ms 1.07  @draw 1958
 R   color0  256x256  -> 0E704000 8_8_8_8  de p320 1x C@1328 D@0  leitores: 3682
P6   p1280 1x C@1328 D@0       550 draws  vs=6 ps=2 tex=25  scissor 0,0-1280,720  ms 7.73  @draw 2025
 R   depth  1280x720  -> 0B246000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 2576
P7   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.15  @draw 2576
 R   color0 1280x720  -> 0CA3E000 32_FLOAT de p1280 1x C@1328 D@0  leitores: 2769,3316,3320,3321,3322,3323…(+163)
P8   p1040 1x C@0 D@1024         1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.02  @draw 2578
P9   p520 4x C@0 D@1024          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 2579 [A]
P10  p1040 1x C@1024 D@1024     51 draws  vs=3 ps=2 tex=1  scissor 0,0-1024,1024  ms 2.05  @draw 2580
 R   depth  1024x1024 -> 0AA46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: nenhum
P11  p1040 1x C@0 D@1024         1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.02  @draw 2632
P12  p520 4x C@0 D@1024          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 2633 [A]
P13  p1040 1x C@1024 D@1024    109 draws  vs=5 ps=1 tex=6  scissor 0,0-1024,1024  ms 1.55  @draw 2634
 R   depth  1024x1024 -> 0AE46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: nenhum
P14  p640 4x C@1328 D@1328      25 draws  vs=2 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.09  @draw 2744 [A]
P15  p1280 1x C@1328 D@0        25 draws  vs=2 ps=2 tex=1  scissor 0,0-1280,720  ms 1.99  @draw 2769
 R   color0 1280x720  -> 0BBDE000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 2796,2797,2798,2799,2800,2801…(+542)
P16  p640 4x C@1328 D@1328       1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 2795 [A]
P17  p1280 1x C@1328 D@0       519 draws  vs=22 ps=50 tex=197  scissor 0,0-1280,720  ms 13.66  @draw 2796
 R   color0 1280x720  -> 0CDD6000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 3316,3317,3318,3331,3357,3593…(+5)
P18  p1280 1x C@1328 D@0       140 draws  vs=9 ps=18 tex=71  scissor 0,0-1280,720  ms 2.70  @draw 3316
 R   color0 1280x720  -> 0D506000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 3457
P19  p1280 1x C@1328 D@0       222 draws  vs=11 ps=20 tex=63  scissor 0,0-1280,720  ms 5.84  @draw 3457
 R   color0 1280x720  -> 0D16E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum
P20  p1280 1x C@1328 D@0         3 draws  vs=1 ps=2 tex=2  scissor 0,0-1280,720  ms 0.25  @draw 3680
 R   color0 1280x720  -> 0D89E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum [D]
P21  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.09  @draw 3684
 R   color0  320x176  -> 0B68A000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3686
P22  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 3686
 R   color0  320x176  -> 0B68A000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3688
P23  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 3688
 R   color0  320x176  -> 0B68A000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3690,3696,3697,3703
P24  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.02  @draw 3690
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3692
P25  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 3692
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3694
P26  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 3694
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3704
P27  p320 1x C@1024 D@0          2 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.03  @draw 3696
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3699
P28  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 3699
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3701
P29  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.00  @draw 3701
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3705
P30  p320 1x C@1024 D@0          3 draws  vs=1 ps=1 tex=3  scissor 0,0-320,176  ms 0.05  @draw 3703
 R   color0  320x176  -> 0E785000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 3732 [D*]
P31  p320 1x C@1024 D@0         24 draws  vs=1 ps=1 tex=0  scissor 0,0-16,16  ms 0.00  @draw 3707
 R   color0 1280x720  -> 0D89E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 3732 [D*]
P32  p1280 1x C@0 D@0          159 draws  vs=6 ps=6 tex=12  scissor 0,0-1280,720  ms 2.87  @draw 3732 [B]
 R   color0 1280x720  -> 0DFCE000 8_8_8_8  de p1280 1x C@0 D@0  leitores: swap
 S   swap frontbuffer=0DFCE000 1280x720
```
