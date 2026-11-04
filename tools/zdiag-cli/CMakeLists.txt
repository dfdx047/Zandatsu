# zdiag_cli: as mesmas fontes da libzdiag.so (app/src/main/cpp/diag) num executavel para adb shell.
# Ver README.md.
cmake_minimum_required(VERSION 3.22)
project(zdiag_cli LANGUAGES C CXX)

get_filename_component(SM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Opcional: -DZDIAG_ADRENOTOOLS_SRC=<.../_deps/adrenotools-src> para testar drivers alternativos
# (precisa tambem das libs de hook numa pasta: native_lib_dir=...).
set(ZDIAG_ADRENOTOOLS_SRC "" CACHE PATH "fontes do libadrenotools (opcional)")
if(ZDIAG_ADRENOTOOLS_SRC)
    if(NOT CMAKE_ANDROID_ARCH_ABI)
        set(CMAKE_ANDROID_ARCH_ABI "${ANDROID_ABI}")
    endif()
    add_subdirectory("${ZDIAG_ADRENOTOOLS_SRC}" adrenotools)
endif()

set(ZDIAG_CLI ON)
set(ZDIAG_CLI_MAIN "${CMAKE_CURRENT_LIST_DIR}/main.cpp")
include("${SM_ROOT}/app/src/main/cpp/diag/diag.cmake")
