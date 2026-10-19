#!/usr/bin/env bash
# Junta ao mgr_overrides.toml ([functions]) as funcoes chamadas por ponteiro que o
# codegen nao encontrou ("UNREGISTERED indirect call: target=0x...").
#
# Le o log da ultima sessao do aparelho (por adb) ou os ficheiros indicados (logs
# ou zips do "Exportar logs"). So acrescenta enderecos que ainda nao estao no toml.
# Depois: bash tools/codegen.sh e compilar o APK de novo.
#
# Para apanhar varias de uma vez, liga no cvars.txt do aparelho:
#   env:DBZ3_COLLECT_UNREGISTERED = 1
# (o runtime regista cada uma e continua em vez de fechar; o jogo pode ficar
# instavel depois da primeira, mas a lista sai completa ate onde chegar).
#
# Uso (a partir da raiz do repositorio):
#   bash tools/collect-unregistered.sh                 (puxa o log por adb)
#   bash tools/collect-unregistered.sh SwordMan-logs-*.zip
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=versions.env
source "$ROOT/tools/versions.env"
TOML="$ROOT/mgr_overrides.toml"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ "$#" -eq 0 ]; then
  # A pasta inteira: o log roda em partes de 5 MB (swordman_last.N.log) quando ha
  # muitas mensagens, e as primeiras funcoes em falta ficam nas partes antigas.
  adb pull "/sdcard/Android/data/$APP_ID/files/logs" "$TMP/pulled" > /dev/null
  set -- "$TMP"/pulled/*.log
fi

: > "$TMP/all.txt"
for F in "$@"; do
  case "$F" in
    *.zip) unzip -p "$F" 'logs/*.log' 'logcat.txt' 2> /dev/null >> "$TMP/all.txt" || true ;;
    *) cat "$F" >> "$TMP/all.txt" ;;
  esac
done

grep -oE 'UNREGISTERED indirect call: target=0x[0-9A-Fa-f]{8}' "$TMP/all.txt" \
  | grep -oE '0x[0-9A-Fa-f]{8}$' | tr 'a-f' 'A-F' | sed 's/^0X/0x/' | sort -u > "$TMP/found.txt" || true

if [ ! -s "$TMP/found.txt" ]; then
  echo "Nenhuma funcao em falta no log."
  exit 0
fi

ADDED=0
while read -r A; do
  if grep -qiE "^${A}[[:space:]]*=" "$TOML"; then
    echo "ja no toml: $A"
  else
    echo "${A} = {}" >> "$TOML"
    echo "adicionada: $A"
    ADDED=$((ADDED + 1))
  fi
done < "$TMP/found.txt"

echo
echo "$ADDED funcao(oes) nova(s) em mgr_overrides.toml."
[ "$ADDED" -gt 0 ] && echo "Proximo passo: bash tools/codegen.sh e compilar o APK de novo."
exit 0
