#!/usr/bin/env bash
# Build an app and run it in emuCasioCalc, as the fx-CG (.g3a, colour) or
# the fx-9860G (.g1a, black and white) version.
#   ./run.sh                        ask which version, then which app
#   ./run.sh cg base-n              run apps/base-n's .g3a
#   ./run.sh fx base-n --scale 3    extra options are passed to the emulator
#   ./run.sh 1 12                   the menus' numbers work too (cg, 12th app)
# Only apps marked FX in their CMakeLists.txt have an fx version.
# Set EMUCASIO if the emulator checkout is not in ~/VSCode/emuCasioCalc.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
EMU="${EMUCASIO:-$HOME/VSCode/emuCasioCalc}/run.sh"

if [[ ! -x "$EMU" ]]; then
  echo "emuCasio not found at $EMU (set EMUCASIO to its folder)" >&2
  exit 1
fi

has_fx() {
  grep -Eq 'casio_app\([A-Za-z0-9_-]+ FX' "$ROOT/apps/$1/CMakeLists.txt"
}

platform=""
if [[ $# -ge 1 && ( "$1" == cg || "$1" == 1 ) ]]; then
  platform=cg; shift
elif [[ $# -ge 1 && ( "$1" == fx || "$1" == 2 ) ]]; then
  platform=fx; shift
else
  PS3="Run which version? "
  select platform in "cg (fx-CG, colour .g3a)" "fx (fx-9860G, black and white .g1a)"; do
    [[ -n "$platform" ]] && break
  done
  [[ -n "$platform" ]] || exit 0  # Ctrl+D
  platform="${platform%% *}"
fi

APPS=()
for d in "$ROOT"/apps/*/; do
  a="$(basename "$d")"
  if [[ $platform == cg ]] || has_fx "$a"; then APPS+=("$a"); fi
done

app=""
if [[ $# -ge 1 && "$1" =~ ^[0-9]+$ && ! -d "$ROOT/apps/$1" ]]; then
  if (( $1 < 1 || $1 > ${#APPS[@]} )); then
    echo "No app number $1 (1 to ${#APPS[@]})" >&2
    exit 1
  fi
  app="${APPS[$1 - 1]}"; shift
elif [[ $# -ge 1 && "$1" != -* ]]; then
  app="$1"; shift
else
  PS3="Run which app? "
  select app in "${APPS[@]}"; do [[ -n "$app" ]] && break; done
  [[ -n "$app" ]] || exit 0  # Ctrl+D
fi

if [[ ! -d "$ROOT/apps/$app" ]]; then
  echo "No app '$app' in apps/" >&2
  exit 1
fi
if [[ $platform == fx ]] && ! has_fx "$app"; then
  echo "$app has no fx-9860G version (run it with: ./run.sh cg $app)" >&2
  exit 1
fi

# Rebuild (incremental); quiet unless it fails
echo "==> building $app"
if ! out=$("$ROOT/scripts/build.sh" "$app" 2>&1); then
  echo "$out" >&2
  exit 1
fi

ext=g3a; [[ $platform == fx ]] && ext=g1a
file=("$ROOT/apps/$app"/*.$ext)
echo "==> running ${file[0]#$ROOT/}"
# The emulator keeps its storage/ folder in the current directory
cd "$ROOT"
exec "$EMU" "$@" "${file[0]}"
