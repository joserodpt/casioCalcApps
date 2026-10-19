#!/usr/bin/env bash
# Record a demo GIF and screenshots of one app or all apps in emuCasioFXCG.
#   scripts/screenshots.sh            -> every app with screenshots/demo.script
#   scripts/screenshots.sh base-n     -> only apps/base-n
# Each apps/<app>/screenshots/demo.script lists emulator steps (keys,
# "wait 1s", "shot name.png"); the run writes demo.gif and the shots there.
# An fx-demo.script does the same for the .g1a (fx-*.png, no GIF).
# Set EMUCASIO if the emulator checkout is not in ~/VSCode/emuCasioFxCG.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EMUDIR="${EMUCASIO:-$HOME/VSCode/emuCasioFxCG}"
EMU="$EMUDIR/build-linux/emucasiofxcg"
[[ "$(uname)" == Darwin ]] && EMU="$EMUDIR/build/emucasiofxcg"

if [[ ! -x "$EMU" ]]; then
  echo "emucasiofxcg not built at $EMU (run the emulator's run.sh once)" >&2
  exit 1
fi
if [[ -d "$HOME/.local/casio-deps/lib" ]]; then
  export LD_LIBRARY_PATH="$HOME/.local/casio-deps/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

if [ $# -ge 1 ]; then APPS=("$@"); else
  APPS=(); for d in "$ROOT"/apps/*/; do APPS+=("$(basename "$d")"); done
fi

# The emulator keeps a storage/ folder in its working directory: use a fresh
# one per app so runs always start from the same state
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT

# run_script <app> <script> <add-in> [--record gif]: shots are relative to
# the app's screenshots folder
run_script() {
  local app=$1 script=$2 addin=$3; shift 3
  local out="$ROOT/apps/$app/screenshots"
  sed -E "s#^([[:space:]]*shot[[:space:]]+)([^/])#\1$out/\2#" "$script" > "$TMP/$app.script"
  rm -rf "$TMP/$app"; mkdir -p "$TMP/$app"
  (cd "$TMP/$app" && "$EMU" --headless --clock "2026-01-01 12:00" \
    --script "$TMP/$app.script" "$@" "$addin" >/dev/null)
}

fail=0
for app in "${APPS[@]}"; do
  out="$ROOT/apps/$app/screenshots"
  [[ -f "$out/demo.script" ]] || { echo "-- $app: no screenshots/demo.script"; continue; }
  g3a=("$ROOT/apps/$app"/*.g3a)
  [[ -f "${g3a[0]}" ]] || { echo "!! $app: no .g3a (run scripts/build.sh)"; fail=1; continue; }
  echo "==> $app"
  find "$out" -maxdepth 1 -name '*.png' ! -name 'fx-*' -delete
  rm -f "$out/demo.gif"
  run_script "$app" "$out/demo.script" "${g3a[0]}" --record "$out/demo.gif" \
    || { echo "!! $app failed"; fail=1; }

  # The fx-9860G version, if the app has one: fx-*.png from fx-demo.script
  [[ -f "$out/fx-demo.script" ]] || continue
  g1a=("$ROOT/apps/$app"/*.g1a)
  [[ -f "${g1a[0]}" ]] || { echo "!! $app: no .g1a (run scripts/build.sh)"; fail=1; continue; }
  echo "==> $app (fx-9860G)"
  rm -f "$out"/fx-*.png
  run_script "$app" "$out/fx-demo.script" "${g1a[0]}" \
    || { echo "!! $app (fx-9860G) failed"; fail=1; }
done
exit $fail
