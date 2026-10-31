#!/usr/bin/env bash
# Builds the static GitHub Pages demo into _site/: the unchanged ui/ files plus
# demo/mock-backend.js, which stands in for camperd inside the browser.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/_site}"

rm -rf "$OUT"
mkdir -p "$OUT/i18n" "$OUT/demo"
cp "$ROOT/ui/app.js" "$ROOT/ui/style.css" "$OUT/"
cp "$ROOT"/ui/i18n/*.json "$OUT/i18n/"
cp "$ROOT/demo/mock-backend.js" "$OUT/demo/"
cp "$ROOT/ui/reference/Camping_Unit_Oberflaeche_v2.html" "$OUT/prototype.html"

# Load the mock before app.js so it can replace window.WebSocket.
sed 's#<script src="app.js"></script>#<script src="demo/mock-backend.js"></script>\n<script src="app.js"></script>#' \
  "$ROOT/ui/index.html" > "$OUT/index.html"
grep -q 'demo/mock-backend.js' "$OUT/index.html" || { echo "script injection failed" >&2; exit 1; }
touch "$OUT/.nojekyll"
echo "demo built in $OUT"
