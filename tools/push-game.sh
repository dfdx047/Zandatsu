#!/usr/bin/env bash
# Copia o jogo (assets/default.xex e assets/media) para o aparelho por adb, na
# pasta que o app le: /sdcard/Android/data/<pacote>/files/game.
#
# O app tem de ter sido aberto uma vez antes (o Android so cria a pasta
# Android/data/<pacote> depois da instalacao/primeira abertura).
# Sao ~8 GB: demora alguns minutos por USB.
#
# Uso (a partir da raiz do repositorio, com um aparelho ligado):
#   bash tools/push-game.sh              (aparelho unico)
#   bash tools/push-game.sh SERIAL       (varios aparelhos: ver adb devices)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=versions.env
source "$ROOT/tools/versions.env"
ADB=(adb)
[ -n "${1:-}" ] && ADB=(adb -s "$1")
DEST="/sdcard/Android/data/$APP_ID/files/game"

if [ ! -f "$ROOT/assets/default.xex" ] || [ ! -d "$ROOT/assets/media" ]; then
  echo "ERRO: falta assets/default.xex ou assets/media (ver README)." >&2
  exit 1
fi

"${ADB[@]}" shell mkdir -p "$DEST"
echo "== default.xex"
"${ADB[@]}" push "$ROOT/assets/default.xex" "$DEST/default.xex"
echo "== media/ (~8 GB)"
"${ADB[@]}" push "$ROOT/assets/media" "$DEST/"
echo
echo "Jogo copiado para $DEST"
