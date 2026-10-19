#!/usr/bin/env bash
# Build one app or all apps and collect the add-ins in dist/:
# dist/g3a/ for the fx-CG (colour), dist/g1a/ for the fx-9860G (mono).
# Apps marked FX in their CMakeLists.txt (casio_app(<name> FX ...)) have a
# 128x64 version and build both.
#   scripts/build.sh            -> build all apps
#   scripts/build.sh base-n     -> build only apps/base-n
set -uo pipefail
export PATH="$HOME/.local/bin:$PATH"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/dist/g3a" "$ROOT/dist/g1a"

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
  if grep -Eq 'casio_app\([A-Za-z0-9_-]+ FX' "$dir/CMakeLists.txt"; then
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
