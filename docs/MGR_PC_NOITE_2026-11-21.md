# MGR PC — noite de 20→21/11/2026

Das 22:42 às 03:30 (hora de Portugal), depois do 2.º reboot do PC. Branch `pc/perf-665` (com push). Só o Odin 2 Mini
(Turnip v27, perfil sd480p). Medida = luta do CONTINUE, 30 s, comparada por faixa de chamadas/quadro.
Relatório com fotos: https://claude.ai/artifact/L8JgF2RS8Eer9utReR9yGz

## Resumo

| Item | Estado | Resultado |
|---|---|---|
| 1. Eventos/semáforos com lock próprio | feito | imports que tomam a máquina 17 mil/s → 1,8 mil/s; máquina ocupada 18–22% → 1–2% |
| 2. InterlockedExchangeAdd sem máquina + A/B | feito | **MT=1 39,6 / 25,1 fps contra MT=0 16,7 / 15,7** na mesma faixa (+60% a +137%) |
| 3. Crash do CONTINUE: causa real | corrigido | lock fantasma na ponte D3D9; 6/6 corridas sem falha, 0 guardas acionadas |
| 4. FSR 1 / CAS e interpolação 30→60 | integrado | FSR 720p→1080p 5,93 ms de GPU; interpolação 300/300 quadros, 4,65 ms cada |
| 5. MT no launcher | feito | opção Multithread (ligada por defeito), lib MT na APK: 172 → 131 MB |
| 6. Toques perdidos no MT (launcher) | corrigido em parte | botões e cliques do toque ficam ≥ 80 ms; e2e pela app 1–2/3 chegam à luta; o aviso de auto-save ainda prende às vezes, também em MT=0 |

## 1–2. Multithread

`guestSyncMutex` protege eventos, semáforos, mutexes e handles de thread (ordem: máquina → guestSyncMutex →
guestSignalMutex). Quem espera verifica e regista-se com o lock na mão; `checkGuestProgress` nunca corre com ele.
`WaitForMultipleObjectsEx` continua com a máquina (APCs).

| Corrida | MGR_MT | fps | chamadas/quadro | imports c/ máquina | máquina ocupada |
|---|---|---|---|---|---|
| ab1-1..3 (antes) | 1 | 19,9 / 26,4 / 18,9 | 10,7k / 8,1k / 13,9k | ~17 mil/s | 17–20% |
| sy1-1, sy1-2 (+ lock próprio) | 1 | 27,7 / 26,9 | 9,4k / 9,5k | 1,8 mil/s | 1–2% |
| ia1-1, ia1-2 (+ ExchangeAdd) | 1 | **39,6 / 25,1** | 8,2k / 14,9k | ~700/s | 2% |
| ia0-1, ia0-2 | 0 | 16,7 / 15,7 | 11,3k / 14,2k | — | 82% |

## 3. CONTINUE

Ver a secção "causa real do crash do CONTINUE" em `MGR_PC_LIVRE_2026-11-20.md`. Resumo: o MGR deixa VB/IB com Lock
permanente; a ponte não apagava o lock ao libertar o objeto; o endereço reutilizado dava Lock INVALIDCALL →
EffectPrimitive falha → StartupScene (0x009EE540) sai sem criar `[0x01B788C0+0x1E70]`. Correção `dropStaleLocks`.

| Corrida (MT=1) | fps | EffectPrimitive | locks fantasma limpos | guardas 1E70 |
|---|---|---|---|---|
| fx1-1..3 | 25,7 / 24,7 / 27,2 | 0 | 4 cada | 0 |
| fx1-4 (2 cargas) | 25,6 | 0 | 8 | 0 |
| fsr-1 / fi-1 | 26,8 / 25,3 | 0 | 4 | 0 |

## 4. FSR e interpolação

| Modo | fps do jogo | GPU por quadro do efeito |
|---|---|---|
| FSR 1 (EASU+RCAS) 1280x720 → 1920x1080 | 26,8 | 5,93 ms |
| Interpolação 30→60 | 25,3 | 4,65 ms (300/300 gerados, fallback 5–8% dos pixels em movimento) |

## 5–6. Launcher

- Opção "Processador: Multithread / Uma thread (compatível)", pt/es/en; `MGR_MT` posto pelo `PcGameActivity`.
- A APK leva a lib do build MT (GUEST_FAST -O2): `libmgrpc.zdat` 93 → 52 MB.
- Pela app, com MT, os toques rápidos (botões do comando no ecrã e cliques) perdiam-se: o jogo lê o input mais de
  uma vez por atualização do menu e o "latch" de uma leitura ficava na leitura errada. Agora ficam carregados pelo
  menos 80 ms (`inputPadHold`, `inputMouseHeldUntil`).
- E2E (`build/lab/pc/launcher/e2e.sh`): espera o aviso de auto-save e toca A até sair; volta a tocar no slot se o
  aviso não aparecer.

## Bancada

- 23:20: MT=0 a 2,5 fps (base e mudança iguais) com o Odin a 1% de bateria; 01:20, com 88%, MT=0 16,7.
- O deploy envia `libmgrpc.stripped.so`, que o ninja não refaz (a 1.ª rodada sy correu com a lib antiga).
- `build/lab/pc/mt/one.sh <nome> <MT> [env]` em bash (no zsh `set -- $n` não separa palavras).
- O DXVK com FSR pede `prepare.py build/lab/pc/dxvk-native-1.9.2b build/lab/pc/dxvk-source` antes do ninja.

## Pendências

- Aviso de auto-save que não fecha pela app em 1–2 de 3 corridas, em MT e em MT=0 (18 corridas, 0 crashes): ver se é o
  save a correr (overlapped/APC) ou o toque; testar com comando físico.

- Avaliar o FSR e a interpolação no ecrã e ligá-los aos perfis.
- Retirar as guardas 1E70 depois de mais cargas sem acerto; log quando `0x009EE540` devolver 0.
- Imports que ainda tomam a máquina: XInput (~100/s), Sleep, COM 0x7c00….
- Merge do `pc/perf-665` no main: precisa do seu ok.

## Commits (pc/perf-665)

`9c6d082` lock próprio · `3988275` merge FSR/FI · `35b0aa9` ExchangeAdd + opção Multithread · `24d949f` locks
fantasma D3D9 · `1e4c2cd` toques ≥ 80 ms. PCPorts: `1f30af2`, `a894c91`.
