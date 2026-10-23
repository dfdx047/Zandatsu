#!/usr/bin/env bash
# Aplica as alteracoes Android do Renut (patches/renut/NNNN-*.patch, por ordem) ao submodulo third_party/mgr-nx-deko,
# fixo no commit do upstream. Os ja aplicados sao saltados (verificacao inversa, sem carimbo: um checkout limpo do
# submodulo volta a receber todos).
#
# Mesma regra dos patches do SDK: nunca alterar um patch publicado; uma correcao entra num patch novo.
#
# Uso (a partir da raiz do repositorio):
#   bash tools/apply-renut-patches.sh
#   bash tools/apply-renut-patches.sh --check   # so verifica (codigo 1 se faltar algum; usado pelo CMake)
set -euo pipefail
CHECK=0
[ "${1:-}" = "--check" ] && CHECK=1

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SUB="$ROOT/third_party/mgr-nx-deko"

if [ ! -f "$SUB/app/src/renut_engine/gpu/gpu.cmake" ]; then
  echo "ERRO: submodulo vazio em $SUB. Corre: git submodule update --init third_party/mgr-nx-deko" >&2
  exit 1
fi

cd "$SUB"
for PATCH in "$ROOT"/patches/renut/[0-9][0-9][0-9][0-9]-*.patch; do
  [ -f "$PATCH" ] || continue
  NAME="$(basename "$PATCH")"
  if patch -p1 --dry-run --reverse --force --silent < "$PATCH" > /dev/null 2>&1; then
    continue
  fi
  if ! TEST="$(patch -p1 --dry-run --forward < "$PATCH" 2>&1)"; then
    # Ja aplicado, mas um patch posterior mexeu nas mesmas linhas (a verificacao inversa falha): o patch diz
    # "previously applied" em todos os ficheiros.
    FILES="$(grep -c '^checking file' <<< "$TEST" || true)"
    APPLIED="$(grep -c 'previously applied' <<< "$TEST" || true)"
    if [ "$FILES" -gt 0 ] && [ "$FILES" = "$APPLIED" ]; then
      continue
    fi
    echo "ERRO: renut/$NAME nao aplica de forma limpa. Saida do teste:" >&2
    echo "$TEST" >&2
    exit 1
  fi
  if [ "$CHECK" = 1 ]; then
    echo "falta: renut/$NAME (corre: bash tools/apply-renut-patches.sh)" >&2
    exit 1
  fi
  patch -p1 --forward --silent --no-backup-if-mismatch < "$PATCH"
  echo "aplicado: renut/$NAME"
done
