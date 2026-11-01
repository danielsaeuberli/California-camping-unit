#!/usr/bin/env bash
# Generates the LVGL fonts in ui/fonts/ from Inter (SIL Open Font License) with lv_font_conv.
# Needs node; the generated .c files are committed, so the build does not need node.
#   INTER_DIR=/path/to/inter tools/gen_fonts.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
INTER_DIR="${INTER_DIR:-/usr/share/fonts/opentype/inter}"
OUT="$ROOT/ui/fonts"
mkdir -p "$OUT"
# Basic Latin, Latin-1 (umlauts, degree, middle dot), en dash, single guillemets, ellipsis, minus.
RANGE="0x20-0x7E,0xA0-0xFF,0x2013,0x2026,0x2039,0x203A,0x2212"
gen() { # name file size (RANGE may be overridden per call)
  npx -y lv_font_conv@1.5.3 --bpp 4 --size "$3" --no-compress --format lvgl \
    --font "$INTER_DIR/$2" -r "$RANGE" --lv-include lvgl.h -o "$OUT/$1.c" --force-fast-kern-format
}
gen font_15_semi Inter-SemiBold.otf 15
gen font_16_semi Inter-SemiBold.otf 16
gen font_18 Inter-Regular.otf 18
gen font_20_semi Inter-SemiBold.otf 20
gen font_22_semi Inter-SemiBold.otf 22
gen font_28_semi Inter-SemiBold.otf 28
gen font_34_semi Inter-SemiBold.otf 34
gen font_46_semi Inter-SemiBold.otf 46
gen font_56_semi Inter-SemiBold.otf 56
RANGE="0x20,0x25,0x2B-0x39,0x43,0xB0,0x2013,0x2212" gen font_84_semi Inter-SemiBold.otf 84  # digits only
RANGE="0x20,0x25,0x30-0x39,0x2013" gen font_44_bold Inter-Bold.otf 44  # battery label: digits only
echo "fonts written to $OUT"
