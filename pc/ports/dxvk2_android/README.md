# DXVK 2.7.1 (só D3D9) para Android arm64

Substituto direto do `../dxvk_android` (DXVK native 1.9.2b) para A/B no port PC do MGR:R.
Fonte: tag `v2.7.1` do DXVK oficial (commit `c3dd74be6baec53786d4e064a572185b70347a17`), com os
submódulos de `include/` (Vulkan-Headers, SPIRV-Headers, cabeçalhos nativos DirectX).

## Compilar

```bash
bash pc/ports/dxvk2_android/build.sh
```

Faz `prepare.py` (copia `build/lab/pc/dxvk2` para `build/lab/pc/dxvk2-android/source` e aplica as mudanças
Android), `shaders.py` (60 shaders internos, glslang `--target-env vulkan1.3`, igual ao meson do DXVK) e
CMake + Ninja com o NDK 29 (`ANDROID_NDK` muda o NDK). Saída:
`build/lab/pc/dxvk2-android/build/libdxvk_android.so`. Tudo o que é gerado fica em `build/`.

## Interface (igual à 1.9.2b)

- Mesmo nome de biblioteca (`libdxvk_android.so`, SONAME igual) e as mesmas funções C:
  `Direct3DCreate9(Ex)`, `dxvkAndroidSetWindow`, `dxvkAndroidGetWindowHandle`, `dxvkAndroidSetRenderSize`,
  `dxvkAndroidSetInstanceProcAddr` (dlsym no `mgr_loader.cpp`), `dxvkAndroidGetInstanceProcAddr`,
  `dxvkAndroidSurfaceDestroyed`.
- O `libmgrpc.so` atual resolve parte da libc++ (estática) pela biblioteca DXVK. `runtime_exports.txt`
  lista os 142 símbolos que ele importa dela; o CMake força-os com `-Wl,-u` para continuarem exportados.
  Se o runtime passar a importar outros, regenerar a lista (`llvm-nm -D --undefined-only libmgrpc.so`
  cruzado com `--defined-only` da biblioteca 1.9).
- Os cabeçalhos D3D9/COM nativos da 2.7.1 têm a mesma vtable que os da 1.9.2b (o runtime continua a
  compilar contra `build/lab/pc/dxvk-source`).

## Mudanças Android (`prepare.py`)

- WSI: driver `Android` (`android_wsi.cpp`, ANativeWindow), padrão quando `DXVK_WSI_DRIVER` não está
  definido. Sem SDL/GLFW.
- Vulkan: `LibraryLoader` usa o driver escolhido no launcher (adrenotools/sistema), não `dlopen`.
- Presenter: `VK_SUBOPTIMAL_KHR` (todo present em paisagem no Android) não recria o swapchain; a
  superfície devolve a referência da janela ao ser destruída.
- EDID: sem libdisplay-info (stub; o WSI não tem EDID).
- Log: avisos e erros para o logcat (tag `DXVK`) ou para `nfsRuntimeLog` quando existe.

## Não portado da 1.9 (extras do laboratório)

Medição por passe (`patch_pass_timing.py`), ficheiro de pipeline cache (`pipeline_cache.*`), `query_check`,
banco Linux X11/headless (`dxvkCreateTestWindow`), proteção PowerVR (`pipeline_guard.h`), fallbacks de
features para Mali/PowerVR/Adreno 610 (depthClamp, clip distance, multiViewport, geometryShader, BC, …),
ordem de reset do command pool para Adreno 6xx antigo e as correções de back buffer nulo no Reset.
A 2.7.1 exige essas features (Turnip em Adreno 7xx tem todas); `DXVK_STATE_CACHE*` não existe mais na 2.7.

## Licenças

DXVK: zlib ([DXVK-LICENSE](DXVK-LICENSE)). Os cabeçalhos nativos DirectX incluem material derivado do
Wine/MinGW-w64 (LGPL 2.1+): manter os avisos ao distribuir.
