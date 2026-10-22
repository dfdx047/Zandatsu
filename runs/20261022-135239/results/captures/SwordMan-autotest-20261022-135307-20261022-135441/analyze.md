# optxbox analyze: /tmp/tmp.VvEiu2qLz0/SwordMan-autotest-20261022-135307/rp5-base-logs/capture/20261022-135441.dfc

Frames 0,1 | perfil: Dragon Ball Z Budokai 3 HD (DBZ3-Android) | custo de um fim de passe a 1280x720: 0.50 ms (mali: medido no Mali-G57 (DBZ3, 2026-10-14)) | tempo por draw: sim

## Resumo por frame

| draws | resolves | passes (min) | trocas de RT | configs de RT | draws de ecra inteiro |
|---:|---:|---:|---:|---:|---:|
| 1923.0 | 23.0 | 32.0 | 17.0 | 11.0 | 3.0 |

## O que tentar (ordenado por ms estimados)

| # | achado | ms/f est. | passes/f poupados | ms/f captura | interruptores |
|---:|---|---:|---:|---:|---|
| 1 | Trocas entre configuracoes de RT que partilham EDRAM (transferencias no Xenia) (`rt_aliasing`) | 7.54 | 0.00 | - | `dbz3_rt_direct = true` |
| 2 | Clear() do XDK como retangulo 4x MSAA de meia largura (`A_clear_msaa`) | 4.88 | 7.00 | 0.50 | `dbz3_clear_to_1x = true` |
| 3 | Custo base dos resolves (cada um fecha o passe e passa por compute no Xenia) (`resolve_cost`) | 1.63 | 0.00 | 160.97 | `dbz3_direct_resolve_texture = 1`, `dbz3_direct_resolve_skip_memory = true`, `dbz3_resolve_early_transition = true` |
| 4 | Resolves sobrescritos sem leitor (`D_unread_resolve`) | 1.08 | 2.00 | 19.31 | `dbz3_rt_elide_restores = true` |
| 5 | Restauro da profundidade desenhado como cor (color base = depth base) (`B_depth_restore_as_color`) | 0.50 | 1.00 | 2.87 | `dbz3_skip_depth_restore = true` |

ms est. = fins de passe poupados × custo por fim de passe (escala com a area do alvo); os achados nao somam (removem por vezes os mesmos passes). ms captura = soma do tempo por draw (inflacionado).

## Achados

### rt_aliasing — Trocas entre configuracoes de RT que partilham EDRAM (transferencias no Xenia)

16.00 ocorrencias/frame, 0.00 fins de passe poupados/frame, ~7.54 ms/frame estimados, tempo na captura - ms/frame, confianca alta.

- frame 0: p640 4x C@0 D@0 -> p1280 1x C@1328 D@0 (draw 1)
- frame 0: p1280 1x C@1328 D@0 -> p320 1x C@1328 D@0 (draw 3)
- frame 0: p320 1x C@1328 D@0 -> p160 4x C@1328 D@0 (draw 4)
- frame 0: p160 4x C@1328 D@0 -> p320 1x C@1328 D@0 (draw 5)
- frame 0: p320 1x C@1328 D@0 -> p1280 1x C@1328 D@0 (draw 72)
- frame 0: p1280 1x C@1328 D@0 -> p1040 1x C@0 D@1024 (draw 625)
- _note_: Com render targets diretos estas trocas deixam de copiar dados; os fins de passe ficam (sao mudancas reais de alvo). Estimativa: cada troca custa uma transferencia ~ um fim de passe do alvo novo.

### A_clear_msaa — Clear() do XDK como retangulo 4x MSAA de meia largura

6.00 ocorrencias/frame, 7.00 fins de passe poupados/frame, ~4.88 ms/frame estimados, tempo na captura 0.50 ms/frame, confianca alta.

- frame 0 draws 0: hash XDK p640 4x C@0 D@0, so prof, seguido de 1x pitch 1280: nao
- frame 0 draws 4: hash XDK p160 4x C@1328 D@0, cor+prof, seguido de 1x pitch 320: sim
- frame 0 draws 626: hash XDK p520 4x C@0 D@1024, so prof, seguido de 1x pitch 1040: nao
- frame 0 draws 680: hash XDK p520 4x C@0 D@1024, so prof, seguido de 1x pitch 1040: nao
- frame 0 draws 791: hash XDK p640 4x C@1328 D@1328, so prof, seguido de 1x pitch 640: nao
- frame 0 draws 842: hash XDK p640 4x C@1328 D@1328, so prof, seguido de 1x pitch 1280: nao
- _dbz3_result_: transfer 1,6 -> 0,12 ms (Adreno 650), 8,7 -> 0,8 ms (Mali-G57)
- _ps_: A4A965C189287B99
- _vs_: 1E6883FCCDE1F688

### resolve_cost — Custo base dos resolves (cada um fecha o passe e passa por compute no Xenia)

23.00 ocorrencias/frame, 0.00 fins de passe poupados/frame, ~1.63 ms/frame estimados, tempo na captura 160.97 ms/frame, confianca media.

- 23.0 resolves/frame, 12.0 deles >= meio ecra
- _note_: Estimativa: 1/4 do custo de um fim de passe por resolve (o resto so desaparece eliminando o resolve: ver D/C). No Adreno 650 com dbz3_resolve_debug os ~8 ms de "resolve" ficaram: o custo e a quebra de passe, nao o compute.

### D_unread_resolve — Resolves sobrescritos sem leitor

2.00 ocorrencias/frame, 2.00 fins de passe poupados/frame, ~1.08 ms/frame estimados, tempo na captura 19.31 ms/frame, confianca alta.

- frame 0 resolve (draw 678) depth 1024x1024 -> 0AA46000, sobrescrito no frame 1
- frame 0 resolve (draw 790) depth 1024x1024 -> 0AE46000, sobrescrito no frame 1
- frame 0 resolve (draw 1744) color0 1280x720 -> 0DC36000, sobrescrito no frame 0
- frame 1 resolve (draw 3683) color0 1280x720 -> 0D89E000, sobrescrito no frame 1

### B_depth_restore_as_color — Restauro da profundidade desenhado como cor (color base = depth base)

1.00 ocorrencias/frame, 1.00 fins de passe poupados/frame, ~0.50 ms/frame estimados, tempo na captura 2.87 ms/frame, confianca media.

- frame 0 draws 1793,1794,1795,1796,1797,1798,1799,1800,1801,1802,1803,1804,1805,1806,1807,1808,1809,1810,1811,1812,1813,1814,1815,1816,1817,1818,1819,1820,1821,1822,1823,1824,1825,1826,1827,1828,1829,1830,1831,1832,1833,1834,1835,1836,1837,1838,1839,1840,1841,1842,1843,1844,1845,1846,1847,1848,1849,1850,1851,1852,1853,1854,1855,1856,1857,1858,1859,1860,1861,1862,1863,1864,1865,1866,1867,1868,1869,1870,1871,1872,1873,1874,1875,1876,1877,1878,1879,1880,1881,1882,1883,1884,1885,1886,1887,1888,1889,1890,1891,1892,1893,1894,1895,1896,1897,1898,1899,1900,1901,1902,1903,1904,1905,1906,1907,1908,1909,1910,1911,1912,1913,1914,1915,1916,1917,1918,1919,1920,1921,1922,1923,1924,1925,1926,1927,1928,1929,1930,1931,1932,1933,1934,1935,1936,1937,1938,1939,1940,1941,1942,1943,1944,1945,1946,1947,1948,1949,1950,1951: p1280 1x C@0 D@0 PS F854BF3B7BF7AA9D, le copia de profundidade: nao, poupa 1 passe(s)
- frame 1 draws 3732,3733,3734,3735,3736,3737,3738,3739,3740,3741,3742,3743,3744,3745,3746,3747,3748,3749,3750,3751,3752,3753,3754,3755,3756,3757,3758,3759,3760,3761,3762,3763,3764,3765,3766,3767,3768,3769,3770,3771,3772,3773,3774,3775,3776,3777,3778,3779,3780,3781,3782,3783,3784,3785,3786,3787,3788,3789,3790,3791,3792,3793,3794,3795,3796,3797,3798,3799,3800,3801,3802,3803,3804,3805,3806,3807,3808,3809,3810,3811,3812,3813,3814,3815,3816,3817,3818,3819,3820,3821,3822,3823,3824,3825,3826,3827,3828,3829,3830,3831,3832,3833,3834,3835,3836,3837,3838,3839,3840,3841,3842,3843,3844,3845,3846,3847,3848,3849,3850,3851,3852,3853,3854,3855,3856,3857,3858,3859,3860,3861,3862,3863,3864,3865,3866,3867,3868,3869,3870,3871,3872,3873,3874,3875,3876,3877,3878,3879,3880,3881,3882,3883,3884,3885,3886,3887,3888,3889,3890: p1280 1x C@0 D@0 PS F854BF3B7BF7AA9D, le copia de profundidade: nao, poupa 1 passe(s)
- _dbz3_result_: fins de passe 37 -> 28/f; Adreno 650 56 -> 59-60 fps
- _groups_reading_depth_copy_per_frame_: 0.0
- _note_: Inutil com render targets diretos: o passe intermedio nao toca na imagem da profundidade.

### D_restore_only_resolve — Resolves lidos so por restauros (B/C)

1.50 ocorrencias/frame, 1.50 fins de passe poupados/frame, ~0.28 ms/frame estimados, tempo na captura 1.70 ms/frame, confianca alta.

- frame 0 resolve (draw 1767) color0 320x176 -> 0E785000, leitores: draws 1793
- frame 1 resolve (draw 3706) color0 320x176 -> 0E785000, leitores: draws 3732
- frame 1 resolve (draw 3731) color0 1280x720 -> 0D89E000, leitores: draws 3732

### D_no_reader_in_capture — Resolves sem leitor dentro da captura (pode ser lido pelo CPU ou mais tarde)

2.50 ocorrencias/frame, 0.00 fins de passe poupados/frame, ~0.00 ms/frame estimados, tempo na captura - ms/frame, confianca baixa.

- frame 0 resolve (draw 2) color0 1280x720 -> 0B671000
- frame 1 resolve (draw 1955) color0 1280x720 -> 0B670000
- frame 1 resolve (draw 2631) depth 1024x1024 -> 0AA46000
- frame 1 resolve (draw 2743) depth 1024x1024 -> 0AE46000
- frame 1 resolve (draw 3679) color0 1280x720 -> 0D16E000

## Resolves por tamanho (por frame)

| tamanho | por frame |
|---|---:|
| 320x176 color0 | 10.0 |
| 1280x720 color0 | 9.0 |
| 1024x1024 depth | 2.0 |
| 1280x720 depth | 1.0 |
| 256x256 color0 | 1.0 |

## Pixel shaders (top 12 por draws)

| PS | draws/f | ms/f (captura) | tfetch | efeito |
|---|---:|---:|---:|---|
| E104A745F271970A | 548.5 | 7.71 | 1 |  |
| A4A965C189287B99 | 148.0 | 2.03 | 0 |  |
| 20E9E2EE7402ADFB | 147.0 | 3.25 | 2 |  |
| 06AF22B469A121E4 | 142.0 | 2.59 | 1 |  |
| 28AEC10B4A0E14FF | 117.0 | 1.66 | 1 |  |
| 59B2332CE973997A | 80.0 | 0.97 | 2 |  |
| A48490965E7C834D | 69.0 | 1.38 | 3 |  |
| 5C6E4C6742C142A6 | 43.0 | 1.98 | 4 |  |
| C1D2AF15BA1F1EAC | 36.0 | 0.62 | 2 |  |
| 483647F90A97963D | 34.0 | 0.84 | 3 |  |
| E664840CC5314C1D | 33.0 | 0.61 | 3 |  |
| E060C0FFE9EE118C | 29.0 | 0.54 | 2 |  |

## Configuracoes de RT

| RT | draws/f |
|---|---:|
| pitch=1280 msaa=1x color@1328 8_8_8_8 depth@0 D24FS8 | 917.0 |
| pitch=1280 msaa=1x color@1328 32_FLOAT depth@0 D24FS8 | 551.0 |
| pitch=1040 msaa=1x color@1024 32_FLOAT depth@1024 D24FS8 | 160.0 |
| pitch=1280 msaa=1x color@0 8_8_8_8 depth@0 D24FS8 | 159.0 |
| pitch=320 msaa=1x color@1328 8_8_8_8 depth@0 D24FS8 | 67.0 |
| pitch=320 msaa=1x color@1024 8_8_8_8 depth@0 D24FS8 | 37.0 |
| pitch=640 msaa=4x color@1328 8_8_8_8 depth@1328 D24S8 | 26.0 |
| pitch=520 msaa=4x color@0 8_8_8_8 depth@1024 D24FS8 | 2.0 |
| pitch=1040 msaa=1x color@0 8_8_8_8 depth@1024 D24FS8 | 2.0 |
| pitch=160 msaa=4x color@1328 8_8_8_8 depth@0 D24FS8 | 1.0 |
| pitch=640 msaa=4x color@0 8_8_8_8 depth@0 D24FS8 | 1.0 |
