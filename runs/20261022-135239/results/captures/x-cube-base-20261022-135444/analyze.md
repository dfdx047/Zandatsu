# optxbox analyze: /tmp/tmp.VvEiu2qLz0/x-cube-base/logs/capture/20261022-135444.dfc

Frames 0,1 | perfil: Dragon Ball Z Budokai 3 HD (DBZ3-Android) | custo de um fim de passe a 1280x720: 0.50 ms (mali: medido no Mali-G57 (DBZ3, 2026-10-14)) | tempo por draw: sim

## Resumo por frame

| draws | resolves | passes (min) | trocas de RT | configs de RT | draws de ecra inteiro |
|---:|---:|---:|---:|---:|---:|
| 612.5 | 22.0 | 28.0 | 14.0 | 9.0 | 3.0 |

## O que tentar (ordenado por ms estimados)

| # | achado | ms/f est. | passes/f poupados | ms/f captura | interruptores |
|---:|---|---:|---:|---:|---|
| 1 | Trocas entre configuracoes de RT que partilham EDRAM (transferencias no Xenia) (`rt_aliasing`) | 7.12 | 0.00 | - | `dbz3_rt_direct = true` |
| 2 | Clear() do XDK como retangulo 4x MSAA de meia largura (`A_clear_msaa`) | 4.62 | 5.00 | 4.58 | `dbz3_clear_to_1x = true` |
| 3 | Custo base dos resolves (cada um fecha o passe e passa por compute no Xenia) (`resolve_cost`) | 1.62 | 0.00 | 184.83 | `dbz3_direct_resolve_texture = 1`, `dbz3_direct_resolve_skip_memory = true`, `dbz3_resolve_early_transition = true` |
| 4 | Resolves sobrescritos sem leitor (`D_unread_resolve`) | 1.08 | 2.00 | 28.89 | `dbz3_rt_elide_restores = true` |
| 5 | Restauro da profundidade desenhado como cor (color base = depth base) (`B_depth_restore_as_color`) | 0.50 | 1.00 | 9.72 | `dbz3_skip_depth_restore = true` |

ms est. = fins de passe poupados × custo por fim de passe (escala com a area do alvo); os achados nao somam (removem por vezes os mesmos passes). ms captura = soma do tempo por draw (inflacionado).

## Achados

### rt_aliasing — Trocas entre configuracoes de RT que partilham EDRAM (transferencias no Xenia)

13.00 ocorrencias/frame, 0.00 fins de passe poupados/frame, ~7.12 ms/frame estimados, tempo na captura - ms/frame, confianca alta.

- frame 0: p640 4x C@0 D@0 -> p1280 1x C@1328 D@0 (draw 1)
- frame 0: p1280 1x C@1328 D@0 -> p1280 1x C@1328 D@0 (draw 3)
- frame 0: p1280 1x C@1328 D@0 -> p1040 1x C@0 D@1024 (draw 161)
- frame 0: p1040 1x C@0 D@1024 -> p520 4x C@0 D@1024 (draw 162)
- frame 0: p520 4x C@0 D@1024 -> p1040 1x C@1024 D@1024 (draw 163)
- frame 0: p1040 1x C@1024 D@1024 -> p1040 1x C@0 D@1024 (draw 183)
- _note_: Com render targets diretos estas trocas deixam de copiar dados; os fins de passe ficam (sao mudancas reais de alvo). Estimativa: cada troca custa uma transferencia ~ um fim de passe do alvo novo.

### A_clear_msaa — Clear() do XDK como retangulo 4x MSAA de meia largura

5.00 ocorrencias/frame, 5.00 fins de passe poupados/frame, ~4.62 ms/frame estimados, tempo na captura 4.58 ms/frame, confianca alta.

- frame 0 draws 0: hash XDK p640 4x C@0 D@0, so prof, seguido de 1x pitch 1280: nao
- frame 0 draws 162: hash XDK p520 4x C@0 D@1024, so prof, seguido de 1x pitch 1040: nao
- frame 0 draws 184: hash XDK p520 4x C@0 D@1024, so prof, seguido de 1x pitch 1040: nao
- frame 0 draws 224: hash XDK p640 4x C@1328 D@1328, so prof, seguido de 1x pitch 640: nao
- frame 0 draws 275: hash XDK p640 4x C@1328 D@1328, so prof, seguido de 1x pitch 1280: nao
- frame 1 draws 635: hash XDK p640 4x C@0 D@0, so prof, seguido de 1x pitch 1280: nao
- _dbz3_result_: transfer 1,6 -> 0,12 ms (Adreno 650), 8,7 -> 0,8 ms (Mali-G57)
- _ps_: A4A965C189287B99
- _vs_: 1E6883FCCDE1F688

### resolve_cost — Custo base dos resolves (cada um fecha o passe e passa por compute no Xenia)

22.00 ocorrencias/frame, 0.00 fins de passe poupados/frame, ~1.62 ms/frame estimados, tempo na captura 184.83 ms/frame, confianca media.

- 22.0 resolves/frame, 12.0 deles >= meio ecra
- _note_: Estimativa: 1/4 do custo de um fim de passe por resolve (o resto so desaparece eliminando o resolve: ver D/C). No Adreno 650 com dbz3_resolve_debug os ~8 ms de "resolve" ficaram: o custo e a quebra de passe, nao o compute.

### D_unread_resolve — Resolves sobrescritos sem leitor

2.00 ocorrencias/frame, 2.00 fins de passe poupados/frame, ~1.08 ms/frame estimados, tempo na captura 28.89 ms/frame, confianca alta.

- frame 0 resolve (draw 182) depth 1024x1024 -> 0AA46000, sobrescrito no frame 1
- frame 0 resolve (draw 223) depth 1024x1024 -> 0AE46000, sobrescrito no frame 1
- frame 0 resolve (draw 512) color0 1280x720 -> 0DC36000, sobrescrito no frame 0
- frame 1 resolve (draw 1146) color0 1280x720 -> 0DC36000, sobrescrito no frame 1

### D_restore_only_resolve — Resolves lidos so por restauros (B/C)

2.00 ocorrencias/frame, 2.00 fins de passe poupados/frame, ~0.53 ms/frame estimados, tempo na captura 17.84 ms/frame, confianca alta.

- frame 0 resolve (draw 535) color0 320x176 -> 0E785000, leitores: draws 561
- frame 0 resolve (draw 560) color0 1280x720 -> 0DC36000, leitores: draws 561
- frame 1 resolve (draw 1169) color0 320x176 -> 0E785000, leitores: draws 1195
- frame 1 resolve (draw 1194) color0 1280x720 -> 0DC36000, leitores: draws 1195

### B_depth_restore_as_color — Restauro da profundidade desenhado como cor (color base = depth base)

1.00 ocorrencias/frame, 1.00 fins de passe poupados/frame, ~0.50 ms/frame estimados, tempo na captura 9.72 ms/frame, confianca media.

- frame 0 draws 561,562,563,564,565,566,567,568,569,570,571,572,573,574,575,576,577,578,579,580,581,582,583,584,585,586,587,588,589,590,591,592,593,594,595,596,597,598,599,600,601,602,603,604,605,606,607,608,609,610,611,612,613,614,615,616,617,618,619,620,621,622,623,624,625,626,627,628,629,630,631,632,633: p1280 1x C@0 D@0 PS F854BF3B7BF7AA9D, le copia de profundidade: nao, poupa 1 passe(s)
- frame 1 draws 1195,1196,1197,1198,1199,1200,1201,1202,1203,1204,1205,1206,1207,1208,1209,1210,1211,1212,1213,1214,1215,1216,1217,1218,1219,1220,1221,1222,1223,1224,1225,1226,1227,1228,1229,1230,1231,1232,1233,1234,1235,1236,1237,1238,1239,1240,1241,1242,1243,1244,1245,1246,1247,1248,1249,1250,1251,1252,1253,1254,1255,1256,1257,1258,1259,1260,1261,1262,1263,1264,1265,1266,1267: p1280 1x C@0 D@0 PS F854BF3B7BF7AA9D, le copia de profundidade: nao, poupa 1 passe(s)
- _dbz3_result_: fins de passe 37 -> 28/f; Adreno 650 56 -> 59-60 fps
- _groups_reading_depth_copy_per_frame_: 0.0
- _note_: Inutil com render targets diretos: o passe intermedio nao toca na imagem da profundidade.

### D_no_reader_in_capture — Resolves sem leitor dentro da captura (pode ser lido pelo CPU ou mais tarde)

2.50 ocorrencias/frame, 0.00 fins de passe poupados/frame, ~0.00 ms/frame estimados, tempo na captura - ms/frame, confianca baixa.

- frame 0 resolve (draw 2) color0 1280x720 -> 0B66E000
- frame 1 resolve (draw 637) color0 1280x720 -> 0B670000
- frame 1 resolve (draw 817) depth 1024x1024 -> 0AA46000
- frame 1 resolve (draw 858) depth 1024x1024 -> 0AE46000
- frame 1 resolve (draw 1145) color0 1280x720 -> 0D16E000

## Resolves por tamanho (por frame)

| tamanho | por frame |
|---|---:|
| 320x176 color0 | 10.0 |
| 1280x720 color0 | 9.0 |
| 1024x1024 depth | 2.0 |
| 1280x720 depth | 1.0 |

## Pixel shaders (top 12 por draws)

| PS | draws/f | ms/f (captura) | tfetch | efeito |
|---|---:|---:|---:|---|
| A4A965C189287B99 | 160.0 | 38.76 | 0 |  |
| E104A745F271970A | 149.5 | 85.58 | 1 |  |
| 59B2332CE973997A | 26.0 | 1.40 | 2 |  |
| C1D2AF15BA1F1EAC | 24.0 | 12.61 | 2 |  |
| 43A64782F4F0ADE7 | 20.0 | 10.70 | 1 |  |
| D47E656630A5D1FD | 19.0 | 9.58 | 4 |  |
| 8B39FDF5486ADD99 | 18.0 | 11.39 | 4 |  |
| 98F7AAEB1C255FEE | 18.0 | 8.98 | 2 |  |
| C590D74C1BA7933C | 14.0 | 7.83 | 2 |  |
| A91E35D4B05AF46F | 12.0 | 8.12 | 4 |  |
| E060C0FFE9EE118C | 9.0 | 2.65 | 2 |  |
| 06AF22B469A121E4 | 8.0 | 5.86 | 1 |  |

## Configuracoes de RT

| RT | draws/f |
|---|---:|
| pitch=1280 msaa=1x color@1328 8_8_8_8 depth@0 D24FS8 | 258.5 |
| pitch=1280 msaa=1x color@1328 32_FLOAT depth@0 D24FS8 | 156.0 |
| pitch=1280 msaa=1x color@0 8_8_8_8 depth@0 D24FS8 | 73.0 |
| pitch=1040 msaa=1x color@1024 32_FLOAT depth@1024 D24FS8 | 57.0 |
| pitch=320 msaa=1x color@1024 8_8_8_8 depth@0 D24FS8 | 37.0 |
| pitch=640 msaa=4x color@1328 8_8_8_8 depth@1328 D24S8 | 26.0 |
| pitch=520 msaa=4x color@0 8_8_8_8 depth@1024 D24FS8 | 2.0 |
| pitch=1040 msaa=1x color@0 8_8_8_8 depth@1024 D24FS8 | 2.0 |
| pitch=640 msaa=4x color@0 8_8_8_8 depth@0 D24FS8 | 1.0 |
