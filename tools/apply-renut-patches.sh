#!/usr/bin/env bash
# Aplica as alteracoes Android do Renut (patches/renut/NNNN-*.patch, por ordem) ao submodulo third_party/mgr-nx-deko,
# fixo no commit do upstream.
#
# Quais ja estao aplicados: os ficheiros tocados pelos patches sao copiados do commit do submodulo (git show HEAD:)
# para uma pasta temporaria e os patches aplicados la um a um; o submodulo esta no estado "ate ao patch k" quando os
# ficheiros dele sao iguais aos da pasta depois do patch k. Assim um patch pode mudar linhas de um anterior (ex.:
# renut/0017 muda a tabela que o 0009 criou), o que a verificacao inversa patch a patch nao aguentava.
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

PATCHES=()
for P in "$ROOT"/patches/renut/[0-9][0-9][0-9][0-9]-*.patch; do
  [ -f "$P" ] && PATCHES+=("$P")
done
[ ${#PATCHES[@]} -eq 0 ] && exit 0

# Os ficheiros que algum patch toca.
mapfile -t FILES < <(sed -n 's#^+++ b/\([^\t]*\).*#\1#p' "${PATCHES[@]}" | sort -u)

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
cd "$SUB"
for f in "${FILES[@]}"; do
  mkdir -p "$TMP/$(dirname "$f")"
  git show "HEAD:$f" > "$TMP/$f" 2> /dev/null || rm -f "$TMP/$f"  # ficheiro novo de um patch: nao existe no commit
done

# O submodulo e igual a pasta temporaria (em todos os ficheiros tocados)?
same() {
  local f
  for f in "${FILES[@]}"; do
    if [ -e "$TMP/$f" ] || [ -e "$f" ]; then
      cmp -s "$TMP/$f" "$f" || return 1
    fi
  done
}

STATE=-1
same && STATE=0
for i in "${!PATCHES[@]}"; do
  (cd "$TMP" && patch -p1 --forward --silent --no-backup-if-mismatch < "${PATCHES[$i]}") > /dev/null || {
    echo "ERRO: renut/$(basename "${PATCHES[$i]}") nao aplica sobre os anteriores." >&2
    exit 1
  }
  same && STATE=$((i + 1))
done

if [ "$STATE" -lt 0 ]; then
  echo "ERRO: o submodulo nao corresponde a nenhum estado dos patches do Renut (alteracoes a mao?):" >&2
  for f in "${FILES[@]}"; do cmp -s "$TMP/$f" "$f" || echo "  $f" >&2; done
  exit 1
fi
[ "$STATE" -eq ${#PATCHES[@]} ] && exit 0
if [ "$CHECK" = 1 ]; then
  echo "falta: renut/$(basename "${PATCHES[$STATE]}") e seguintes (corre: bash tools/apply-renut-patches.sh)" >&2
  exit 1
fi
for P in "${PATCHES[@]:$STATE}"; do
  patch -p1 --forward --silent --no-backup-if-mismatch < "$P"
  echo "aplicado: renut/$(basename "$P")"
done
