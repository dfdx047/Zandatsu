# Zandatsu - nucleo nativo
#
# Compila o SDK (external/rexglue-sdk: o mesmo do Dragon Burst, fork do ReXGlue do
# Burst Limit com os patches Android de patches/) e o MGR recompilado (src/ e
# generated/default na raiz do repositorio) numa biblioteca libgame.so que o
# SDLActivity carrega (encriptada no APK; ver o fim deste ficheiro).
#
# Resultado (todas em arm64-v8a, empacotadas no APK):
#   libmain.so          carregador: le o default.xex do jogador (chave do libgame.so)
#   libgame.so          jogo recompilado + hooks + Renut + shaders traduzidos + glue Android;
#                       NAO vai no APK: so encriptado (assets/libgame.zdat, tools/pack-libgame.py)
#   librexruntime.so    runtime ReXGlue (kernel, memoria, audio, input, UI)
#   librexgpu-xenos.so  plugin da GPU Xenos (Vulkan)
#   libSDL3.so          SDL3 (janela, eventos, audio, comandos)

cmake_minimum_required(VERSION 3.25)
project(swordman_android LANGUAGES C CXX)

get_filename_component(SM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(REXSDK_ROOT  "${SM_ROOT}/external/rexglue-sdk")
set(SM_GENERATED "${SM_ROOT}/generated/default")

#-----------------------------------------------------------------------------
# Verificacoes antes de comecar (mensagens claras em vez de erros obscuros)
#-----------------------------------------------------------------------------
if(NOT EXISTS "${REXSDK_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "SDK nao encontrado em ${REXSDK_ROOT}.\n"
        "Corre na raiz do repositorio:  bash tools/setup-sdk.sh")
endif()
if(NOT EXISTS "${REXSDK_ROOT}/include/rex/main_android.h")
    message(FATAL_ERROR
        "Os patches Android ainda nao foram aplicados ao SDK.\n"
        "Corre na raiz do repositorio:  bash tools/setup-sdk.sh")
endif()
# Todos os patches de patches/ tem de estar aplicados (o script regista-os em
# .swordman-patches). Evita compilar um SDK desatualizado sem dar por isso.
file(GLOB SM_PATCH_FILES "${SM_ROOT}/patches/[0-9][0-9][0-9][0-9]-*.patch")
set(SM_PATCH_STAMP "${REXSDK_ROOT}/.swordman-patches")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SM_PATCH_FILES} "${SM_PATCH_STAMP}")
set(SM_APPLIED "")
if(EXISTS "${SM_PATCH_STAMP}")
    file(STRINGS "${SM_PATCH_STAMP}" SM_APPLIED)
endif()
foreach(PATCH ${SM_PATCH_FILES})
    get_filename_component(PATCH_NAME "${PATCH}" NAME)
    if(NOT PATCH_NAME IN_LIST SM_APPLIED)
        message(FATAL_ERROR
            "O patch ${PATCH_NAME} ainda nao foi aplicado ao SDK.\n"
            "Corre na raiz do repositorio:  bash tools/apply-sdk-patches.sh")
    endif()
endforeach()
if(NOT EXISTS "${SM_GENERATED}/sources.cmake")
    message(FATAL_ERROR
        "Falta ${SM_GENERATED}/sources.cmake (codigo recompilado do default.xex).\n"
        "Corre na raiz do repositorio:  bash tools/codegen.sh")
endif()

#-----------------------------------------------------------------------------
# SDK ReXGlue
#-----------------------------------------------------------------------------
# O SDK fixa a pasta de saida em out/<plataforma>; aqui apontamo-la para a pasta que
# o Gradle espera, para as .so entrarem no APK.
set(REXGLUE_OUTPUT_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}" CACHE STRING "" FORCE)
set(REXGLUE_USE_VULKAN ON CACHE BOOL "" FORCE)
set(REXGLUE_ENABLE_TRACY OFF CACHE BOOL "" FORCE)
set(REXGLUE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# DLSS / FSR 4 / FidelityFX 2-3 sao so D3D12 (Windows).
set(REXGLUE_ENABLE_DLSS OFF CACHE BOOL "" FORCE)
set(REXGLUE_ENABLE_FSR_SDK OFF CACHE BOOL "" FORCE)
set(REXGLUE_ENABLE_FIDELITYFX OFF CACHE BOOL "" FORCE)
# ARMv8.2-A tambem no SDK (runtime, plugin da GPU, terceiros): atomicos LSE em linha
# em vez das chamadas __aarch64_cas*/ldadd*. Os aparelhos de teste (Snapdragon 865 e
# Unisoc T770/T820) sao ARMv8.2.
string(APPEND CMAKE_C_FLAGS " -march=armv8.2-a")
string(APPEND CMAKE_CXX_FLAGS " -march=armv8.2-a")
add_subdirectory("${REXSDK_ROOT}" rexglue-sdk)

#-----------------------------------------------------------------------------
# libadrenotools: driver Vulkan alternativo (Turnip/Mesa) no lugar do driver
# Qualcomm do sistema. So e usado se o GameActivity definir
# REX_VULKAN_DRIVER_DIR/REX_VULKAN_DRIVER_NAME (aba Drivers).
#-----------------------------------------------------------------------------
include(FetchContent)
if(NOT CMAKE_ANDROID_ARCH_ABI)
    set(CMAKE_ANDROID_ARCH_ABI "${ANDROID_ABI}")
endif()
FetchContent_Declare(adrenotools
    GIT_REPOSITORY https://github.com/bylaws/libadrenotools.git
    GIT_TAG 8fae8ce254dfc1344527e05301e43f37dea2df80
    GIT_SUBMODULES_RECURSE TRUE
)
FetchContent_MakeAvailable(adrenotools)
target_link_libraries(rexui PRIVATE adrenotools)
target_compile_definitions(rexui PRIVATE REX_HAS_ADRENOTOOLS=1)
target_link_libraries(rexruntime PRIVATE adrenotools)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#-----------------------------------------------------------------------------
# Codigo recompilado do default.xex (define GENERATED_SOURCES, caminhos absolutos).
#-----------------------------------------------------------------------------
include("${SM_GENERATED}/sources.cmake")

# Opcoes do codigo recompilado:
#  - so tabelas de linhas no debug info (.o pequenos, link rapido, mas os crashes no
#    logcat continuam a mostrar ficheiro:linha);
#  - sem contracao de FMA: em ARM64 o clang funde a*b+c por omissao e isso muda os
#    resultados de virgula flutuante em relacao ao PowerPC;
#  - o cabecalho pre-compilado que o codegen gera (mgr_pch.h).
set_source_files_properties(${GENERATED_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-gline-tables-only;-ffp-contract=off;-fno-strict-aliasing")

add_library(mgr_recomp OBJECT ${GENERATED_SOURCES})
target_include_directories(mgr_recomp PRIVATE "${SM_ROOT}" "${SM_ROOT}/src" "${SM_GENERATED}")
target_link_libraries(mgr_recomp PRIVATE rex::runtime)
target_precompile_headers(mgr_recomp PRIVATE "${SM_GENERATED}/mgr_pch.h")
target_compile_options(mgr_recomp PRIVATE -march=armv8.2-a -ffp-contract=off)

#-----------------------------------------------------------------------------
# libgame.so: src/ do projeto (o mesmo main.cpp do build de PC) + glue Android.
#-----------------------------------------------------------------------------
add_library(game SHARED "${SM_ROOT}/src/main.cpp" $<TARGET_OBJECTS:mgr_recomp>)

target_sources(game PRIVATE
    # Pausa no VdSwap (patch 0100) e contador de frames.
    "${CMAKE_CURRENT_LIST_DIR}/android_hooks.cpp"
    # Ponte com o app: pausa, cvars ao vivo, volume, log (NativeGameState.kt).
    "${CMAKE_CURRENT_LIST_DIR}/game_state_jni.cpp"
    # Vigia: pilhas de todas as threads em logs/watchdog.txt se o jogo encravar.
    "${CMAKE_CURRENT_LIST_DIR}/watchdog.cpp"
    # Esperas ativas do MGR na CPU (swordman_rwlock_wait).
    "${CMAKE_CURRENT_LIST_DIR}/mgr_cpu_hooks.cpp"
    # A GPU reinicia (hang/falha) sem o jogo fechar: volta ao launcher com o aviso.
    "${CMAKE_CURRENT_LIST_DIR}/gpu_reset.cpp"
)

# Rastreio das funcoes D3D candidatas (optxbox d3dmap + d3d_trace_gen.py do Dragon Burst).
option(DB_D3D_TRACE "Hooks de rastreio da biblioteca D3D (logs/d3d_trace.log)" OFF)
if(DB_D3D_TRACE)
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/d3d_trace_hooks.cpp")
        target_sources(game PRIVATE "${CMAKE_CURRENT_LIST_DIR}/d3d_trace_hooks.cpp")
    else()
        message(FATAL_ERROR "DB_D3D_TRACE ligado mas falta app/src/main/cpp/d3d_trace_hooks.cpp")
    endif()
endif()

target_include_directories(game PRIVATE
    "${SM_ROOT}"
    "${SM_ROOT}/src"
    "${SM_GENERATED}"
    "${REXSDK_ROOT}/thirdparty/imgui"
    "${REXSDK_ROOT}/thirdparty/xxHash"
    "${CMAKE_BINARY_DIR}/rexglue-sdk/include"
)

target_compile_options(game PRIVATE
    -fno-strict-aliasing
    -ffp-contract=off
)

target_link_libraries(game PRIVATE
    rex::runtime
    rex::gpu-xenos
    imgui::imgui
    android
    log
)

# Acrescenta windowed_app_main_sdl.cpp (com o SDL_main do Android) e rex_app.cpp, e as
# opcoes de plataforma do SDK.
rexglue_configure_target(game GPU_PLUGINS xenos)

# Alvo minimo: ARMv8.2-A. Fica depois do rexglue_configure_target, que poe
# -march=armv8-a, para prevalecer.
target_compile_options(game PRIVATE -march=armv8.2-a)

# Chamadas entre funcoes do proprio libgame.so sem PLT: as ~85 mil sub_* do codigo gerado tem visibilidade padrao
# (exportadas, logo "substituiveis" em tempo de execucao) e cada chamada entre elas passava por um stub do PLT e pelo
# GOT (25 mil entradas). Com -Bsymbolic-functions o linker liga-as diretamente; os hooks fortes continuam a ganhar as
# definicoes fracas no link. (-fvisibility=hidden partiria: os imports do kernel, __imp__Rtl..., vem do librexruntime.)
option(SM_BSYMBOLIC "libgame.so com -Bsymbolic-functions" ON)
if(SM_BSYMBOLIC)
    target_link_options(game PRIVATE -Wl,-Bsymbolic-functions)
endif()

# ThinLTO no codigo gerado e no libgame.so (o LTO nao atravessa .so: o librexruntime fica de fora). Cache do ThinLTO
# no build, para os relinks. Ligado por SM_THINLTO (cache do CMake: -DSM_THINLTO=ON no gradle ou no build.sh).
if(SM_THINLTO)
    target_compile_options(mgr_recomp PRIVATE -flto=thin)
    target_compile_options(game PRIVATE -flto=thin)
    target_link_options(game PRIVATE -flto=thin -Wl,--thinlto-jobs=4
                        "-Wl,--thinlto-cache-dir=${CMAKE_BINARY_DIR}/thinlto-cache")
endif()

#-----------------------------------------------------------------------------
# Renut: renderizador nativo de Direct3D do mgr-nx-deko, escolhido no arranque com
# swordman_renderer = renut (padrao xenos). Shaders: tools/renut-shaders.sh.
#-----------------------------------------------------------------------------
option(SM_RENUT "Compila o Renut (swordman_renderer = renut)" ON)
if(SM_RENUT)
    include("${SM_ROOT}/cmake/renut.cmake")
    target_sources(game PRIVATE "${SM_ROOT}/src/renut_glue.cpp")
    sm_renut_setup(game)
endif()

#-----------------------------------------------------------------------------
# libgame.so fora do APK. O codigo do jogo (gerado, hooks, Renut e os shaders traduzidos) so pode estar no aparelho
# de quem tem o disco: o APK leva o libgame.so encriptado (AES-256-GCM, chave = SHA-256 da imagem PE do default.xex)
# e o libmain.so, que no import le o default.xex do jogador (src/xex_image.cpp) para o GameLib.kt derivar a chave,
# desencriptar para getFilesDir() e o GameActivity carregar de la. Os hooks ficam no libgame.so porque substituem as
# definicoes fracas sub_X no link (-Bsymbolic-functions); o runtime, o plugin da GPU e o SDL ja sao .so a parte.
#-----------------------------------------------------------------------------
set(SM_XEX "${SM_ROOT}/assets/default.xex" CACHE FILEPATH "default.xex do codegen (chave do libgame.so)")
set(SM_PACK_DIR "${SM_ROOT}/app/build/zandatsu-assets")
add_custom_command(
    OUTPUT "${SM_PACK_DIR}/libgame.zdat" "${SM_PACK_DIR}/libgame.json"
    COMMAND python3 "${SM_ROOT}/tools/pack-libgame.py" "$<TARGET_FILE:game>" "${CMAKE_STRIP}" "${SM_XEX}"
            "${SM_PACK_DIR}"
    DEPENDS game "${SM_ROOT}/tools/pack-libgame.py" "${SM_ROOT}/src/xex_image.cpp" "${SM_XEX}"
    COMMENT "libgame.so -> assets/libgame.zdat (encriptado)"
    VERBATIM)
add_custom_target(game_pack DEPENDS "${SM_PACK_DIR}/libgame.zdat" "${SM_PACK_DIR}/libgame.json")

add_library(main SHARED "${CMAKE_CURRENT_LIST_DIR}/game_lib_jni.cpp" "${SM_ROOT}/src/xex_image.cpp")
target_include_directories(main PRIVATE "${SM_ROOT}/src" "${REXSDK_ROOT}/thirdparty")
target_link_libraries(main PRIVATE mspack log)
target_compile_options(main PRIVATE -march=armv8.2-a)
# O Gradle so compila os alvos da lista (build.gradle.kts): o libgame.so e o pacote vem por aqui.
add_dependencies(main game_pack)
