#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Gir Editor's tables from the firmware's sources: tools/gen_editor_tables.c -> web/gir-editor/src/generated/tables.json,
# then the help from the manual and the controls reference (tools/gen_editor_help.py -> help.json).
# Run from the repo root after one ./build.sh (build/gen). --check: fail when the committed files are not up to date.
set -e
cd "$(dirname "$0")/.."
[ -f build/gen/felucca_tables.h ] || { echo "tables: run ./build.sh once first (build/gen)"; exit 1; }
CC=${CC:-cc}
OUT=build/host
GEN=web/gir-editor/src/generated
mkdir -p "$OUT" "$GEN"
VERSION=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c)
$CC -w "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src -o "$OUT/gen_editor_tables" tools/gen_editor_tables.c -lm
TMP=$(mktemp -d)
"$OUT/gen_editor_tables" > "$TMP/tables.json"
python3 -c "import json,sys; json.load(open(sys.argv[1]))" "$TMP/tables.json"
python3 tools/gen_editor_help.py "$TMP/tables.json" > "$TMP/help.json"
if [ "$1" = --check ]; then
    st=0
    for f in tables.json help.json; do
        cmp -s "$TMP/$f" "$GEN/$f" || { echo "tables: $GEN/$f is not up to date: run tools/gen_editor_tables.sh"; st=1; }
    done
    rm -rf "$TMP"
    exit $st
fi
mv "$TMP/tables.json" "$TMP/help.json" "$GEN/"
rmdir "$TMP"
echo "tables: $GEN/tables.json ($(wc -c < "$GEN/tables.json" | tr -d ' ') B), help.json ($(wc -c < "$GEN/help.json" | tr -d ' ') B)"
