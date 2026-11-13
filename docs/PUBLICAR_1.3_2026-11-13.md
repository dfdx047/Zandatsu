# Publicação da v1.3 (13/11, 03:00 Lisboa)

| Item | Resultado |
|---|---|
| Release v1.3 | Publicada como Latest: https://github.com/dfdx047/Zandatsu/releases/tag/v1.3 |
| APK | `Zandatsu-v1.3.apk` (build de 12/11 22:28, sem recompilar), SHA-256 `239f424f…6d47`, o baixado do GitHub confere |
| Instala por cima | Mesmo certificado da v1.2.2 (`1a7325cf…f532`), versionCode 6 > 5, mesmo pacote `com.dfdx047.zandatsu` |
| Issues | 54 fechadas com resposta, 4 abertas com atualização (#16, #30, #47, #52) |
| README | Cabeçalho com badges (versão, Android 11+, ARM64, Vulkan 1.3, licença), botões Baixar APK, Ko-fi e Discord, 8 fotos novas em `docs/assets/screens/`, sem as novidades por versão |

## Issues

- Corrigidas na 1.3: #53 (pular logos), #24 e #35 (céu preto G615), #19, #14, #2, #7, #74 (fps na Mali), #42, #43, #46,
  #21 (cutscenes), #61 (Adreno 829, pede reteste), #44 (DLC: só trajes).
- Corrigidas antes (1.2/1.2.2): #45, #13, #23, #36, #50, #51 (tela cortada), #18 (Sundowner, confirmado), #17, #22, #29
  (crash G615), #27 (PowerVR), #41 (Emulado), #5 (som).
- Duplicadas: #38, #39 → #27; #20, #40 → #16; #12 → #52.
- Testes de GPU recebidos: #54–#60, #62–#69, #71–#73, #75, #76.
- Sem log / sem suporte: #48 (Turnip ARMSX2 no A710), #49 (RG556): pedem o teste de GPU na 1.3.
- Abertas: #16 (Nativo na Mali-G57 MC2), #47 (crashes Mali-G57), #52 (esconder Pausa/Ajustes com controle), #30
  (pedidos do controle na tela, inclui os da #66).

## Pendente

- O README novo está no `fix/poco-x6-pro` (commit `9aa8f02`). A página principal do GitHub mostra o `main`, que está 72
  commits atrás (fast-forward limpo). Falta autorizar o push para o `main`.
- Nenhum aparelho testado nesta sessão (pedido do utilizador): o APK é o mesmo já testado no Odin 2 Mini e no POCO X6 Pro.
