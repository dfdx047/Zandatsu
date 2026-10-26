Oi, Eclipse! Tudo bem?

A próxima versão do Zandatsu traz duas coisas para o seu Galaxy S26 (Xclipse 960), e queria pedir a sua ajuda para testar:

1. **Renut nas GPUs que não são Adreno.** Agora o Renut aparece em **Desempenho → Renderizador** como *experimental*. O "auto" continua no Xenia (ainda não testamos a Xclipse), por isso você precisa escolher **Renut** à mão. Se faltar alguma coisa na GPU, o jogo volta sozinho para o Xenia e o launcher mostra o que faltou.
2. **Drivers RADV Xclipse.** A tela **Drivers** agora funciona na Xclipse, com os drivers RADV do JimVulkan e do Matheus-TestUser1 (precisa Android 14 ou mais novo). É experimental: se a GPU travar com o RADV, **o celular pode reiniciar sozinho**. Nada de errado com o aparelho; o driver da Samsung continua disponível como "driver do sistema".

**O que testar (cada um com 720p e uma luta de uns 2 minutos):**

| # | Driver | Renderizador |
|---|---|---|
| 1 | Samsung (sistema) | Xenia |
| 2 | Samsung (sistema) | Renut |
| 3 | RADV Xclipse | Xenia |
| 4 | RADV Xclipse | Renut |

Para cada teste:
1. Na tela **Drivers**, ative o driver (para o da Samsung, escolha "driver do sistema").
2. Em **Desempenho**, escolha o **Renderizador** e a resolução **720p**.
3. Toque em **JOGAR**, carregue um save e lute uns 2 minutos (golpes normais e Blade Mode).
4. Saia do jogo e, logo em seguida, toque em **Exportar logs** (aba **Sistema** ou **Sobre** do launcher; ou no menu do jogo, pelo botão Voltar). O zip fica em **Downloads** com o nome `Zandatsu-logs-...zip`.
5. Anote: FPS mais ou menos, se a imagem ficou certa (cores, sombras, texturas) e se travou, fechou ou reiniciou.

**Como mandar:** os 4 zips (ou quantos conseguir) e as anotações, numa issue no GitHub do Zandatsu ou por mensagem direta. Se o celular reiniciar no teste 3 ou 4, conta pra gente em que momento foi e mande o zip exportado logo depois de ligar de novo (o log da última sessão fica guardado).

Valeu demais pela ajuda!
