#!/usr/bin/env bash
# Create a new app from the template: scripts/new-app.sh <folder-name> [Display-Name]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[ $# -ge 1 ] || { echo "usage: $0 <folder-name> [Display-Name]"; exit 1; }
DIR="$ROOT/apps/$1"
NAME="${2:-$1}"
[ -e "$DIR" ] && { echo "apps/$1 already exists"; exit 1; }
cp -r "$ROOT/template" "$DIR"
grep -rl "__APP_NAME__" "$DIR" | xargs perl -pi -e "s/__APP_NAME__/$NAME/g"
mkdir -p "$DIR/helpers"
echo "Created apps/$1 (add-in name: $NAME)"
