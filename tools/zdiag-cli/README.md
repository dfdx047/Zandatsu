# zdiag-cli

As mesmas fontes da `libzdiag.so` (`app/src/main/cpp/diag/`) num executável para correr com
`adb shell`, sem a app. Só usa o driver Vulkan do sistema (o driver alternativo via libadrenotools
precisa das libs de hook da app; ver em baixo).

## Compilar

```sh
NDK=$HOME/Android/Sdk/ndk/28.2.13676358
cmake -S tools/zdiag-cli -B tools/zdiag-cli/build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-30 -DCMAKE_BUILD_TYPE=Release
cmake --build tools/zdiag-cli/build
```

O `bc_decode` (i) usa o SPIR-V do Renut em `out/renut/host_shaders/bc_decode_cs.spirv.h`; sem ele
esse sub-teste fica `skip`.

## Correr

```sh
S=<serial>
adb -s $S shell mkdir -p /data/local/tmp/zdiag
adb -s $S push tools/zdiag-cli/build/zdiag_cli /data/local/tmp/zdiag/
adb -s $S shell 'cd /data/local/tmp/zdiag && ./zdiag_cli list'
adb -s $S shell 'cd /data/local/tmp/zdiag && ./zdiag_cli all out'            # cada teste num fork
adb -s $S shell 'cd /data/local/tmp/zdiag && ./zdiag_cli viewport_scissor out'
adb -s $S pull /data/local/tmp/zdiag/out
```

`zdiag_cli <teste|all> <outDir> [opcoes]` — cada teste escreve `<outDir>/<teste>.json`, PNGs
`<teste>_<passo>.png` e, durante a execução, `<teste>.progress` (linha 1 = passo atual; se o
processo morrer por sinal, linha 2 = `signal=N`). `all` diz em que passo um filho morreu.

Opções (`chave=valor;chave=valor`):

| opção | efeito |
|---|---|
| `force_crash=<teste>/<passo>` | SIGSEGV quando o passo começa (ex. `viewport_scissor/minimap/area_full`) |
| `timeout_ms=N` | espera máxima por cada submit (10000) |
| `device=N` | VkPhysicalDevice a usar (0) |
| `threads=N` | threads do `pipelines_parallel` (min(4, núcleos/2), pelo menos 2) |
| `screen=WxH` | alvo grande do `viewport_scissor` (2400x1080) |
| `png_scale=N` | redução dos PNGs grandes (2) |
| `inject_bug=1` | `viewport_scissor` estraga o scissor de propósito (prova que a verificação falha) |
| `bc_tol=N` | tolerância por canal da descodificação BC nativa (4) |
| `fsync=0` | não faz fsync do `.progress` |
| `driver_dir`, `driver_name`, `native_lib_dir` | só no CLI: driver alternativo via libadrenotools (compilar com `-DZDIAG_ADRENOTOOLS_SRC=<.../_deps/adrenotools-src>`) |
