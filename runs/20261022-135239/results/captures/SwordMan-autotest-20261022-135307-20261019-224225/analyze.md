# optxbox analyze: /tmp/tmp.VvEiu2qLz0/SwordMan-autotest-20261022-135307/rp5-base-logs/capture/20261019-224225.dfc

Frames 0,1 | perfil: Dragon Ball Z Budokai 3 HD (DBZ3-Android) | custo de um fim de passe a 1280x720: 0.50 ms (mali: medido no Mali-G57 (DBZ3, 2026-10-14)) | tempo por draw: sim

## Resumo por frame

| draws | resolves | passes (min) | trocas de RT | configs de RT | draws de ecra inteiro |
|---:|---:|---:|---:|---:|---:|
| 594.0 | 23.0 | 27.0 | 13.0 | 9.0 | 4.0 |

## O que tentar (ordenado por ms estimados)

| # | achado | ms/f est. | passes/f poupados | ms/f captura | interruptores |
|---:|---|---:|---:|---:|---|
| 1 | Trocas entre configuracoes de RT que partilham EDRAM (transferencias no Xenia) (`rt_aliasing`) | 6.33 | 0.00 | - | `dbz3_rt_direct = true` |
| 2 | Clear() do XDK como retangulo 4x MSAA de meia largura (`A_clear_msaa`) | 3.81 | 4.00 | 0.37 | `dbz3_clear_to_1x = true` |
| 3 | Custo base dos resolves (cada um fecha o passe e passa por compute no Xenia) (`resolve_cost`) | 1.74 | 0.00 | 103.48 | `dbz3_direct_resolve_texture = 1`, `dbz3_direct_resolve_skip_memory = true`, `dbz3_resolve_early_transition = true` |
| 4 | Restauro da profundidade desenhado como cor (color base = depth base) (`B_depth_restore_as_color`) | 1.50 | 3.00 | 4.70 | `dbz3_skip_depth_restore = true` |
| 5 | Resolves lidos so por restauros (B/C) (`D_restore_only_resolve`) | 1.28 | 3.50 | 14.68 | `dbz3_rt_elide_restores = true` |

ms est. = fins de passe poupados × custo por fim de passe (escala com a area do alvo); os achados nao somam (removem por vezes os mesmos passes). ms captura = soma do tempo por draw (inflacionado).

## Achados

### rt_aliasing — Trocas entre configuracoes de RT que partilham EDRAM (transferencias no Xenia)

12.00 ocorrencias/frame, 0.00 fins de passe poupados/frame, ~6.33 ms/frame estimados, tempo na captura - ms/frame, confianca alta.

- frame 0: p640 4x C@0 D@0 -> p1280 1x C@1328 D@0 (draw 1)
- frame 0: p1280 1x C@1328 D@0 -> p1280 1x C@1328 D@0 (draw 3)
- frame 0: p1280 1x C@1328 D@0 -> p1040 1x C@0 D@1024 (draw 35)
- frame 0: p1040 1x C@0 D@1024 -> p520 4x C@0 D@1024 (draw 36)
- frame 0: p520 4x C@0 D@1024 -> p1040 1x C@1024 D@1024 (draw 37)
- frame 0: p640 4x C@1328 D@1328 -> p1280 1x C@1328 D@0 (draw 88)
- _note_: Com render targets diretos estas trocas deixam de copiar dados; os fins de passe ficam (sao mudancas reais de alvo). Estimativa: cada troca custa uma transferencia ~ um fim de passe do alvo novo.

### A_clear_msaa — Clear() do XDK como retangulo 4x MSAA de meia largura

4.00 ocorrencias/frame, 4.00 fins de passe poupados/frame, ~3.81 ms/frame estimados, tempo na captura 0.37 ms/frame, confianca alta.

- frame 0 draws 0: hash XDK p640 4x C@0 D@0, so prof, seguido de 1x pitch 1280: nao
- frame 0 draws 36: hash XDK p520 4x C@0 D@1024, so prof, seguido de 1x pitch 1040: nao
- frame 0 draws 63: hash XDK p640 4x C@1328 D@1328, so prof, seguido de 1x pitch 640: nao
- frame 0 draws 114: hash XDK p640 4x C@1328 D@1328, so prof, seguido de 1x pitch 1280: nao
- frame 1 draws 617: hash XDK p640 4x C@0 D@0, so prof, seguido de 1x pitch 1280: nao
- frame 1 draws 653: hash XDK p520 4x C@0 D@1024, so prof, seguido de 1x pitch 1040: nao
- _dbz3_result_: transfer 1,6 -> 0,12 ms (Adreno 650), 8,7 -> 0,8 ms (Mali-G57)
- _ps_: A4A965C189287B99
- _vs_: 1E6883FCCDE1F688

### resolve_cost — Custo base dos resolves (cada um fecha o passe e passa por compute no Xenia)

23.00 ocorrencias/frame, 0.00 fins de passe poupados/frame, ~1.74 ms/frame estimados, tempo na captura 103.48 ms/frame, confianca media.

- 23.0 resolves/frame, 13.0 deles >= meio ecra
- _note_: Estimativa: 1/4 do custo de um fim de passe por resolve (o resto so desaparece eliminando o resolve: ver D/C). No Adreno 650 com dbz3_resolve_debug os ~8 ms de "resolve" ficaram: o custo e a quebra de passe, nao o compute.

### B_depth_restore_as_color — Restauro da profundidade desenhado como cor (color base = depth base)

3.00 ocorrencias/frame, 3.00 fins de passe poupados/frame, ~1.50 ms/frame estimados, tempo na captura 4.70 ms/frame, confianca media.

- frame 0 draws 460,461,462,463,464,465,466,467,468,469,470,471,472,473,474,475,476,477,478,479,480,481,482,483,484,485,486,487,488,489,490,491,492,493,494,495,496,497,498: p1280 1x C@0 D@0 PS F854BF3B7BF7AA9D, le copia de profundidade: nao, poupa 1 passe(s)
- frame 0 draws 502,503: p1280 1x C@0 D@0 PS F4F164727712FDF5, le copia de profundidade: nao, poupa 1 passe(s)
- frame 0 draws 505,506,507,508,509,510,511,512,513,514,515,516,517,518,519,520,521,522,523,524,525,526,527,528,529,530,531,532,533,534,535,536,537,538,539,540,541,542,543,544,545,546,547,548,549,550,551,552,553,554,555,556,557,558,559,560,561,562,563,564,565,566,567,568,569,570,571,572,573,574,575,576,577,578,579,580,581,582,583,584,585,586,587,588,589,590,591,592,593,594,595,596,597,598,599,600,601,602,603,604,605,606,607,608,609,610,611,612,613,614,615: p1280 1x C@0 D@0 PS 01A62A46B2F996DA, le copia de profundidade: nao, poupa 1 passe(s)
- frame 1 draws 1077,1078,1079,1080,1081,1082,1083,1084,1085,1086,1087,1088,1089,1090,1091,1092,1093,1094,1095,1096,1097,1098,1099,1100,1101,1102,1103,1104,1105,1106,1107,1108,1109,1110,1111,1112,1113,1114,1115: p1280 1x C@0 D@0 PS F854BF3B7BF7AA9D, le copia de profundidade: nao, poupa 1 passe(s)
- frame 1 draws 1119,1120: p1280 1x C@0 D@0 PS F4F164727712FDF5, le copia de profundidade: nao, poupa 1 passe(s)
- frame 1 draws 1122,1123,1124,1125,1126,1127,1128,1129,1130,1131,1132,1133,1134,1135,1136,1137,1138,1139,1140,1141,1142,1143,1144,1145,1146,1147,1148,1149,1150,1151,1152,1153,1154,1155,1156,1157,1158,1159,1160,1161,1162,1163,1164,1165,1166,1167,1168,1169,1170,1171,1172,1173,1174,1175,1176,1177,1178,1179,1180,1181,1182,1183,1184,1185,1186,1187,1188,1189,1190,1191,1192,1193,1194,1195,1196,1197,1198,1199,1200,1201,1202,1203,1204,1205,1206,1207,1208,1209,1210,1211,1212,1213,1214,1215,1216,1217,1218,1219,1220,1221,1222,1223,1224,1225,1226,1227,1228,1229,1230,1231,1232: p1280 1x C@0 D@0 PS 01A62A46B2F996DA, le copia de profundidade: nao, poupa 1 passe(s)
- _dbz3_result_: fins de passe 37 -> 28/f; Adreno 650 56 -> 59-60 fps
- _groups_reading_depth_copy_per_frame_: 0.0
- _note_: Inutil com render targets diretos: o passe intermedio nao toca na imagem da profundidade.

### D_restore_only_resolve — Resolves lidos so por restauros (B/C)

3.50 ocorrencias/frame, 3.50 fins de passe poupados/frame, ~1.28 ms/frame estimados, tempo na captura 14.68 ms/frame, confianca alta.

- frame 0 resolve (draw 434) color0 320x176 -> 0E785000, leitores: draws 460
- frame 0 resolve (draw 501) color0 1280x720 -> 0C30E000, leitores: draws 503
- frame 0 resolve (draw 504) color0 1280x720 -> 0C30E000, leitores: draws 505
- frame 1 resolve (draw 1051) color0 320x176 -> 0E785000, leitores: draws 1077
- frame 1 resolve (draw 1076) color0 1280x720 -> 0D89E000, leitores: draws 1077
- frame 1 resolve (draw 1118) color0 1280x720 -> 0C30E000, leitores: draws 1120

### D_unread_resolve — Resolves sobrescritos sem leitor

1.50 ocorrencias/frame, 1.50 fins de passe poupados/frame, ~0.79 ms/frame estimados, tempo na captura 12.23 ms/frame, confianca alta.

- frame 0 resolve (draw 62) depth 1024x1024 -> 0AE46000, sobrescrito no frame 1
- frame 0 resolve (draw 411) color0 1280x720 -> 0DC36000, sobrescrito no frame 0
- frame 1 resolve (draw 1028) color0 1280x720 -> 0D89E000, sobrescrito no frame 1

### D_no_reader_in_capture — Resolves sem leitor dentro da captura (pode ser lido pelo CPU ou mais tarde)

1.50 ocorrencias/frame, 0.00 fins de passe poupados/frame, ~0.00 ms/frame estimados, tempo na captura - ms/frame, confianca baixa.

- frame 0 resolve (draw 2) color0 1280x720 -> 0B671000
- frame 1 resolve (draw 619) color0 1280x720 -> 0B670000
- frame 1 resolve (draw 679) depth 1024x1024 -> 0AE46000

## Resolves por tamanho (por frame)

| tamanho | por frame |
|---|---:|
| 1280x720 color0 | 10.0 |
| 320x176 color0 | 10.0 |
| 1024x1024 depth | 2.0 |
| 1280x720 depth | 1.0 |

## Pixel shaders (top 12 por draws)

| PS | draws/f | ms/f (captura) | tfetch | efeito |
|---|---:|---:|---:|---|
| FA9269B5806DD167 | 154.0 | 3.21 | 1 |  |
| A4A965C189287B99 | 124.0 | 1.41 | 0 |  |
| 2CE21E05F9680D34 | 96.0 | 2.81 | 2 |  |
| 59B2332CE973997A | 50.0 | 0.67 | 2 |  |
| E060C0FFE9EE118C | 37.0 | 0.90 | 2 |  |
| E104A745F271970A | 24.0 | 0.88 | 1 |  |
| AF038115631EBD16 | 18.0 | 0.30 | 4 |  |
| 75F9DF2732797388 | 15.0 | 0.19 | 4 |  |
| DAFD35EBE60D9860 | 12.0 | 3.16 | 4 |  |
| 06AF22B469A121E4 | 10.0 | 0.12 | 1 |  |
| 11C7657530951A90 | 6.0 | 0.12 | 44 |  |
| A755AD0AC2470588 | 4.0 | 0.14 | 2 |  |

## Configuracoes de RT

| RT | draws/f |
|---|---:|
| pitch=1280 msaa=1x color@1328 8_8_8_8 depth@0 D24FS8 | 322.0 |
| pitch=1280 msaa=1x color@0 8_8_8_8 depth@0 D24FS8 | 152.0 |
| pitch=320 msaa=1x color@1024 8_8_8_8 depth@0 D24FS8 | 37.0 |
| pitch=1280 msaa=1x color@1328 32_FLOAT depth@0 D24FS8 | 30.0 |
| pitch=640 msaa=4x color@1328 8_8_8_8 depth@1328 D24S8 | 26.0 |
| pitch=1040 msaa=1x color@1024 32_FLOAT depth@1024 D24FS8 | 24.0 |
| pitch=520 msaa=4x color@0 8_8_8_8 depth@1024 D24FS8 | 1.0 |
| pitch=640 msaa=4x color@0 8_8_8_8 depth@0 D24FS8 | 1.0 |
| pitch=1040 msaa=1x color@0 8_8_8_8 depth@1024 D24FS8 | 1.0 |
