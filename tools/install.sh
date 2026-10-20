#!/usr/bin/env bash
# Instala o APK mais recente nos aparelhos ligados por adb (USB ou Wi-Fi).
#
# Uso (a partir da raiz do repositorio):
#   bash tools/install.sh          # todos os aparelhos ligados
#   bash tools/install.sh rp5      # so o Retroid Pocket 5
#   bash tools/install.sh cube     # so o RG Cube
#   bash tools/install.sh odin     # so os AYN Odin
#
# O aparelho e reconhecido pelo modelo, pelo device e pelo numero de serie. Os dados do
# app (jogo importado, opcoes, drivers) ficam: e uma atualizacao (install -r).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APK="${APK:-$ROOT/app/build/outputs/apk/debug/app-debug.apk}"
WANT="${1:-all}"

if [ ! -f "$APK" ]; then
  echo "ERRO: APK nao encontrado ($APK). Rode antes: bash tools/build.sh" >&2
  exit 1
fi
if ! command -v adb > /dev/null; then
  echo "ERRO: adb nao encontrado (pacote android-tools)." >&2
  exit 1
fi

echo "APK: $(basename "$APK") ($(du -h "$APK" | cut -f1), $(date -r "$APK" '+%d/%m %H:%M'))"
FOUND=0
FAILED=0
while read -r SERIAL STATE _; do
  [ -z "${SERIAL:-}" ] && continue
  if [ "$STATE" != "device" ]; then
    echo "   $SERIAL esta '$STATE' (unauthorized: aceite a depuracao USB no aparelho)" >&2
    continue
  fi
  MODEL="$(adb -s "$SERIAL" shell getprop ro.product.model < /dev/null | tr -d '\r')"
  DEVICE="$(adb -s "$SERIAL" shell getprop ro.product.device < /dev/null | tr -d '\r')"
  LOWER="$(echo "$MODEL $DEVICE $SERIAL" | tr '[:upper:]' '[:lower:]')"
  case "$WANT" in
    rp5)  [[ "$LOWER" == *pocket*5* || "$LOWER" == *rp5* || "$LOWER" == *kona* ]] || continue ;;
    cube) [[ "$LOWER" == *cube* ]] || continue ;;
    odin) [[ "$LOWER" == *odin* ]] || continue ;;
    all)  ;;
    *)    [[ "$LOWER" == *"$WANT"* ]] || continue ;;
  esac
  FOUND=$((FOUND + 1))
  echo "== $MODEL ($SERIAL)"
  if adb -s "$SERIAL" install -r "$APK" < /dev/null; then
    echo "   instalado"
  else
    FAILED=$((FAILED + 1))
  fi
done < <(adb devices | tail -n +2)

if [ "$FOUND" = 0 ]; then
  echo "Nenhum aparelho ligado (filtro: $WANT). Aparelhos que o adb ve:" >&2
  adb devices -l >&2
  exit 1
fi
[ "$FAILED" = 0 ]
