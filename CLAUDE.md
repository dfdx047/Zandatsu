# Zandatsu — instruções para o Claude Code

Port de Metal Gear Rising: Revengeance (Xbox 360) para Android por recompilação estática
(ReXGlue SDK, GPU Xenia/Vulkan). App Kotlin em `app/`, código do jogo gerado (nunca no git).

## Como falar com o utilizador
- Responder em português do Brasil. Comandos git e mensagens de commit em inglês, curtos.
- Horas sempre na hora de Portugal (Europe/Lisbon).
- Ir direto ao ponto: fazer, medir, mostrar números. Nada de planos longos sem executar.

## Como trabalhar
- Sem pedir ok (decidir sozinho e anotar no relatório): repetir ou alongar medições (até 30 min por item); liberar
  memória (`./gradlew --stop`, matar processos de build antigos, limpar caches de build); reinstalar o APK, repor
  saves de teste, mudar `cvars.txt` nos aparelhos de teste.
- Parar e perguntar só para: apagar arquivos fora do projeto, push/merge no main, release, trocar o driver padrão
  da app, ou algo que mexa nos dados pessoais de um aparelho.

## Aparelhos (adb, ligados neste PC)
| Aparelho | Serial | GPU | Meta |
|---|---|---|---|
| Retroid Pocket 5 | `611e0fa0` | Adreno 650 (SD865), Turnip ou driver Qualcomm | 720p a 60 fps |
| RG Cube | `RGCUBE13528869` | Mali-G57 | pelo menos 30 fps |

- Pasta do app no aparelho: `/sdcard/Android/data/com.dfdx047.zandatsu/files`
  (jogo em `game/`, `cvars.txt` sobrepõe cvars, logs em `logs/`).
- Para testar cvars sem compilar: escrever `cvars.txt` com linhas `nome = valor` e reabrir o
  jogo; apagar o ficheiro (ou deixá-lo vazio) no fim. Sempre `vibration = false` nos testes.
- Captura de tela: `adb -s <serial> exec-out screencap -p > foto.png`.

## Build
- `bash tools/build.sh` — pull, aplica patches do SDK, codegen se preciso, compila, instala em
  todos os aparelhos. `bash tools/build.sh rp5` / `cube` instala só num; `none` não instala.
  O build usa o branch `main`.
- Patches do SDK: `patches/NNNN-*.patch`, aplicados por `tools/apply-sdk-patches.sh`.
  Nunca editar um patch publicado: criar o próximo número.
- O jogo (código gerado, hooks, Renut, shaders) vai no `libgame.so`, que só entra no APK encriptado com a chave
  do `default.xex` (`tools/pack-libgame.py`); a app desencripta-o para `getFilesDir()` no import ou no JOGAR
  quando o APK muda. `libmain.so` = só o leitor do XEX. Antes de publicar: `tools/check-clean.py`.

## Regras do driver
- Otimizar para o Turnip; o driver Qualcomm continua opção. Nada de troca silenciosa de driver.
- No Turnip em Adreno 6xx o app põe sozinho `TU_DEBUG=push_consts_per_stage` (GMEM; sysmem saiu em 23/10) e
  `REX_DBZ3_VK_MAX_STORAGE_BUFFER_RANGE_MB=512` (`TurnipSupport.applyAutoEnv`).

## Medir
- Autoteste ao vivo do render (menu do jogo) mede a GPU A/B com o jogo a correr.
- Comandos por broadcast: `adb -s <serial> shell am broadcast -a com.dfdx047.zandatsu.CMD -p com.dfdx047.zandatsu
  --es cmd auto|capture|selftest|press ...` (ver `game/AutoPilot.kt`).
- `swordman_skip_ps = HASH,HASH` salta draws por pixel shader (desligar efeitos para medir).
- Loop sem ninguém: `tools/autoloop.sh` + `tools/autotest-plan.txt` (opcional).
- Análise de capturas `.dfc`: repositório Optxbox (`python3 -m optxbox timeline|analyze|shaders`).

## Relatórios
- Ao fim de cada sessão longa (noite, dia, ou mais de 3 itens), publique o relatório como Artifact (página privada
  no claude.ai) no formato do relatório de 27/10: tabela-resumo por item, fotos ao lado de cada item, antes/depois,
  drivers, pendências e commits. Mande o link no fim.
- O mesmo relatório também vai em docs/ como .md (sem as fotos).
- No terminal, só um resumo de 5 linhas e o link.

## Mapa do frame do MGR (RP5, captura de 22/10)
- Pré-passe de profundidade ~5,5 ms; mapa de sombra; sombra em tela (PS `BE9C3F694476A5B5`, ~3 ms);
  materiais da cena ~8,7 ms; efeitos/golpes ~3,6 ms; brilho 320x176 (desfoque
  `C45580526034B1F2,4086D1DF9FA6220E`, desligado por defeito); composição final `F854BF3B7BF7AA9D` + HUD.

## Trabalho em curso
1. Menu de qualidade estilo PC (perfis Baixo/Médio/Alto/Ultra/Personalizado por SoC + opções).
2. Render nativo só dos efeitos: sombra em tela primeiro (cheia / meia resolução / desligada).
3. Template de launcher no Optxbox; portar tudo para Dragon Burst e Budokai 3.
4. Em aberto: DEVICE_LOST intermitente no Turnip, mosaico no HUD, abaixo de 720p (patch 0091).
