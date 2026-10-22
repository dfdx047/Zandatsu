# optxbox analyze: /tmp/tmp.VvEiu2qLz0/SwordMan-autotest-20261022-135307/rp5-base-logs/capture/20261019-231005.dfc

Frames 0,1 | perfil: Dragon Ball Z Budokai 3 HD (DBZ3-Android) | custo de um fim de passe a 1280x720: 0.50 ms (mali: medido no Mali-G57 (DBZ3, 2026-10-14)) | tempo por draw: sim

## Resumo por frame

| draws | resolves | passes (min) | trocas de RT | configs de RT | draws de ecra inteiro |
|---:|---:|---:|---:|---:|---:|
| 650.5 | 23.0 | 27.0 | 13.0 | 9.0 | 4.0 |

## O que tentar (ordenado por ms estimados)

| # | achado | ms/f est. | passes/f poupados | ms/f captura | interruptores |
|---:|---|---:|---:|---:|---|
| 1 | Trocas entre configuracoes de RT que partilham EDRAM (transferencias no Xenia) (`rt_aliasing`) | 6.33 | 0.00 | - | `dbz3_rt_direct = true` |
| 2 | Clear() do XDK como retangulo 4x MSAA de meia largura (`A_clear_msaa`) | 3.81 | 4.00 | 0.37 | `dbz3_clear_to_1x = true` |
| 3 | Custo base dos resolves (cada um fecha o passe e passa por compute no Xenia) (`resolve_cost`) | 1.74 | 0.00 | 100.48 | `dbz3_direct_resolve_texture = 1`, `dbz3_direct_resolve_skip_memory = true`, `dbz3_resolve_early_transition = true` |
| 4 | Restauro da profundidade desenhado como cor (color base = depth base) (`B_depth_restore_as_color`) | 1.50 | 3.00 | 4.32 | `dbz3_skip_depth_restore = true` |
| 5 | Resolves lidos so por restauros (B/C) (`D_restore_only_resolve`) | 1.28 | 3.50 | 14.21 | `dbz3_rt_elide_restores = true` |

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
- frame 1 draws 674: hash XDK p640 4x C@0 D@0, so prof, seguido de 1x pitch 1280: nao
- frame 1 draws 710: hash XDK p520 4x C@0 D@1024, so prof, seguido de 1x pitch 1040: nao
- _dbz3_result_: transfer 1,6 -> 0,12 ms (Adreno 650), 8,7 -> 0,8 ms (Mali-G57)
- _ps_: A4A965C189287B99
- _vs_: 1E6883FCCDE1F688

### resolve_cost — Custo base dos resolves (cada um fecha o passe e passa por compute no Xenia)

23.00 ocorrencias/frame, 0.00 fins de passe poupados/frame, ~1.74 ms/frame estimados, tempo na captura 100.48 ms/frame, confianca media.

- 23.0 resolves/frame, 13.0 deles >= meio ecra
- _note_: Estimativa: 1/4 do custo de um fim de passe por resolve (o resto so desaparece eliminando o resolve: ver D/C). No Adreno 650 com dbz3_resolve_debug os ~8 ms de "resolve" ficaram: o custo e a quebra de passe, nao o compute.

### B_depth_restore_as_color — Restauro da profundidade desenhado como cor (color base = depth base)

3.00 ocorrencias/frame, 3.00 fins de passe poupados/frame, ~1.50 ms/frame estimados, tempo na captura 4.32 ms/frame, confianca media.

- frame 0 draws 517,518,519,520,521,522,523,524,525,526,527,528,529,530,531,532,533,534,535,536,537,538,539,540,541,542,543,544,545,546,547,548,549,550,551,552,553,554,555: p1280 1x C@0 D@0 PS F854BF3B7BF7AA9D, le copia de profundidade: nao, poupa 1 passe(s)
- frame 0 draws 559,560: p1280 1x C@0 D@0 PS F4F164727712FDF5, le copia de profundidade: nao, poupa 1 passe(s)
- frame 0 draws 562,563,564,565,566,567,568,569,570,571,572,573,574,575,576,577,578,579,580,581,582,583,584,585,586,587,588,589,590,591,592,593,594,595,596,597,598,599,600,601,602,603,604,605,606,607,608,609,610,611,612,613,614,615,616,617,618,619,620,621,622,623,624,625,626,627,628,629,630,631,632,633,634,635,636,637,638,639,640,641,642,643,644,645,646,647,648,649,650,651,652,653,654,655,656,657,658,659,660,661,662,663,664,665,666,667,668,669,670,671,672: p1280 1x C@0 D@0 PS 01A62A46B2F996DA, le copia de profundidade: nao, poupa 1 passe(s)
- frame 1 draws 1190,1191,1192,1193,1194,1195,1196,1197,1198,1199,1200,1201,1202,1203,1204,1205,1206,1207,1208,1209,1210,1211,1212,1213,1214,1215,1216,1217,1218,1219,1220,1221,1222,1223,1224,1225,1226,1227,1228: p1280 1x C@0 D@0 PS F854BF3B7BF7AA9D, le copia de profundidade: nao, poupa 1 passe(s)
- frame 1 draws 1232,1233: p1280 1x C@0 D@0 PS F4F164727712FDF5, le copia de profundidade: nao, poupa 1 passe(s)
- frame 1 draws 1235,1236,1237,1238,1239,1240,1241,1242,1243,1244,1245,1246,1247,1248,1249,1250,1251,1252,1253,1254,1255,1256,1257,1258,1259,1260,1261,1262,1263,1264,1265,1266,1267,1268,1269,1270,1271,1272,1273,1274,1275,1276,1277,1278,1279,1280,1281,1282,1283,1284,1285,1286,1287,1288,1289,1290,1291,1292,1293,1294,1295,1296,1297,1298,1299,1300,1301,1302,1303,1304,1305,1306,1307,1308,1309,1310,1311,1312,1313,1314,1315,1316,1317,1318,1319,1320,1321,1322,1323,1324,1325,1326,1327,1328,1329,1330,1331,1332,1333,1334,1335,1336,1337,1338,1339,1340,1341,1342,1343,1344,1345: p1280 1x C@0 D@0 PS 01A62A46B2F996DA, le copia de profundidade: nao, poupa 1 passe(s)
- _dbz3_result_: fins de passe 37 -> 28/f; Adreno 650 56 -> 59-60 fps
- _groups_reading_depth_copy_per_frame_: 0.0
- _note_: Inutil com render targets diretos: o passe intermedio nao toca na imagem da profundidade.

### D_restore_only_resolve — Resolves lidos so por restauros (B/C)

3.50 ocorrencias/frame, 3.50 fins de passe poupados/frame, ~1.28 ms/frame estimados, tempo na captura 14.21 ms/frame, confianca alta.

- frame 0 resolve (draw 491) color0 320x176 -> 0E785000, leitores: draws 517
- frame 0 resolve (draw 558) color0 1280x720 -> 0C30E000, leitores: draws 560
- frame 0 resolve (draw 561) color0 1280x720 -> 0C30E000, leitores: draws 562
- frame 1 resolve (draw 1164) color0 320x176 -> 0E785000, leitores: draws 1190
- frame 1 resolve (draw 1189) color0 1280x720 -> 0D89E000, leitores: draws 1190
- frame 1 resolve (draw 1231) color0 1280x720 -> 0C30E000, leitores: draws 1233

### D_unread_resolve — Resolves sobrescritos sem leitor

1.50 ocorrencias/frame, 1.50 fins de passe poupados/frame, ~0.79 ms/frame estimados, tempo na captura 12.31 ms/frame, confianca alta.

- frame 0 resolve (draw 62) depth 1024x1024 -> 0AE46000, sobrescrito no frame 1
- frame 0 resolve (draw 468) color0 1280x720 -> 0DC36000, sobrescrito no frame 0
- frame 1 resolve (draw 1141) color0 1280x720 -> 0D89E000, sobrescrito no frame 1

### D_no_reader_in_capture — Resolves sem leitor dentro da captura (pode ser lido pelo CPU ou mais tarde)

1.50 ocorrencias/frame, 0.00 fins de passe poupados/frame, ~0.00 ms/frame estimados, tempo na captura - ms/frame, confianca baixa.

- frame 0 resolve (draw 2) color0 1280x720 -> 0B670000
- frame 1 resolve (draw 676) color0 1280x720 -> 0B66F000
- frame 1 resolve (draw 736) depth 1024x1024 -> 0AE46000

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
| FA9269B5806DD167 | 156.0 | 2.90 | 1 |  |
| A4A965C189287B99 | 124.0 | 1.03 | 0 |  |
| 2CE21E05F9680D34 | 100.5 | 2.96 | 2 |  |
| 59B2332CE973997A | 50.0 | 0.67 | 2 |  |
| 7ABDFEFBCADB4BF9 | 48.0 | 3.10 | 1 |  |
| E060C0FFE9EE118C | 37.0 | 0.90 | 2 |  |
| E104A745F271970A | 27.0 | 0.97 | 1 |  |
| AF038115631EBD16 | 18.0 | 0.30 | 4 |  |
| 75F9DF2732797388 | 15.0 | 0.19 | 4 |  |
| DAFD35EBE60D9860 | 12.0 | 4.28 | 4 |  |
| 06AF22B469A121E4 | 11.0 | 0.27 | 1 |  |
| A755AD0AC2470588 | 4.0 | 0.14 | 2 |  |

## Configuracoes de RT

| RT | draws/f |
|---|---:|
| pitch=1280 msaa=1x color@1328 8_8_8_8 depth@0 D24FS8 | 378.5 |
| pitch=1280 msaa=1x color@0 8_8_8_8 depth@0 D24FS8 | 152.0 |
| pitch=320 msaa=1x color@1024 8_8_8_8 depth@0 D24FS8 | 37.0 |
| pitch=1280 msaa=1x color@1328 32_FLOAT depth@0 D24FS8 | 30.0 |
| pitch=640 msaa=4x color@1328 8_8_8_8 depth@1328 D24S8 | 26.0 |
| pitch=1040 msaa=1x color@1024 32_FLOAT depth@1024 D24FS8 | 24.0 |
| pitch=520 msaa=4x color@0 8_8_8_8 depth@1024 D24FS8 | 1.0 |
| pitch=640 msaa=4x color@0 8_8_8_8 depth@0 D24FS8 | 1.0 |
| pitch=1040 msaa=1x color@0 8_8_8_8 depth@1024 D24FS8 | 1.0 |
