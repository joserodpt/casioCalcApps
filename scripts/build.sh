#!/usr/bin/env bash
# Build one app or all apps and copy every .g3a into dist/.
#   scripts/build.sh            -> build all apps
#   scripts/build.sh base-n     -> build only apps/base-n
set -uo pipefail
export PATH="$HOME/.local/bin:$PATH"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/dist"

if [ $# -ge 1 ]; then APPS=("$@"); else
  APPS=(); for d in "$ROOT"/apps/*/; do APPS+=("$(basename "$d")"); done
fi

fail=0
for app in "${APPS[@]}"; do
  dir="$ROOT/apps/$app"
  echo "==> $app"
  if (cd "$dir" && fxsdk build-cg); then
    cp "$dir"/*.g3a "$ROOT/dist/"
  else
    echo "!! $app failed"; fail=1
  fi
done
echo; ls -1 "$ROOT"/dist/*.g3a 2>/dev/null
exit $fail
