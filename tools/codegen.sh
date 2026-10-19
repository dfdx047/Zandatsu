#!/usr/bin/env bash
# Gera o codigo recompilado (generated/default) a partir de assets/default.xex,
# com o rexglue do PC (o SDK v0.10.0 instalado, ver README).
#
# Confere o SHA-1 do default.xex (tools/versions.env): os enderecos de
# mgr_overrides.toml e dos hooks so valem para esse executavel.
#
# Uso (a partir da raiz do repositorio):
#   bash tools/codegen.sh            (gera de novo so se algo mudou)
#   bash tools/codegen.sh --force    (gera mesmo com erros de validacao)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=versions.env
source "$ROOT/tools/versions.env"
XEX="$ROOT/assets/default.xex"
REXGLUE="${REXGLUE:-rexglue}"

if ! command -v "$REXGLUE" > /dev/null; then
  echo "ERRO: rexglue nao encontrado no PATH (ou define REXGLUE=/caminho/rexglue)." >&2
  exit 1
fi
if [ ! -f "$XEX" ]; then
  echo "ERRO: falta assets/default.xex (extrai o jogo para assets/, ver README)." >&2
  exit 1
fi
SHA="$(sha1sum "$XEX" | cut -d' ' -f1)"
if [ "$SHA" != "$XEX_SHA1" ]; then
  echo "ERRO: default.xex com SHA-1 $SHA, esperado $XEX_SHA1." >&2
  echo "  O projeto usa o disco 'Metal Gear Rising - Revengeance (USA, Europe)'." >&2
  exit 1
fi

cd "$ROOT"
FORCE=()
[ "${1:-}" = "--force" ] && FORCE=(--force)
"$REXGLUE" "${FORCE[@]}" codegen mgr_manifest.toml 2>&1 | tee codegen.log
if [ ! -f "$ROOT/generated/default/sources.cmake" ]; then
  echo "ERRO: o codegen nao gerou generated/default/sources.cmake (ver codegen.log)." >&2
  exit 1
fi
COUNT="$(grep -c '\.cpp' "$ROOT/generated/default/sources.cmake" || true)"
echo
echo "Codigo recompilado pronto: $COUNT ficheiros em generated/default."
