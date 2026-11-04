# zdiag: diagnostico Vulkan autonomo (libzdiag.so), para os jogadores com GPUs problematicas.
#
# Incluir do app/src/main/cpp/CMakeLists.txt DEPOIS do bloco do adrenotools:
#     include("${CMAKE_CURRENT_LIST_DIR}/diag/diag.cmake")
# Nao depende do jogo nem do SDK: so do NDK (Vulkan carregado em tempo de execucao, libz, liblog),
# do libadrenotools (se o alvo "adrenotools" existir) e do SPIR-V do bc_decode do Renut
# (${SM_ROOT}/out/renut/host_shaders/bc_decode_cs.spirv.h; sem ele o teste bc_decode (i) fica "skip").
#
# Variaveis:
#   SM_ROOT          raiz do repositorio (ja definida no CMakeLists do app)
#   ZDIAG_CLI        ON = constroi o executavel zdiag_cli (tools/zdiag-cli) em vez da .so
#   ZDIAG_CLI_MAIN   o main.cpp do executavel
#   ZDIAG_GLSLC      caminho do glslc (por omissao o do NDK)

set(ZDIAG_DIR "${CMAKE_CURRENT_LIST_DIR}")

if(NOT ZDIAG_GLSLC)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(_zd_host darwin-x86_64)
    elseif(CMAKE_HOST_WIN32)
        set(_zd_host windows-x86_64)
    else()
        set(_zd_host linux-x86_64)
    endif()
    find_program(ZDIAG_GLSLC glslc
        HINTS "${ANDROID_NDK}/shader-tools/${_zd_host}" "${CMAKE_ANDROID_NDK}/shader-tools/${_zd_host}"
        NO_DEFAULT_PATH)
endif()
if(NOT ZDIAG_GLSLC)
    message(FATAL_ERROR "zdiag: glslc nao encontrado (esperado em <NDK>/shader-tools/<host>/glslc)")
endif()

# GLSL -> SPIR-V (lista de palavras em C, glslc -mfmt=c), incluidos por shaders.cpp.
set(ZDIAG_SHADERS
    fs_tri.vert solid.frag
    pl_basic.vert pl_full.vert pl_inst.vert
    pl_f0.frag pl_f1.frag pl_f2.frag pl_f3.frag pl_f4.frag pl_f5.frag pl_f6.frag pl_f7.frag
    workload.comp reduce.comp bc_fetch.comp)
set(_zd_gen "${CMAKE_CURRENT_BINARY_DIR}/zdiag_shaders")
file(MAKE_DIRECTORY "${_zd_gen}")
set(_zd_incs "")
foreach(_zd_s ${ZDIAG_SHADERS})
    add_custom_command(
        OUTPUT "${_zd_gen}/${_zd_s}.inc"
        COMMAND "${ZDIAG_GLSLC}" --target-env=vulkan1.0 -O -mfmt=c -I "${ZDIAG_DIR}/shaders"
                -o "${_zd_gen}/${_zd_s}.inc" "${ZDIAG_DIR}/shaders/${_zd_s}"
        DEPENDS "${ZDIAG_DIR}/shaders/${_zd_s}" "${ZDIAG_DIR}/shaders/pl_common.glsl"
        COMMENT "zdiag: glslc ${_zd_s}"
        VERBATIM)
    list(APPEND _zd_incs "${_zd_gen}/${_zd_s}.inc")
endforeach()

set(_zd_sources
    "${ZDIAG_DIR}/core.cpp"
    "${ZDIAG_DIR}/vk_loader.cpp"
    "${ZDIAG_DIR}/gpu.cpp"
    "${ZDIAG_DIR}/shaders.cpp"
    "${ZDIAG_DIR}/test_info.cpp"
    "${ZDIAG_DIR}/test_pipelines.cpp"
    "${ZDIAG_DIR}/test_render.cpp"
    "${ZDIAG_DIR}/test_bc.cpp"
    "${ZDIAG_DIR}/test_timestamps.cpp"
    "${ZDIAG_DIR}/test_cpu.cpp"
    "${ZDIAG_DIR}/profiler.cpp")

if(ZDIAG_CLI)
    set(_zd_target zdiag_cli)
    add_executable(zdiag_cli ${_zd_sources} "${ZDIAG_CLI_MAIN}" ${_zd_incs})
    target_compile_definitions(zdiag_cli PRIVATE ZDIAG_CLI=1)
else()
    set(_zd_target zdiag)
    add_library(zdiag SHARED ${_zd_sources} "${ZDIAG_DIR}/jni.cpp" ${_zd_incs})
    # So exporta as funcoes JNI.
    set_target_properties(zdiag PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
    target_link_options(zdiag PRIVATE "-Wl,--exclude-libs,ALL")
endif()

set_source_files_properties("${ZDIAG_DIR}/shaders.cpp" PROPERTIES OBJECT_DEPENDS "${_zd_incs}")
target_include_directories(${_zd_target} PRIVATE "${ZDIAG_DIR}" "${_zd_gen}")
target_compile_features(${_zd_target} PRIVATE cxx_std_20)
target_compile_options(${_zd_target} PRIVATE -Wall -Wno-missing-field-initializers -fexceptions)
target_link_libraries(${_zd_target} PRIVATE android log z dl)

set(_zd_bc "${SM_ROOT}/out/renut/host_shaders/bc_decode_cs.spirv.h")
if(EXISTS "${_zd_bc}")
    target_include_directories(${_zd_target} PRIVATE "${SM_ROOT}/out/renut/host_shaders")
    target_compile_definitions(${_zd_target} PRIVATE ZDIAG_HAS_BC_DECODE_CS=1)
    set_property(SOURCE "${ZDIAG_DIR}/shaders.cpp" APPEND PROPERTY OBJECT_DEPENDS "${_zd_bc}")
else()
    message(WARNING "zdiag: ${_zd_bc} nao existe; bc_decode (shader do Renut) vai ficar skip")
endif()

if(TARGET adrenotools)
    target_link_libraries(${_zd_target} PRIVATE adrenotools)
    target_compile_definitions(${_zd_target} PRIVATE ZDIAG_HAS_ADRENOTOOLS=1)
endif()
