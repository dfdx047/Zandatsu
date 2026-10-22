#!/usr/bin/env bash
# Teste automatico nos aparelhos ligados por adb, todos em paralelo, e UM zip no fim.
#
# Uma vez por aparelho, grava o caminho do titulo ate ao gameplay (os botoes do comando):
#   bash tools/autotest.sh record rp5
#   bash tools/autotest.sh record cube
#
# Depois, cada rodada de teste e um comando so:
#   bash tools/autotest.sh                          # a config atual, nos aparelhos ligados
#   bash tools/autotest.sh --build                  # compila e instala antes (build.sh all)
#   bash tools/autotest.sh base fp16:swordman_ps_fp16=true
#   bash tools/autotest.sh base 'rp5/sysmem:env:TU_DEBUG=sysmem' 'cube/fp16:swordman_ps_fp16=true'
#                                                   # duas variantes seguidas em cada aparelho
#   bash tools/autotest.sh --only cube ...          # so um aparelho
#   bash tools/autotest.sh --settle 45 ...          # segundos no gameplay antes do autoteste
#
# Variante: "nome" (config atual) ou "nome:cvar=valor;cvar=valor" (cvars.txt do aparelho so
# nessa rodada; "env:VAR=valor" para variaveis do driver, ex. env:TU_DEBUG=sysmem). Com
# "rp5/nome:..." a variante so corre nesse aparelho. Em cada aparelho e variante: fecha o jogo, abre-o, repete o caminho gravado,
# espera, corre o autoteste do render (que exporta o zip) e puxa o zip. No fim junta tudo em
# ~/Downloads/SwordMan-autotest-<data>.zip - e so mandar esse ficheiro.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=versions.env
source "$ROOT/tools/versions.env"
APPDIR="/sdcard/Android/data/$APP_ID/files"
ACTION="$APP_ID.CMD"

BUILD=0
ONLY=""
SETTLE=30
CAPTURE=false
VARIANTS=()
MODE="test"
REC_TARGET=""
while [ $# -gt 0 ]; do
  case "$1" in
    record) MODE="record"; REC_TARGET="${2:-}"; shift $(( $# > 1 ? 2 : 1 )); continue ;;
    --build) BUILD=1 ;;
    --only) ONLY="$2"; shift ;;
    --settle) SETTLE="$2"; shift ;;
    --capture) CAPTURE=true ;;
    *) VARIANTS+=("$1") ;;
  esac
  shift
done
[ ${#VARIANTS[@]} -eq 0 ] && VARIANTS=("base")

if ! command -v adb > /dev/null; then
  echo "ERRO: adb nao encontrado (pacote android-tools)." >&2
  exit 1
fi

# "serial rotulo" de cada aparelho ligado (rp5, cube, odin ou o serial).
devices() {
  adb devices | awk 'NR > 1 && $2 == "device" { print $1 }' | while read -r serial; do
    local model dev lower label
    model="$(adb -s "$serial" shell getprop ro.product.model < /dev/null | tr -d '\r')"
    dev="$(adb -s "$serial" shell getprop ro.product.device < /dev/null | tr -d '\r')"
    lower="$(echo "$model $dev $serial" | tr '[:upper:]' '[:lower:]')"
    case "$lower" in
      *pocket*5*|*rp5*|*kona*) label=rp5 ;;
      *cube*) label=cube ;;
      *odin*) label=odin ;;
      *) label="$serial" ;;
    esac
    if [ -z "$ONLY" ] || [ "$label" = "$ONLY" ] || [ "$serial" = "$ONLY" ]; then
      echo "$serial $label"
    fi
  done
}

cmd() {  # cmd <serial> <comando> [extras do am]
  local serial="$1" c="$2"
  shift 2
  adb -s "$serial" shell am broadcast -a "$ACTION" -p "$APP_ID" --es cmd "'$c'" "$@" > /dev/null
}

start_game() {
  local serial="$1"
  adb -s "$serial" shell am force-stop "$APP_ID"
  sleep 1
  adb -s "$serial" shell am start -n "$APP_ID/.game.GameActivity" -a "$APP_ID.PLAY" > /dev/null
  # O receptor do comando e registado com o jogo ja aberto.
  sleep 4
}

latest_zip() {
  adb -s "$1" shell "ls -t /sdcard/Download/SwordMan-logs-*.zip 2>/dev/null | head -n 1" | tr -d '\r'
}

if [ "$MODE" = "record" ]; then
  mapfile -t DEVS < <(ONLY="$REC_TARGET" devices)
  if [ ${#DEVS[@]} -ne 1 ]; then
    echo "ERRO: diga qual aparelho gravar (bash tools/autotest.sh record rp5|cube)." >&2
    exit 1
  fi
  read -r SERIAL LABEL <<< "${DEVS[0]}"
  echo "== $LABEL: a abrir o jogo"
  start_game "$SERIAL"
  cmd "$SERIAL" rec-start
  echo "Use o comando do aparelho para ir do titulo ate ao gameplay (carregar o save)."
  echo "Quando estiver a jogar, carregue Enter aqui."
  read -r
  cmd "$SERIAL" rec-stop
  sleep 1
  echo "Caminho gravado:"
  adb -s "$SERIAL" shell cat "$APPDIR/autoplay.seq" | tr -d '\r' | paste -sd' ' | cut -c1-300
  adb -s "$SERIAL" shell am force-stop "$APP_ID"
  exit 0
fi

if [ "$BUILD" = 1 ]; then
  bash "$ROOT/tools/build.sh" "${ONLY:-all}"
fi

# Limpeza (21/10 ~20h: desde a rodada 9 os resultados chegavam com ficheiros vazios - o /tmp,
# que no CachyOS e RAM, enchia com as pastas de cada rodada que nunca eram apagadas):
# pastas temporarias de rodadas antigas (deste script e do autoloop) e zips antigos em ~/Downloads.
cleanup_old() {
  local tmp="${TMPDIR:-/tmp}" d
  for d in "$tmp"/tmp.*; do
    [ -d "$d" ] || continue
    # So pastas nossas: com SwordMan-autotest-* (autotest) ou <data>/commit.txt (autoloop), e com
    # mais de 90 min (a rodada atual do autoloop fica).
    if ls -d "$d"/SwordMan-autotest-* > /dev/null 2>&1 || ls "$d"/20*/commit.txt > /dev/null 2>&1; then
      if [ -n "$(find "$d" -maxdepth 0 -mmin +90)" ]; then rm -rf "$d"; fi
    fi
  done
  ls -1t "$HOME"/Downloads/SwordMan-autotest-*.zip "$HOME"/Downloads/SwordMan-autotest-*.tar.gz 2>/dev/null |
    tail -n +7 | while read -r f; do rm -f "$f"; done
  echo "Espaco livre: /tmp $(df -h "$tmp" | awk 'NR == 2 { print $4 }'), ~/Downloads $(df -h "$HOME/Downloads" 2>/dev/null | awk 'NR == 2 { print $4 }')"
}
cleanup_old

STAMP="$(date +%Y%m%d-%H%M%S)"
OUT="$(mktemp -d)/SwordMan-autotest-$STAMP"
mkdir -p "$OUT"
mapfile -t DEVS < <(devices)
if [ ${#DEVS[@]} -eq 0 ]; then
  echo "ERRO: nenhum aparelho ligado." >&2
  exit 1
fi

# Driver Vulkan por variante: "@driver=system" (o do aparelho) ou "@driver=dono/repo/texto"
# (o primeiro .zip das releases desse repositorio do GitHub cujo nome/tag contem o texto; fica em
# cache em ~/.cache/swordman-drivers). O driver do app e reposto no fim da variante.
fetch_driver() {  # fetch_driver dono/repo/texto -> caminho do .so (ou vazio)
  # dono/repo/texto ("#" nao serve: o autoloop corta o plano no "#" como comentario - 21/10 16:53)
  local spec="$1" repo match cache url dir so
  repo="$(echo "$spec" | cut -d/ -f1-2)"
  match="$(echo "$spec" | cut -d/ -f3-)"
  cache="$HOME/.cache/swordman-drivers"
  mkdir -p "$cache"
  # Diagnostico no log (stderr): na rodada de 21/10 15:49 nada foi encontrado sem dizer porque.
  local api="$cache/releases-$(echo "$repo" | tr '/' '_').json" auth=()
  if command -v gh > /dev/null && gh auth token > /dev/null 2>&1; then
    auth=(-H "Authorization: Bearer $(gh auth token)")
  fi
  curl -sS "${auth[@]}" "https://api.github.com/repos/$repo/releases?per_page=60" -o "$api" >&2 || true
  url="$(python3 - "$match" "$api" <<'PY' || true
import json, sys
m, path = sys.argv[1].lower(), sys.argv[2]
try:
    rels = json.load(open(path))
except Exception as e:
    print("   driver: resposta da API ilegivel: %s" % e, file=sys.stderr); sys.exit()
if not isinstance(rels, list):
    print("   driver: API do GitHub: %s" % str(rels.get("message", rels))[:200], file=sys.stderr); sys.exit()
names = []
for rel in rels:
    for a in rel.get("assets", []):
        n = a["name"]
        names.append(rel["tag_name"] + " / " + n)
        if n.lower().endswith(".zip") and (m in n.lower() or m in rel["tag_name"].lower()):
            print(a["browser_download_url"]); sys.exit()
print("   driver: %d releases, nenhum .zip com '%s'. Primeiros: %s" % (len(rels), m, "; ".join(names[:12])), file=sys.stderr)
PY
)"
  [ -z "$url" ] && return 0
  dir="$cache/$(echo "$url" | md5sum | cut -c1-12)"
  if [ ! -d "$dir" ]; then
    mkdir -p "$dir"
    curl -sSL "$url" -o "$dir/driver.zip" >&2 && unzip -q -o "$dir/driver.zip" -d "$dir" >&2 ||
      { echo "   driver: falhou baixar/extrair $url" >&2; rm -rf "$dir"; return 0; }
  fi
  so="$(find "$dir" -name '*.so' | head -n 1 || true)"
  if [ -z "$so" ]; then echo "   driver: sem .so em $(basename "$url")" >&2; fi
  if [ -n "$so" ]; then echo "$so|$(basename "$url")"; fi
}

set_driver() {  # set_driver serial spec
  local serial="$1" spec="$2" got so
  adb -s "$serial" shell "rm -rf $APPDIR/driver.bak; [ -d $APPDIR/driver ] && mv $APPDIR/driver $APPDIR/driver.bak; mkdir -p $APPDIR/driver"
  if [ "$spec" = "system" ]; then
    echo "   driver: o do sistema"
    return
  fi
  got="$(fetch_driver "$spec" || true)"
  if [ -z "$got" ]; then
    echo "   driver: nada encontrado para '$spec' - fica o do sistema"
    return
  fi
  so="${got%%|*}"
  adb -s "$serial" push "$so" "$APPDIR/driver/" > /dev/null
  echo "   driver: ${got#*|} ($(basename "$so"))"
}

restore_driver() {
  adb -s "$1" shell "[ -d $APPDIR/driver.bak ] && { rm -rf $APPDIR/driver; mv $APPDIR/driver.bak $APPDIR/driver; }; true"
}

run_device() {
  local serial="$1" label="$2"
  local log="$OUT/$label.txt"
  for v in "${VARIANTS[@]}"; do
    # "rp5/nome:..." = so nesse aparelho.
    if [[ "${v%%:*}" == */* ]]; then
      [ "${v%%/*}" = "$label" ] || continue
      v="${v#*/}"
    fi
    local name="${v%%:*}" cvars="" driver=""
    [[ "$v" == *:* ]] && cvars="${v#*:}"
    # "@driver=..." sai das cvars e troca o driver so nesta variante.
    if [[ "$cvars" == *@driver=* ]]; then
      driver="${cvars#*@driver=}"
      driver="${driver%%;*}"
      cvars="$(echo "$cvars" | tr ';' '\n' | grep -v '^@driver=' | paste -sd';' || true)"
    fi
    {
      echo "== $label / $name ($(date +%H:%M:%S))"
      # Sempre sem vibracao (os aparelhos ficam na mesa o dia todo) + as cvars da variante.
      # (if, nao "[ ] &&": com set -e + pipefail um teste falso no fim do grupo parava o script -
      # as rodadas de 21/10 08:11-13:35 nao correram nenhum teste)
      {
        echo "vibration = false"
        if [ -n "$cvars" ]; then echo "$cvars" | tr ';' '\n' | sed 's/=/ = /'; fi
      } | adb -s "$serial" shell "cat > $APPDIR/cvars.txt"
      if [ -n "$cvars" ]; then echo "   cvars.txt: $cvars"; fi
      if [ -n "$driver" ]; then set_driver "$serial" "$driver"; fi
      local before
      before="$(latest_zip "$serial")"
      start_game "$serial"
      cmd "$serial" auto --ei settle "$SETTLE" --ez capture "$CAPTURE"
      local t=0 zip=""
      while [ $t -lt 900 ]; do
        sleep 10
        t=$((t + 10))
        zip="$(latest_zip "$serial")"
        if [ -n "$zip" ] && [ "$zip" != "$before" ]; then
          sleep 3
          break
        fi
        zip=""
        if ! adb -s "$serial" shell pidof "$APP_ID:game" > /dev/null; then
          echo "   o jogo fechou (crash?) - a puxar os logs e o logcat"
          adb -s "$serial" pull "$APPDIR/logs" "$OUT/$label-$name-logs" > /dev/null 2>&1 || true
          adb -s "$serial" logcat -d -t 3000 > "$OUT/$label-$name-logcat.txt" 2>/dev/null || true
          break
        fi
      done
      if [ -n "$zip" ]; then
        adb -s "$serial" pull "$zip" "$OUT/$label-$name.zip" > /dev/null
        echo "   zip: $label-$name.zip ($t s)"
      elif [ $t -ge 900 ]; then
        echo "   sem zip em 15 min (jogo preso?) - a puxar os logs, o logcat e a tela"
        adb -s "$serial" pull "$APPDIR/logs" "$OUT/$label-$name-logs" > /dev/null 2>&1 || true
        adb -s "$serial" logcat -d -t 3000 > "$OUT/$label-$name-logcat.txt" 2>/dev/null || true
        adb -s "$serial" exec-out screencap -p > "$OUT/$label-$name-tela.png" 2>/dev/null || true
      fi
      adb -s "$serial" shell rm -f "$APPDIR/cvars.txt"
      adb -s "$serial" shell am force-stop "$APP_ID"
      if [ -n "$driver" ]; then restore_driver "$serial"; fi
    } >> "$log" 2>&1
  done
}

echo "Aparelhos: $(printf '%s ' "${DEVS[@]}" | awk '{for (i = 2; i <= NF; i += 2) printf "%s ", $i}')"
echo "Variantes: ${VARIANTS[*]}  (gameplay ${SETTLE} s antes do autoteste)"
for d in "${DEVS[@]}"; do
  read -r SERIAL LABEL <<< "$d"
  run_device "$SERIAL" "$LABEL" &
done
wait
cat "$OUT"/*.txt
FINAL="$HOME/Downloads/SwordMan-autotest-$STAMP.zip"
mkdir -p "$HOME/Downloads"
if command -v zip > /dev/null; then
  (cd "$(dirname "$OUT")" && zip -qr "$FINAL" "$(basename "$OUT")")
else
  FINAL="${FINAL%.zip}.tar.gz"
  tar -C "$(dirname "$OUT")" -czf "$FINAL" "$(basename "$OUT")"
fi
# A pasta de trabalho ja esta no zip.
rm -rf "$(dirname "$OUT")"
echo
echo "Pronto: $FINAL"
