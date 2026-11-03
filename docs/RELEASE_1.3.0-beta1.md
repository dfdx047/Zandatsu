# Zandatsu v1.3.0-beta1

**Beta de teste · Test beta · Beta de prueba.** Use a v1.2 se não puder testar · Stay on v1.2 if you can't test · Quédate en la v1.2 si no puedes probar.

**APK:** `Zandatsu-v1.3.0-beta1.apk` (1.3.0-beta1, versionCode 4)
**SHA-256:** `PREENCHER`

## Português

Esta beta corrige causas achadas nos logs de vocês. Preciso de quem tem:

- **Mali-G615** (Dimensity 8300/8350): crash no meio do jogo no Nativo (#24). O app agora anota o shader que derruba o driver. Pode cair mais 1 ou 2 vezes no mesmo lugar, depois deve passar.
- **PowerVR BXM-8-256** (Dimensity 7025/7050): crash ao abrir o Nativo (#27). Os pipelines passam a ser criados um de cada vez. O primeiro carregamento pode demorar mais.
- **Adreno 710 com quedas para 0 fps no Emulado** (#41): a compilação de efeitos não para mais o jogo.
- **Mali-G710/G720** (Dimensity 9000 a 9400, POCO X7 Pro): tela cortada (#13) e minimapa na tela inteira.

Depois de jogar, toque em **Exportar logs** e anexe o zip na issue, mesmo que tudo funcione.

## English

This beta fixes causes found in your logs. I need testers with:

- **Mali-G615** (Dimensity 8300/8350): crash mid-game on Native (#24). The app now records the shader that crashes the driver. It may crash once or twice more at the same spot, then it should go through.
- **PowerVR BXM-8-256** (Dimensity 7025/7050): crash when Native starts (#27). Pipelines are now created one at a time. The first load may take longer.
- **Adreno 710 with drops to 0 fps on Emulated** (#41): compiling new effects no longer stops the game.
- **Mali-G710/G720** (Dimensity 9000 to 9400, POCO X7 Pro): cut-off screen (#13) and minimap covering the whole screen.

After playing, tap **Export logs** and attach the zip to the issue, even if everything works.

## Español

Esta beta corrige causas encontradas en sus logs. Necesito a quien tenga:

- **Mali-G615** (Dimensity 8300/8350): cierre a mitad del juego en Nativo (#24). La app ahora anota el shader que tumba el driver. Puede cerrarse 1 o 2 veces más en el mismo lugar; después debería pasar.
- **PowerVR BXM-8-256** (Dimensity 7025/7050): cierre al abrir Nativo (#27). Los pipelines se crean uno a la vez. La primera carga puede tardar más.
- **Adreno 710 con caídas a 0 fps en Emulado** (#41): compilar efectos nuevos ya no detiene el juego.
- **Mali-G710/G720** (Dimensity 9000 a 9400, POCO X7 Pro): pantalla cortada (#13) y minimapa en toda la pantalla.

Después de jugar, toca **Exportar logs** y adjunta el zip en la issue, aunque todo funcione.
