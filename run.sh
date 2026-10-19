#!/usr/bin/env bash
# Build an app and run its .g3a in emuCasioFXCG.
#   ./run.sh                     ask which app to run
#   ./run.sh base-n              run apps/base-n
#   ./run.sh base-n --scale 3    extra options are passed to the emulator
# Set EMUCASIO if the emulator checkout is not in ~/VSCode/emuCasioFxCG.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
EMU="${EMUCASIO:-$HOME/VSCode/emuCasioFxCG}/run.sh"

if [[ ! -x "$EMU" ]]; then
  echo "emuCasio not found at $EMU (set EMUCASIO to its folder)" >&2
  exit 1
fi

app=""
if [[ $# -ge 1 && "$1" != -* ]]; then
  app="$1"; shift
else
  APPS=(); for d in "$ROOT"/apps/*/; do APPS+=("$(basename "$d")"); done
  PS3="Run which app? "
  select app in "${APPS[@]}"; do [[ -n "$app" ]] && break; done
  [[ -n "$app" ]] || exit 0  # Ctrl+D
fi

if [[ ! -d "$ROOT/apps/$app" ]]; then
  echo "No app '$app' in apps/" >&2
  exit 1
fi

# Rebuild (incremental); quiet unless it fails
echo "==> building $app"
if ! out=$("$ROOT/scripts/build.sh" "$app" 2>&1); then
  echo "$out" >&2
  exit 1
fi

g3a=("$ROOT/apps/$app"/*.g3a)
echo "==> running ${g3a[0]#$ROOT/}"
# The emulator keeps its storage/ folder in the current directory
cd "$ROOT"
exec "$EMU" "$@" "${g3a[0]}"
