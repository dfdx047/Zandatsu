# optxbox analyze: /tmp/tmp.VvEiu2qLz0/SwordMan-autotest-20261022-135307/rp5-base-logs/capture/20261022-045846.dfc

Frames 0,1 | perfil: Dragon Ball Z Budokai 3 HD (DBZ3-Android) | custo de um fim de passe a 1280x720: 0.50 ms (mali: medido no Mali-G57 (DBZ3, 2026-10-14)) | tempo por draw: sim

## Resumo por frame

| draws | resolves | passes (min) | trocas de RT | configs de RT | draws de ecra inteiro |
|---:|---:|---:|---:|---:|---:|
| 1580.5 | 21.0 | 30.0 | 17.0 | 11.0 | 3.0 |

## O que tentar (ordenado por ms estimados)

| # | achado | ms/f est. | passes/f poupados | ms/f captura | interruptores |
|---:|---|---:|---:|---:|---|
| 1 | Trocas entre configuracoes de RT que partilham EDRAM (transferencias no Xenia) (`rt_aliasing`) | 7.54 | 0.00 | - | `dbz3_rt_direct = true` |
| 2 | Clear() do XDK como retangulo 4x MSAA de meia largura (`A_clear_msaa`) | 4.88 | 7.00 | 0.49 | `dbz3_clear_to_1x = true` |
| 3 | Custo base dos resolves (cada um fecha o passe e passa por compute no Xenia) (`resolve_cost`) | 1.38 | 0.00 | 141.79 | `dbz3_direct_resolve_texture = 1`, `dbz3_direct_resolve_skip_memory = true`, `dbz3_resolve_early_transition = true` |
| 4 | Resolves sobrescritos sem leitor (`D_unread_resolve`) | 1.08 | 2.00 | 20.64 | `dbz3_rt_elide_restores = true` |
| 5 | Restauro da profundidade desenhado como cor (color base = depth base) (`B_depth_restore_as_color`) | 0.50 | 1.00 | 2.81 | `dbz3_skip_depth_restore = true` |

ms est. = fins de passe poupados × custo por fim de passe (escala com a area do alvo); os achados nao somam (removem por vezes os mesmos passes). ms captura = soma do tempo por draw (inflacionado).

## Achados

### rt_aliasing — Trocas entre configuracoes de RT que partilham EDRAM (transferencias no Xenia)

16.00 ocorrencias/frame, 0.00 fins de passe poupados/frame, ~7.54 ms/frame estimados, tempo na captura - ms/frame, confianca alta.

- frame 0: p640 4x C@0 D@0 -> p1280 1x C@1328 D@0 (draw 1)
- frame 0: p1280 1x C@1328 D@0 -> p320 1x C@1328 D@0 (draw 3)
- frame 0: p320 1x C@1328 D@0 -> p160 4x C@1328 D@0 (draw 4)
- frame 0: p160 4x C@1328 D@0 -> p320 1x C@1328 D@0 (draw 5)
- frame 0: p320 1x C@1328 D@0 -> p1280 1x C@1328 D@0 (draw 46)
- frame 0: p1280 1x C@1328 D@0 -> p1040 1x C@0 D@1024 (draw 479)
- _note_: Com render targets diretos estas trocas deixam de copiar dados; os fins de passe ficam (sao mudancas reais de alvo). Estimativa: cada troca custa uma transferencia ~ um fim de passe do alvo novo.

### A_clear_msaa — Clear() do XDK como retangulo 4x MSAA de meia largura

6.00 ocorrencias/frame, 7.00 fins de passe poupados/frame, ~4.88 ms/frame estimados, tempo na captura 0.49 ms/frame, confianca alta.

- frame 0 draws 0: hash XDK p640 4x C@0 D@0, so prof, seguido de 1x pitch 1280: nao
- frame 0 draws 4: hash XDK p160 4x C@1328 D@0, cor+prof, seguido de 1x pitch 320: sim
- frame 0 draws 480: hash XDK p520 4x C@0 D@1024, so prof, seguido de 1x pitch 1040: nao
- frame 0 draws 543: hash XDK p520 4x C@0 D@1024, so prof, seguido de 1x pitch 1040: nao
- frame 0 draws 725: hash XDK p640 4x C@1328 D@1328, so prof, seguido de 1x pitch 640: nao
- frame 0 draws 776: hash XDK p640 4x C@1328 D@1328, so prof, seguido de 1x pitch 1280: nao
- _dbz3_result_: transfer 1,6 -> 0,12 ms (Adreno 650), 8,7 -> 0,8 ms (Mali-G57)
- _ps_: A4A965C189287B99
- _vs_: 1E6883FCCDE1F688

### resolve_cost — Custo base dos resolves (cada um fecha o passe e passa por compute no Xenia)

21.00 ocorrencias/frame, 0.00 fins de passe poupados/frame, ~1.38 ms/frame estimados, tempo na captura 141.79 ms/frame, confianca media.

- 21.0 resolves/frame, 10.0 deles >= meio ecra
- _note_: Estimativa: 1/4 do custo de um fim de passe por resolve (o resto so desaparece eliminando o resolve: ver D/C). No Adreno 650 com dbz3_resolve_debug os ~8 ms de "resolve" ficaram: o custo e a quebra de passe, nao o compute.

### D_unread_resolve — Resolves sobrescritos sem leitor

2.00 ocorrencias/frame, 2.00 fins de passe poupados/frame, ~1.08 ms/frame estimados, tempo na captura 20.64 ms/frame, confianca alta.

- frame 0 resolve (draw 541) depth 1024x1024 -> 0AA46000, sobrescrito no frame 1
- frame 0 resolve (draw 724) depth 1024x1024 -> 0AE46000, sobrescrito no frame 1
- frame 0 resolve (draw 1401) color0 1280x720 -> 0DC36000, sobrescrito no frame 0
- frame 1 resolve (draw 3002) color0 1280x720 -> 0D89E000, sobrescrito no frame 1

### B_depth_restore_as_color — Restauro da profundidade desenhado como cor (color base = depth base)

1.00 ocorrencias/frame, 1.00 fins de passe poupados/frame, ~0.50 ms/frame estimados, tempo na captura 2.81 ms/frame, confianca media.

- frame 0 draws 1450,1451,1452,1453,1454,1455,1456,1457,1458,1459,1460,1461,1462,1463,1464,1465,1466,1467,1468,1469,1470,1471,1472,1473,1474,1475,1476,1477,1478,1479,1480,1481,1482,1483,1484,1485,1486,1487,1488,1489,1490,1491,1492,1493,1494,1495,1496,1497,1498,1499,1500,1501,1502,1503,1504,1505,1506,1507,1508,1509,1510,1511,1512,1513,1514,1515,1516,1517,1518,1519,1520,1521,1522,1523,1524,1525,1526,1527,1528,1529,1530,1531,1532,1533,1534,1535,1536,1537,1538,1539,1540,1541,1542,1543,1544,1545,1546,1547,1548,1549,1550,1551,1552,1553,1554,1555,1556,1557,1558,1559,1560,1561,1562,1563,1564,1565,1566,1567,1568,1569,1570,1571,1572,1573,1574,1575,1576,1577,1578,1579,1580,1581,1582,1583,1584,1585,1586,1587,1588,1589,1590,1591,1592,1593,1594,1595,1596,1597,1598,1599,1600: p1280 1x C@0 D@0 PS F854BF3B7BF7AA9D, le copia de profundidade: nao, poupa 1 passe(s)
- frame 1 draws 3051,3052,3053,3054,3055,3056,3057,3058,3059,3060,3061,3062,3063,3064,3065,3066,3067,3068,3069,3070,3071,3072,3073,3074,3075,3076,3077,3078,3079,3080,3081,3082,3083,3084,3085,3086,3087,3088,3089,3090,3091,3092,3093,3094,3095,3096,3097,3098,3099,3100,3101,3102,3103,3104,3105,3106,3107,3108,3109,3110,3111,3112,3113,3114,3115,3116,3117,3118,3119,3120,3121,3122,3123,3124,3125,3126,3127,3128,3129,3130,3131,3132,3133,3134,3135,3136,3137,3138,3139,3140,3141,3142,3143,3144,3145,3146,3147,3148,3149,3150,3151,3152,3153,3154,3155,3156,3157,3158,3159,3160,3161,3162,3163,3164,3165,3166,3167,3168,3169,3170,3171,3172,3173,3174,3175,3176,3177,3178,3179,3180,3181,3182,3183,3184,3185,3186,3187,3188,3189,3190,3191,3192,3193,3194,3195,3196,3197,3198,3199,3200,3201: p1280 1x C@0 D@0 PS F854BF3B7BF7AA9D, le copia de profundidade: nao, poupa 1 passe(s)
- _dbz3_result_: fins de passe 37 -> 28/f; Adreno 650 56 -> 59-60 fps
- _groups_reading_depth_copy_per_frame_: 0.0
- _note_: Inutil com render targets diretos: o passe intermedio nao toca na imagem da profundidade.

### D_restore_only_resolve — Resolves lidos so por restauros (B/C)

1.50 ocorrencias/frame, 1.50 fins de passe poupados/frame, ~0.28 ms/frame estimados, tempo na captura 1.09 ms/frame, confianca alta.

- frame 0 resolve (draw 1424) color0 320x176 -> 0E785000, leitores: draws 1450
- frame 1 resolve (draw 3025) color0 320x176 -> 0E785000, leitores: draws 3051
- frame 1 resolve (draw 3050) color0 1280x720 -> 0D89E000, leitores: draws 3051

### D_no_reader_in_capture — Resolves sem leitor dentro da captura (pode ser lido pelo CPU ou mais tarde)

2.00 ocorrencias/frame, 0.00 fins de passe poupados/frame, ~0.00 ms/frame estimados, tempo na captura - ms/frame, confianca baixa.

- frame 0 resolve (draw 2) color0 1280x720 -> 0B671000
- frame 1 resolve (draw 1604) color0 1280x720 -> 0B670000
- frame 1 resolve (draw 2143) depth 1024x1024 -> 0AA46000
- frame 1 resolve (draw 2326) depth 1024x1024 -> 0AE46000

## Resolves por tamanho (por frame)

| tamanho | por frame |
|---|---:|
| 320x176 color0 | 10.0 |
| 1280x720 color0 | 7.0 |
| 1024x1024 depth | 2.0 |
| 1280x720 depth | 1.0 |
| 256x256 color0 | 1.0 |

## Pixel shaders (top 12 por draws)

| PS | draws/f | ms/f (captura) | tfetch | efeito |
|---|---:|---:|---:|---|
| E104A745F271970A | 428.0 | 5.80 | 1 |  |
| 28AEC10B4A0E14FF | 240.0 | 3.74 | 1 |  |
| A4A965C189287B99 | 105.0 | 0.57 | 0 |  |
| A48490965E7C834D | 74.0 | 1.49 | 3 |  |
| 59B2332CE973997A | 54.0 | 0.67 | 2 |  |
| 06AF22B469A121E4 | 51.0 | 0.93 | 1 |  |
| E060C0FFE9EE118C | 41.0 | 0.78 | 2 |  |
| E664840CC5314C1D | 41.0 | 0.45 | 3 |  |
| 9AD669DECC901755 | 36.0 | 0.99 | 3 |  |
| 483647F90A97963D | 27.0 | 0.53 | 3 |  |
| 758982A45D868AA8 | 24.0 | 0.49 | 3 |  |
| C9769A4005A60F77 | 21.0 | 0.22 | 2 |  |

## Configuracoes de RT

| RT | draws/f |
|---|---:|
| pitch=1280 msaa=1x color@1328 8_8_8_8 depth@0 D24FS8 | 648.5 |
| pitch=1280 msaa=1x color@1328 32_FLOAT depth@0 D24FS8 | 431.0 |
| pitch=1040 msaa=1x color@1024 32_FLOAT depth@1024 D24FS8 | 240.0 |
| pitch=1280 msaa=1x color@0 8_8_8_8 depth@0 D24FS8 | 151.0 |
| pitch=320 msaa=1x color@1328 8_8_8_8 depth@0 D24FS8 | 41.0 |
| pitch=320 msaa=1x color@1024 8_8_8_8 depth@0 D24FS8 | 37.0 |
| pitch=640 msaa=4x color@1328 8_8_8_8 depth@1328 D24S8 | 26.0 |
| pitch=520 msaa=4x color@0 8_8_8_8 depth@1024 D24FS8 | 2.0 |
| pitch=1040 msaa=1x color@0 8_8_8_8 depth@1024 D24FS8 | 2.0 |
| pitch=160 msaa=4x color@1328 8_8_8_8 depth@0 D24FS8 | 1.0 |
| pitch=640 msaa=4x color@0 8_8_8_8 depth@0 D24FS8 | 1.0 |
