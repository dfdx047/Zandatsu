# Linha do tempo: /tmp/tmp.VvEiu2qLz0/SwordMan-autotest-20261022-135307/rp5-base-logs/capture/20261019-224225.dfc

77 shaders, 178.2 MB de memoria capturada, tempo por draw: sim. dbz3 capture frames=2 resolve_readback=1

Legenda: `P` passe (draws seguidos no mesmo RT), `R` resolve, `S` swap. Etiquetas: A/B/C/D padroes (docs/en/PATTERNS.md, docs/pt-BR/PADROES.md), D* = resolve lido so por restauros, fx:* efeito conhecido. ms = tempo por draw da captura (cada draw no seu passe: inflacionado, serve para comparar).

## Frame 0 — 594 draws, 23 resolves, 27 passes (minimo), 13 trocas de RT

```text
P1   p640 4x C@0 D@0             1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.14  @draw 0 [A]
P2   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.02  @draw 1
 R   color0 1280x720  -> 0B671000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum
P3   p1280 1x C@1328 D@0        29 draws  vs=5 ps=2 tex=4  scissor 0,0-1280,720  ms 0.91  @draw 3
 R   depth  1280x720  -> 0B246000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 33
P4   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 1.13  @draw 33
 R   color0 1280x720  -> 0CA3E000 32_FLOAT de p1280 1x C@1328 D@0  leitores: 88,142,143,144,145,146…(+93)
P5   p1040 1x C@0 D@1024         1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.02  @draw 35
P6   p520 4x C@0 D@1024          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.07  @draw 36 [A]
P7   p1040 1x C@1024 D@1024     24 draws  vs=3 ps=2 tex=1  scissor 0,0-1024,1024  ms 0.58  @draw 37
 R   depth  1024x1024 -> 0AA46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: 88
 R   depth  1024x1024 -> 0AE46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: nenhum [D]
P8   p640 4x C@1328 D@1328      25 draws  vs=2 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 63 [A]
P9   p1280 1x C@1328 D@0        25 draws  vs=2 ps=2 tex=2  scissor 0,0-1280,720  ms 3.45  @draw 88
 R   color0 1280x720  -> 0BBDE000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 119,120,121,122,125,127…(+11)
P10  p640 4x C@1328 D@1328       1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 114 [A]
P11  p1280 1x C@1328 D@0        26 draws  vs=8 ps=9 tex=29  scissor 0,0-1280,720  ms 3.54  @draw 115
 R   color0 1280x720  -> 0CDD6000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 407,408
P12  p1280 1x C@1328 D@0       269 draws  vs=9 ps=9 tex=24  scissor 0,0-1280,720  ms 7.51  @draw 142
 R   color0 1280x720  -> 0DC36000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum [D]
P13  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.08  @draw 412
 R   color0  320x176  -> 0B68A000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 414
P14  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.04  @draw 414
 R   color0  320x176  -> 0B68A000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 416
P15  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.04  @draw 416
 R   color0  320x176  -> 0B68A000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 418,424,425,431
P16  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.02  @draw 418
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 420
P17  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 420
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 422
P18  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.02  @draw 422
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 432
P19  p320 1x C@1024 D@0          2 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.03  @draw 424
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 427
P20  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 427
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 429
P21  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 429
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 433
P22  p320 1x C@1024 D@0          3 draws  vs=1 ps=1 tex=3  scissor 0,0-320,176  ms 0.05  @draw 431
 R   color0  320x176  -> 0E785000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 460 [D*]
P23  p320 1x C@1024 D@0         24 draws  vs=1 ps=1 tex=0  scissor 0,0-16,16  ms 0.00  @draw 435
 R   color0 1280x720  -> 0DC36000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 460,618,620,621,622,623…(+2)
P24  p1280 1x C@0 D@0           39 draws  vs=3 ps=3 tex=8  scissor 0,0-1280,720  ms 1.21  @draw 460 [B]
 R   color0 1280x720  -> 0D506000 8_8_8_8  de p1280 1x C@0 D@0  leitores: 500,502
P25  p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.78  @draw 500
 R   color0 1280x720  -> 0C30E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 503 [D*]
P26  p1280 1x C@0 D@0            2 draws  vs=2 ps=2 tex=2  scissor 0,0-1280,720  ms 0.96  @draw 502 [B]
 R   color0 1280x720  -> 0C30E000 8_8_8_8  de p1280 1x C@0 D@0  leitores: 505 [D*]
P27  p1280 1x C@0 D@0          111 draws  vs=7 ps=7 tex=8  scissor 0,0-1280,720  ms 2.70  @draw 505 [B]
 R   color0 1280x720  -> 0E366000 8_8_8_8  de p1280 1x C@0 D@0  leitores: swap
 S   swap frontbuffer=0E366000 1280x720
```

## Frame 1 — 594 draws, 23 resolves, 27 passes (minimo), 13 trocas de RT

```text
P1   p640 4x C@0 D@0             1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.14  @draw 617 [A]
P2   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.03  @draw 618
 R   color0 1280x720  -> 0B670000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum
P3   p1280 1x C@1328 D@0        29 draws  vs=5 ps=2 tex=4  scissor 0,0-1280,720  ms 1.04  @draw 620
 R   depth  1280x720  -> 0B246000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 650
P4   p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 1.26  @draw 650
 R   color0 1280x720  -> 0CA3E000 32_FLOAT de p1280 1x C@1328 D@0  leitores: 705,759,760,761,762,763…(+93)
P5   p1040 1x C@0 D@1024         1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.02  @draw 652
P6   p520 4x C@0 D@1024          1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 653 [A]
P7   p1040 1x C@1024 D@1024     24 draws  vs=3 ps=2 tex=1  scissor 0,0-1024,1024  ms 0.66  @draw 654
 R   depth  1024x1024 -> 0AA46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: 705
 R   depth  1024x1024 -> 0AE46000 8_8_8_8  de p1040 1x C@1024 D@1024  leitores: nenhum
P8   p640 4x C@1328 D@1328      25 draws  vs=2 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 680 [A]
P9   p1280 1x C@1328 D@0        25 draws  vs=2 ps=2 tex=2  scissor 0,0-1280,720  ms 5.04  @draw 705
 R   color0 1280x720  -> 0BBDE000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 736,737,738,739,742,744…(+11)
P10  p640 4x C@1328 D@1328       1 draws  vs=1 ps=1 tex=0  scissor 0,0-8192,8192  ms 0.08  @draw 731 [A]
P11  p1280 1x C@1328 D@0        26 draws  vs=8 ps=9 tex=29  scissor 0,0-1280,720  ms 4.11  @draw 732
 R   color0 1280x720  -> 0CDD6000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 1024,1025
P12  p1280 1x C@1328 D@0       269 draws  vs=9 ps=9 tex=24  scissor 0,0-1280,720  ms 8.52  @draw 759
 R   color0 1280x720  -> 0D89E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: nenhum [D]
P13  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.11  @draw 1029
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1031
P14  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.03  @draw 1031
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1033
P15  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.04  @draw 1033
 R   color0  320x176  -> 0B6C6000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1035,1041,1042,1048
P16  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.02  @draw 1035
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1037
P17  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 1037
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1039
P18  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 1039
 R   color0  320x176  -> 0B675000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1049
P19  p320 1x C@1024 D@0          2 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.03  @draw 1041
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1044
P20  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 1044
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1046
P21  p320 1x C@1024 D@0          1 draws  vs=1 ps=1 tex=1  scissor 0,0-320,176  ms 0.01  @draw 1046
 R   color0  320x176  -> 0B673000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1050
P22  p320 1x C@1024 D@0          3 draws  vs=1 ps=1 tex=3  scissor 0,0-320,176  ms 0.06  @draw 1048
 R   color0  320x176  -> 0E785000 8_8_8_8  de p320 1x C@1024 D@0  leitores: 1077 [D*]
P23  p320 1x C@1024 D@0         24 draws  vs=1 ps=1 tex=0  scissor 0,0-16,16  ms 0.00  @draw 1052
 R   color0 1280x720  -> 0D89E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 1077 [D*]
P24  p1280 1x C@0 D@0           39 draws  vs=3 ps=3 tex=8  scissor 0,0-1280,720  ms 1.31  @draw 1077 [B]
 R   color0 1280x720  -> 0D506000 8_8_8_8  de p1280 1x C@0 D@0  leitores: 1117,1119
P25  p1280 1x C@1328 D@0         1 draws  vs=1 ps=1 tex=1  scissor 0,0-1280,720  ms 0.86  @draw 1117
 R   color0 1280x720  -> 0C30E000 8_8_8_8  de p1280 1x C@1328 D@0  leitores: 1120 [D*]
P26  p1280 1x C@0 D@0            2 draws  vs=2 ps=2 tex=2  scissor 0,0-1280,720  ms 1.07  @draw 1119 [B]
 R   color0 1280x720  -> 0C30E000 8_8_8_8  de p1280 1x C@0 D@0  leitores: 1122 [D*]
P27  p1280 1x C@0 D@0          111 draws  vs=7 ps=7 tex=8  scissor 0,0-1280,720  ms 2.14  @draw 1122 [B]
 R   color0 1280x720  -> 0DFCE000 8_8_8_8  de p1280 1x C@0 D@0  leitores: swap
 S   swap frontbuffer=0DFCE000 1280x720
```
