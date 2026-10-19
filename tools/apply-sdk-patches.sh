#!/usr/bin/env bash
# Aplica os patches Android (patches/NNNN-*.patch, por ordem) ao SDK em
# external/rexglue-sdk (preparado por tools/setup-sdk.sh, que chama este script).
#
# 0000-0098 sao os patches do Dragon Burst (com os do Dragon Fury refeitos sobre
# este SDK). Os do MGR comecam no 0100.
# Pode ser executado sempre que houver patches novos: os ja aplicados sao saltados.
#
# Regra: nunca alterar um patch ja publicado. Uma correcao entra sempre num patch novo
# (o script marca os aplicados em .swordman-patches e ignoraria a versao nova).
#
# Uso (a partir da raiz do repositorio):
#   bash tools/apply-sdk-patches.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK="$ROOT/external/rexglue-sdk"

if [ ! -d "$SDK/src" ] || [ ! -f "$SDK/thirdparty/sdl3/CMakeLists.txt" ]; then
  echo "ERRO: SDK nao preparado em $SDK. Corre primeiro: bash tools/setup-sdk.sh" >&2
  exit 1
fi

STAMP="$SDK/.swordman-patches"
touch "$STAMP"
cd "$SDK"
for PATCH in "$ROOT"/patches/[0-9][0-9][0-9][0-9]-*.patch; do
  [ -f "$PATCH" ] || continue
  NAME="$(basename "$PATCH")"
  if grep -qxF "$NAME" "$STAMP"; then
    echo "ja aplicado: $NAME"
    continue
  fi
  if patch -p1 --dry-run --reverse --force --silent < "$PATCH" > /dev/null 2>&1; then
    echo "$NAME" >> "$STAMP"
    echo "ja aplicado: $NAME"
    continue
  fi
  TEST="$(patch -p1 --dry-run --forward < "$PATCH" 2>&1 || true)"
  if ! patch -p1 --dry-run --forward --silent < "$PATCH" > /dev/null 2>&1; then
    FILES="$(grep -c '^checking file' <<< "$TEST" || true)"
    APPLIED="$(grep -c 'previously applied' <<< "$TEST" || true)"
    if [ "$FILES" -gt 0 ] && [ "$FILES" = "$APPLIED" ]; then
      echo "$NAME" >> "$STAMP"
      echo "ja aplicado: $NAME"
      continue
    fi
    echo "ERRO: $NAME nao aplica de forma limpa. Saida do teste:" >&2
    echo "$TEST" >&2
    exit 1
  fi
  patch -p1 --forward --silent --no-backup-if-mismatch < "$PATCH"
  echo "$NAME" >> "$STAMP"
  echo "aplicado:    $NAME"
done
echo "Patches aplicados. Proximo passo: bash tools/codegen.sh (so na primeira vez ou com o default.xex novo)."
