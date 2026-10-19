#!/usr/bin/env bash
# Build one app or all apps and collect the add-ins in dist/:
# dist/g3a/ for the fx-CG (colour), dist/g1a/ for the fx-9860G (mono).
#   scripts/build.sh                 -> build all apps (.g3a)
#   scripts/build.sh base-n          -> build only apps/base-n
#   scripts/build.sh --fx [apps...]  -> also build the .g1a versions
set -uo pipefail
export PATH="$HOME/.local/bin:$PATH"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/dist/g3a" "$ROOT/dist/g1a"

fx=0
if [ "${1:-}" = "--fx" ]; then fx=1; shift; fi

if [ $# -ge 1 ]; then APPS=("$@"); else
  APPS=(); for d in "$ROOT"/apps/*/; do APPS+=("$(basename "$d")"); done
fi

fail=0
for app in "${APPS[@]}"; do
  dir="$ROOT/apps/$app"
  echo "==> $app"
  if (cd "$dir" && fxsdk build-cg); then
    cp "$dir"/*.g3a "$ROOT/dist/g3a/"
  else
    echo "!! $app failed"; fail=1
  fi
  if [ $fx = 1 ]; then
    echo "==> $app (fx-9860G)"
    if (cd "$dir" && fxsdk build-fx); then
      cp "$dir"/*.g1a "$ROOT/dist/g1a/"
    else
      echo "!! $app (fx-9860G) failed"; fail=1
    fi
  fi
done
echo; ls -1 "$ROOT"/dist/g3a/*.g3a "$ROOT"/dist/g1a/*.g1a 2>/dev/null
exit $fail
