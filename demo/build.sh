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

# Version stamp on every asset: GitHub Pages lets browsers cache files for ~10 min, and a
# mix of old and new files (e.g. new index.html, old app.js) breaks the page.
VER="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || date +%s)"

# Load the mock before app.js so it can replace window.WebSocket.
sed -e "s#<script src=\"app.js\"></script>#<script src=\"demo/mock-backend.js?v=$VER\"></script>\n<script src=\"app.js?v=$VER\"></script>#" \
    -e "s#href=\"style.css\"#href=\"style.css?v=$VER\"#" \
  "$ROOT/ui/index.html" > "$OUT/index.html"
grep -q "demo/mock-backend.js?v=$VER" "$OUT/index.html" || { echo "script injection failed" >&2; exit 1; }
grep -q "style.css?v=$VER" "$OUT/index.html" || { echo "stylesheet stamp failed" >&2; exit 1; }
touch "$OUT/.nojekyll"
echo "demo built in $OUT"
