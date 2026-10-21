#!/usr/bin/env bash
# Um comando so: atualiza, aplica os patches novos, refaz o codegen SO se for preciso,
# compila o APK e instala nos aparelhos ligados.
#
# Uso (a partir da raiz do repositorio):
#   bash tools/build.sh             # tudo e instala em todos os aparelhos
#   bash tools/build.sh cube        # instala so no RG Cube (rp5, odin, ou parte do serial)
#   bash tools/build.sh none        # compila mas nao instala
#   PULL=0 bash tools/build.sh      # sem git pull
#
# O codegen (o passo mais lento) so corre quando muda algo que o afeta: o
# mgr_manifest.toml, o mgr_overrides.toml, os patches do SDK (o rexglue e
# compilado a partir dele) ou o default.xex. Fica um carimbo em generated/.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
TARGET="${1:-all}"
START=$(date +%s)

step() { echo; echo "== $*"; }

if [ "${PULL:-1}" = "1" ]; then
  step "git pull"
  BRANCH="$(git rev-parse --abbrev-ref HEAD)"
  if [ "$BRANCH" != "main" ]; then
    echo "ERRO: estas no branch '$BRANCH', o build usa o main." >&2
    echo "  git stash        (se tiveres alteracoes por gravar)" >&2
    echo "  git checkout main" >&2
    echo "  (ou PULL=0 bash tools/build.sh para compilar este branch sem atualizar)" >&2
    exit 1
  fi
  git pull --ff-only origin main
fi

step "patches do SDK"
LOG="$(mktemp)"
if ! bash tools/apply-sdk-patches.sh > "$LOG" 2>&1; then
  cat "$LOG"; rm -f "$LOG"; exit 1
fi
grep -v "^ja aplicado" "$LOG" || true
rm -f "$LOG"

# Carimbo do codegen: tudo o que muda o codigo gerado.
STAMP_FILE="generated/.codegen-stamp"
STAMP="$(cat mgr_manifest.toml mgr_overrides.toml patches/*.patch assets/default.xex 2>/dev/null | sha1sum | cut -d' ' -f1)"
if [ ! -f generated/default/sources.cmake ] || [ "$(cat "$STAMP_FILE" 2>/dev/null)" != "$STAMP" ]; then
  step "codegen (mudou o manifesto, os overrides, os patches ou o default.xex)"
  bash tools/codegen.sh --force
  echo "$STAMP" > "$STAMP_FILE"
else
  step "codegen: nada mudou, saltado"
fi

step "APK"
./gradlew :app:assembleDebug --console=plain -q
APK="app/build/outputs/apk/debug/app-debug.apk"
echo "APK: $(du -h "$APK" | cut -f1), $(date -r "$APK" '+%d/%m %H:%M')"

if [ "$TARGET" != "none" ]; then
  step "instalar ($TARGET)"
  bash tools/install.sh "$TARGET"
fi

echo
echo "Pronto em $(( $(date +%s) - START )) s."
