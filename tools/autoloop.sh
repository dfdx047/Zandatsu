#!/usr/bin/env bash
# Loop de testes sem ninguem: deixe a correr com o PC ligado e os aparelhos por USB.
#
#   bash tools/autoloop.sh
#
# A cada commit novo no main (ou mudanca do plano): git pull, compila, instala nos aparelhos,
# corre o plano de testes (tools/autotest-plan.txt) com tools/autotest.sh e manda o resultado
# para o branch "autotest-results" do GitHub (runs/<data>/). Se a compilacao falhar, manda o log
# do build no lugar dos testes. Quem corrige le esse branch, faz push de um commit novo e o
# loop recomeca sozinho. Ctrl+C para parar.
#
# Antes da primeira vez: grave o caminho ate ao gameplay em cada aparelho
# (bash tools/autotest.sh record rp5 / record cube) e confirme que o git consegue fazer push.
set -uo pipefail

# O git pull do loop troca este ficheiro enquanto o bash o le: corre-se de uma copia.
if [ -z "${AUTOLOOP_ROOT:-}" ]; then
  AUTOLOOP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
  COPY="$(mktemp "${TMPDIR:-/tmp}/swordman-autoloop.XXXXXX")"
  cp "${BASH_SOURCE[0]}" "$COPY"
  AUTOLOOP_ROOT="$AUTOLOOP_ROOT" exec bash "$COPY" "$@"
fi
ROOT="$AUTOLOOP_ROOT"
cd "$ROOT"
RESULTS_DIR="${RESULTS_DIR:-$HOME/.swordman-autotest-results}"
# OptXbox no PC (para analisar as capturas .dfc): clonado/atualizado aqui se faltar.
OPTXBOX_DIR="${OPTXBOX_DIR:-$HOME/.cache/swordman-optxbox}"
if [ ! -d "$OPTXBOX_DIR/.git" ]; then
  git clone -q https://github.com/dfdx047/optxbox "$OPTXBOX_DIR" 2>/dev/null || OPTXBOX_DIR=""
fi
POLL="${POLL:-120}"
KEEP="${KEEP:-6}"
# RESULTS_REMOTE: outro repositorio (ex. privado) para os resultados; padrao = o origin.
REMOTE_URL="${RESULTS_REMOTE:-$(git remote get-url origin)}"
LAST=""

say() { echo "[$(date '+%d/%m %H:%M:%S')] $*"; }

# Aparelhos acordados e sem bloqueio de ecra enquanto estao na USB.
keep_awake() {
  adb devices | awk 'NR > 1 && $2 == "device" { print $1 }' | while read -r s; do
    adb -s "$s" shell svc power stayon usb < /dev/null > /dev/null 2>&1
    adb -s "$s" shell input keyevent KEYCODE_WAKEUP < /dev/null > /dev/null 2>&1
  done
}

# Publica a pasta $1 em runs/<nome> do branch autotest-results (so os ultimos $KEEP).
publish() {
  local src="$1" name
  name="$(basename "$src")"
  if [ ! -d "$RESULTS_DIR/.git" ]; then
    rm -rf "$RESULTS_DIR"
    git clone -q --depth 1 --branch autotest-results "$REMOTE_URL" "$RESULTS_DIR" 2>/dev/null ||
      { mkdir -p "$RESULTS_DIR" && git -C "$RESULTS_DIR" init -q &&
        git -C "$RESULTS_DIR" remote add origin "$REMOTE_URL" &&
        git -C "$RESULTS_DIR" checkout -q -b autotest-results; }
  fi
  mkdir -p "$RESULTS_DIR/runs"
  rm -rf "${RESULTS_DIR:?}/runs/$name"
  cp -r "$src" "$RESULTS_DIR/runs/$name"
  # So os ultimos $KEEP (o branch e refeito a cada envio: nada de historico pesado).
  ls -1d "$RESULTS_DIR"/runs/*/ 2>/dev/null | sort | head -n -"$KEEP" | xargs -r rm -rf
  find "$RESULTS_DIR/runs" -type f -size +90M -delete
  ls -1 "$RESULTS_DIR/runs" | sort > "$RESULTS_DIR/INDEX.txt"
  (
    cd "$RESULTS_DIR" || exit 1
    git checkout -q --orphan publish-tmp
    git add -A
    git -c user.name="SwordMan autoloop" -c user.email="autoloop@localhost" commit -qm "autotest $name"
    git branch -D autotest-results > /dev/null 2>&1
    git branch -m autotest-results
    for try in 1 2 3; do
      git push -q -f origin autotest-results && exit 0
      sleep 10
    done
    exit 1
  ) && say "resultado enviado: runs/$name" || say "ERRO: nao consegui enviar o resultado (git push)"
}

# Um loop so: dois ao mesmo tempo disputam os aparelhos e o envio (21/10 07:45).
exec 9> "${TMPDIR:-/tmp}/swordman-autoloop.lock"
if ! flock -n 9; then
  echo "ERRO: ja ha um autoloop a correr (feche o outro terminal)." >&2
  exit 1
fi

# O zip completo passa de 200 MB (o GitHub recusa ficheiros acima de 100 MB, 21/10): para o
# GitHub vai so o que se analisa - textos, logs comprimidos e as primeiras fotos de cada teste.
# O zip completo fica em ~/Downloads.
slim() {
  local zip="$1" out="$2" tmp
  tmp="$(mktemp -d)"
  mkdir -p "$out"
  case "$zip" in
    *.zip) unzip -q -o "$zip" -d "$tmp" ;;
    *) tar -C "$tmp" -xzf "$zip" ;;
  esac
  # Os zips do "Exportar logs" de cada aparelho/variante, e as pastas de logs de um crash.
  find "$tmp" -name '*.zip' | while read -r inner; do
    local name dir
    name="$(basename "$inner" .zip)"
    dir="$out/$name"
    mkdir -p "$dir"
    unzip -q -o "$inner" -d "$tmp/x-$name"
    find "$tmp/x-$name" -type f \( -name '*.txt' -o -name '*.log' -o -name '*.toml' \) \
      -exec cp {} "$dir/" \;
    find "$tmp/x-$name" -type f \( -name 'autoteste_0[0-3].jpg' -o -name 'turnip_sweep_*.jpg' -o -name 'autopilot_*.jpg' \) \
      -exec cp {} "$dir/" \;
  done
  find "$tmp" -maxdepth 2 -type d -name '*-logs' | while read -r d; do
    cp -r "$d" "$out/"
    find "$out/$(basename "$d")" -name '*.jpg' ! -name 'autoteste_0[0-3].jpg' -delete
  done
  find "$tmp" -maxdepth 2 \( -name '*.txt' -o -name '*.png' \) -exec cp {} "$out/" \;
  # Capturas de frames (.dfc, ~200 MB): nao cabem no GitHub. O mapa do frame, a analise e os
  # shaders (ucode) saem daqui pelo OptXbox e so o texto e publicado (render nativo, 22/10).
  find "$tmp" -name '*.dfc' -size +1M | while read -r dfc; do
    local name
    name="$(basename "$(dirname "$(dirname "$(dirname "$dfc")")")")-$(basename "$dfc" .dfc)"
    mkdir -p "$out/captures/$name"
    if [ -n "$OPTXBOX_DIR" ] && [ -d "$OPTXBOX_DIR/optxbox" ]; then
      (cd "$OPTXBOX_DIR" && python3 -m optxbox timeline "$dfc" > "$out/captures/$name/timeline.md" 2>&1 || true)
      (cd "$OPTXBOX_DIR" && python3 -m optxbox analyze "$dfc" > "$out/captures/$name/analyze.md" 2>&1 || true)
      (cd "$OPTXBOX_DIR" && python3 -m optxbox shaders "$dfc" --dump "$out/captures/$name/shaders" > "$out/captures/$name/shaders.log" 2>&1 || true)
    else
      echo "OptXbox nao encontrado (OPTXBOX_DIR=$OPTXBOX_DIR)" > "$out/captures/$name/ERRO.txt"
    fi
  done
  # Logs grandes comprimidos; nada acima de 40 MB.
  find "$out" -type f \( -name '*.log' -o -name '*.txt' \) -size +1M -exec gzip -9f {} \;
  find "$out" -type f -size +40M -delete
  rm -rf "$tmp"
}

say "loop iniciado (a verificar o main a cada $POLL s)"
while true; do
  git fetch -q origin main 2>/dev/null
  HEAD_NEW="$(git rev-parse origin/main 2>/dev/null)"
  if [ -n "$HEAD_NEW" ] && [ "$HEAD_NEW" != "$LAST" ]; then
    git pull -q --ff-only origin main || { say "ERRO: git pull falhou (alteracoes locais?)"; sleep "$POLL"; continue; }
    COMMIT="$(git log -1 --format='%h %s')"
    say "commit novo: $COMMIT"
    STAMP="$(date +%Y%m%d-%H%M%S)"
    RUN="$(mktemp -d)/$STAMP"
    mkdir -p "$RUN"
    echo "$COMMIT" > "$RUN/commit.txt"
    keep_awake
    if [ -n "$OPTXBOX_DIR" ]; then git -C "$OPTXBOX_DIR" pull -q --ff-only 2>/dev/null || true; fi
    if ! PULL=0 bash tools/build.sh all > "$RUN/build.log" 2>&1; then
      say "build falhou - a enviar o log"
      tail -n 400 "$RUN/build.log" > "$RUN/build-tail.log"
      publish "$RUN"
      LAST="$HEAD_NEW"
      continue
    fi
    # Aviso logo a seguir ao build (antes dos testes, que demoram): prova que o envio funciona.
    {
      echo "build ok, a testar ($(date '+%d/%m %H:%M'))"
      adb devices -l | tail -n +2
    } > "$RUN/status.txt"
    publish "$RUN"
    # Plano: uma variante por linha (# comentarios); a primeira linha "settle N" muda a espera.
    SETTLE=30
    CAPTURE_ARG=()
    VARIANTS=()
    while read -r line; do
      line="${line%%#*}"
      line="$(echo "$line" | xargs)"
      [ -z "$line" ] && continue
      if [[ "$line" == settle* ]]; then SETTLE="${line#settle }"; continue; fi
      if [[ "$line" == capture ]]; then CAPTURE_ARG=(--capture); continue; fi
      VARIANTS+=("$line")
    done < tools/autotest-plan.txt
    say "a testar: ${VARIANTS[*]}"
    bash tools/autotest.sh --settle "$SETTLE" "${CAPTURE_ARG[@]}" "${VARIANTS[@]}" > "$RUN/autotest.log" 2>&1
    ZIP="$(ls -t "$HOME"/Downloads/SwordMan-autotest-*.zip "$HOME"/Downloads/SwordMan-autotest-*.tar.gz 2>/dev/null | head -n 1)"
    [ -n "$ZIP" ] && slim "$ZIP" "$RUN/results"
    echo "testes acabados ($(date '+%d/%m %H:%M'))" >> "$RUN/status.txt"
    publish "$RUN"
    rm -rf "$(dirname "$RUN")"
    LAST="$HEAD_NEW"
  fi
  sleep "$POLL"
done
