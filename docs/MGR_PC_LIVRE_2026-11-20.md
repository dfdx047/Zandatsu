# MGR PC — sessão livre de 20/11/2026 (19:36 → …, Lisboa)

Branch `pc/perf-665` (sem push). Só o Odin 2 Mini (192.168.1.206), perfil sd480p com pin. Turnip v27, DXVK 1.9.2b.
Medida = 3 × 30 s no combate do CONTINUE; só se comparam corridas da mesma faixa de chamadas/quadro (~8k ou ~11–14k).
Relatório: https://claude.ai/artifact/LUqZE9JVJFYHD8Pa9dKJNa

## 1. Raio-X da thread principal (MGR_XRAY=1 + simpleperf)

`MGR_XRAY=1` (novo, lab) mede por thread o tempo à espera da máquina (fila do lock), à espera de sinais do jogo
(event/semáforo/mutex/thread), a dormir, e o resto (a correr: código do jogo, imports, bridge D3D9). Log com o Present.
Scripts: `build/lab/pc/mt/xray.sh` (m4run + simpleperf + /proc/pid/maps) e `build/lab/pc/mt/perfsplit.py`
(o simpleperf não resolve libs em memfd: separamos jogo/runtime/DXVK pelos intervalos do maps e pelos símbolos).

| Corrida | fps | chamadas | render corre | espera máquina | espera sinais | máquina ocupada |
|---|---|---|---|---|---|---|
| xr-1 | 24,5 | 8,2k | 5,7 ms | 15,0 ms | 20,2 ms | 80–82% |
| xr-5 | 17,3 | 8,6k | 6,1 ms | 23,0 ms | 27,5 ms | 81–83% |
| xr-6 | 16,0 | 12,3k | — | — | — | — |

CPU (xr-6, 20 s, 122% de um núcleo): jogo 40% das amostras (49% de um núcleo), kernel 22% (27%: futex/escalonador),
runtime 10,6%, libc 10,4% (memcpy, cas, futex, malloc), DXVK 6,7%, vdso/clock_gettime 6,2% (8% de um núcleo),
driver 2,9%. Função mais quente do jogo: sub_0141AD60 (5,7%).

Conclusão: a thread de render só corre ~11% do quadro; ~40% à espera da máquina e ~50% à espera dos jobs, que por
sua vez esperam a máquina. ~35–40% de um núcleo vai só nas trocas (futex, relógio, locks). Teto sem a máquina única:
a GPU (50–58% a 16–18 fps, 124,8 MHz) limita perto de 30–34 fps.

## Crash do CONTINUE: novo sítio 0x00F40ED0

2 de 2 corridas caíram às 19:44/19:51 com escrita em [NULL+0xC4] (`sub_00F40ED0`, chamada por `sub_009F3EC0`).
`sub_009F3EC0` é o update por quadro do objeto 0x01B788C0, chamado pelo loop principal (0x00A547F9 em
`sub_00A54490`) a menos que a flag 0x8000000 de [0x01BEA070] esteja ligada. Durante o recarregar do CONTINUE o loop
corre quadros depois de `sub_00A540F0` destruir o objeto e antes de 0x009EE60A o criar, com a flag já desligada.
Guarda à entrada de `9F3EC0` (`guards.txt`): sem objeto, salta a chamada como o jogo faz com a flag. Depois: 0 crashes
em 3 corridas seguidas.

## 2. Runtime multithread (em curso)

- pcrecomp `RECOMP_MT`: todo o estado da CPU (registos, flags, x87 em janela, SSE/MMX, cur_func, fs) num
  `recomp_cpu_t` por thread, achado num slot fixo do TCB do bionic (TLS_SLOT_APP, livre para apps desde o Android 10):
  `mrs tpidr_el0; ldr` uma vez por função. Fora do Android, `__thread`.
- lift32: `lock cmpxchg/xadd/add/sub/inc/dec`, `lock cmpxchg8b` e `xchg` com memória → atómicos seq_cst do host
  (`recomp_lock_cas/add/xchg`); desalinhado → spinlock global. No MGR: 142 cmpxchg, 41 cmpxchg8b, 64 xchg, 2 add/sub.
- Runtime: `MGR_MT=1` deixa os threads correrem o código do jogo em paralelo; a máquina só é tomada dentro dos imports
  (lookup → thunk) e largada por `callGuest` enquanto o código do jogo corre. EnterCriticalSection contido dorme no
  próprio mutex (futex). Interlocked* passam a atómicos do host. `MGR_MT=0` (padrão) = máquina única como antes.

### MT a funcionar (retomado 20:41 depois do reboot, Odin 2 Mini, luta do CONTINUE, 30 s, X-ray)

O que foi preciso para `MGR_MT=1` correr:
1. **Arranque travado** (deadlock): `D3DXCreateTexture` chama `CreateTexture` por `callGuest`, que resolvia o alvo por
   `recomp_lookup_import` (toma a máquina) e logo a largava toda no `MtRelease`. O import corria sem máquina e o
   `unlock` do thunk deixava a thread de arranque dona da máquina para sempre. Agora um alvo host no `callGuest` corre
   com a máquina que já temos. Detetor `MT leak` no lookup + dump de stall também antes do 1.º quadro (dono da máquina).
2. **Contenção**: com tudo pela máquina, 85–165 mil imports/s, 50 mil trocas/s, **5 fps**. Saíram da máquina:
   D3DX math, GetCurrentThreadId, Interlocked*, Get/SetLastError, relógios (puros) → 20 fps; Enter/LeaveCriticalSection
   (tabela com lock próprio) → 28 fps no título; HeapAlloc/Free/ReAlloc/Size (`GuestHeaps` com mutex, `GuestMemory`
   com shared_mutex), TlsGet/SetValue; **bridge D3D9 com mutex próprio** (a thread que grava os draws fazia ~160 mil
   chamadas/s, cada uma a tomar a máquina). `MGR_IMPORT_STATS=1|2` (por chamador / por thread) mostra o que falta.
3. **Crashes do CONTINUE** mais frequentes com o load mais rápido: guardas nos setters `00F40ED0`/`00F40EF0` e no
   update por quadro `009D5F00` (só o ficheiro do shard muda: re-lift `--shard` + cópia de um .c, sem rebuild total).

A/B no mesmo build (`build/pc/android-mt`), alternado:

| Corrida | MGR_MT | fps | chamadas/quadro | máquina ocupada | render: espera máquina | render: espera sinais |
|---|---|---|---|---|---|---|
| ab1-1 | 1 | **19,9** | 10,7k | 17% | 8,9 ms | 39,1 ms |
| ab0-1 | 0 | 17,9 | 8,2k | 82% | 21,2 ms | 26,8 ms |
| ab1-2 | 1 | **26,4** | 8,1k | 20% | 6,4 ms | 27,9 ms |
| ab0-2 | 0 | 17,4 | 10,2k | 82% | 19,7 ms | 26,0 ms |
| ab1-3 | 1 | **18,9** | 13,9k | 17% | 6,3 ms | 38,3 ms |
| ab0-3 | 0 | — | — | — | — | — (parou: ITAIL 0 de 0x00F40F20, corrida do CONTINUE, não MT) |

Faixa ~8k chamadas: 26,4 (MT) contra 17,9 (máquina única), **+47%**. Faixa ~10–11k: 19,9 contra 17,4, **+14%**.
0 crashes em 3 corridas MT=1 depois das guardas (antes: 1 crash e 1 paragem em 3).

Pendências do MT:
- A render agora espera sobretudo **sinais** (jobs): ReleaseSemaphore/WaitForSingleObject (~17 mil/s) ainda tomam a
  máquina e cada espera re-toma a máquina ao acordar. Próximo passo: eventos/semáforos com lock próprio.
- Corrida do CONTINUE (NULL em `[obj+0x1E70]`) aparece também com MT=0 (`0x00F40F20`): precisa de uma correção na
  origem (o loop principal a correr durante o teardown), não de mais guardas.
- `MGR_MT` continua desligado por defeito; o build MT (`-DMGR_MT=ON`) não é o do launcher ainda.
- FSR/interpolação (branch `worktree-agent-a15e8a5b9f3fbd060`) ainda por integrar e testar no aparelho.

## Eventos/semáforos com lock próprio (20/11, 22:45–23:35, depois do 2.º reboot do PC)

Eventos, semáforos, mutexes e handles de thread passam a ter o lock `guestSyncMutex` (ordem: máquina → guestSyncMutex
→ guestSignalMutex). No MT, `SetEvent`, `ResetEvent`, `ReleaseSemaphore`, `ReleaseMutex`, `WaitForSingleObject` e
`WaitForMultipleObjects` correm sem a máquina; quem espera verifica e regista-se sob o lock, e o sinal nunca se perde.
`WaitForMultipleObjectsEx` continua com a máquina (as APCs chamam código do jogo).

| Corrida | MGR_MT | fps | imports com máquina | máquina ocupada | jobs: espera máquina |
|---|---|---|---|---|---|
| ab1-1..3 (antes) | 1 | 19,9 / 26,4 / 18,9 | ~17 mil/s | 17–22% | 9–14% |
| sy1-1 | 1 | **27,7** | 1,8 mil/s | 1% | 0% |
| sy1-2 | 1 | **26,9** | 1,8 mil/s | 2% | 0% |
| sy1-3 | 1 | — (a navegação não entrou na luta; jogo a correr na rua, sem travar) | | | |

0 crashes e 0 paragens. Os imports que ainda tomam a máquina são `InterlockedExchangeAdd`, XInput, Sleep e COM.

MT=0 caiu para 2,4 fps, mas com o HEAD sem a mudança também deu 2,6: **o Odin estava com 1% de bateria** (na tomada),
com os núcleos 5–7 desligados, e as corridas foram para os núcleos pequenos 0–2. Repetir o A/B MT=0 com a bateria
carregada.

Atenção: o `deploy-lab.sh` envia `libmgrpc.stripped.so`, que o ninja não regenera. Depois do ninja, gerar com
`llvm-strip --strip-debug -o libmgrpc.stripped.so libmgrpc.so` (a primeira rodada sy correu com a lib antiga).

Próximo: `InterlockedExchangeAdd` sem máquina (atómico como os outros Interlocked); A/B MT=0 com bateria cheia.

## 21/11 madrugada: causa real do crash do CONTINUE (lock fantasma na ponte D3D9)

Não há janela de corrida no loop principal: o objeto `cSceneEspManager` (0x01B788C0) é destruído (SCENE CLEANUP,
sub_00F42590 em 0x00F426D8) e recriado (SCENE STARTUP, sub_00A533F0 → sub_009EE540 → sub_00F44440) na mesma thread.
`sub_009EE540` (EspSystemApp::StartupScene) sai com 0 **sem mensagem** quando `0xEC8000`/`0xF581C0` falham, e o
chamador (0x00A53599) não testa: `[+0x1E70]` fica NULL nessa cena inteira → 0x00F40F20 (ITAIL 0) e todas as guardas.
A falha vem de `D3D9 call failed: slot 11 = Lock, INVALIDCALL, caller 00F99AC0` → `EffectPrimitive::Startup` falha.
Nos ~60 run.log com 3+ startups: as 11 corridas que caíram têm todas esse erro; nenhuma sem ele caiu (MT=0 e MT=1).

Mecanismo: o MGR deixa VB/IB com Lock permanente (sub_00F99AC0 Lock / sub_00F99B10 Unlock). No D3D9 real o Release
leva o lock junto. A ponte (`retireGuestObject`) libertava o endereço guest mas deixava a entrada em `resourceLocks`;
quando o heap reutilizava o endereço para o VB/IB novo, o Lock dava INVALIDCALL ("lock já aberto"). Intermitente porque
depende da ordem das alocações. Correção: `dropStaleLocks(address)` em `retireGuestObject` (log "D3D9 stale lock
dropped"). As guardas 1E70 ficam até confirmar no aparelho.

Confirmação no Odin (MT=1, 6 corridas com CONTINUE, 01:30–01:46): `EffectPrimitive` 0 falhas (antes: em todas as
corridas que caíam), 4–8 "stale lock dropped" por corrida, 0 guardas 1E70 acionadas, 0 crashes. Luta 24,7–27,2 fps.
As 2 falhas restantes de "slot 11" são de `IDirect3D9::CheckDeviceMultiSampleType` (sonda de MSAA 8x, NOTAVAILABLE),
não de Lock.
FSR 1 (DXVK_FSR=1, 1280x720 → 1920x1080): 5,93 ms de GPU por upscale, 26,8 fps (a luta está presa na CPU).
Interpolação (DXVK_FI=1): 300/300 quadros gerados, 4,65 ms de GPU por quadro gerado.
