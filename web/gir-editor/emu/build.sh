#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Gir Editor's virtual FM-1: the firmware as Try Gir runs it (web/emu/felucca_web.c) plus its SysEx editor handler
# (firmware/src/editor.c), compiled to WebAssembly. Run after one ./build.sh (build/gen) from the repo root:
#   web/gir-editor/emu/build.sh   ->  web/gir-editor/public/emu/felucca_link.wasm
set -e
cd "$(dirname "$0")/../../.."
[ -f build/gen/felucca_tables.h ] || { echo "link: run ./build.sh once first (build/gen)"; exit 1; }
command -v emcc >/dev/null 2>&1 || { echo "link: no emcc (Emscripten)"; exit 1; }
VERSION=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c)
OUT=web/gir-editor/public/emu
mkdir -p "$OUT"
# shellcheck disable=SC2086
emcc ${EMU_OPT:--O2} -ffp-contract=off -std=gnu99 -w ${EMU_CFLAGS:-} \
    "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o "$OUT/felucca_link.wasm" web/gir-editor/emu/felucca_link.c
echo "link: $OUT/felucca_link.wasm ($(wc -c < "$OUT/felucca_link.wasm" | tr -d ' ') B, $VERSION)"
# (rc2) the Screen view's copy of a real unit's screen: web/emu/cast_follow.c, as the original editor's Screen tab
# (web/emu/build.sh builds the same file for it; here beside the virtual FM-1, for the editor's own build)
# shellcheck disable=SC2086
emcc ${EMU_OPT:--O2} -ffp-contract=off -std=gnu99 -w ${EMU_CFLAGS:-} \
    "-DFELUCCA_VERSION=\"$VERSION\"" -DFELUCCA_CAST=1 -Ibuild/gen -Ifirmware/src \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o "$OUT/felucca_cast.wasm" web/emu/cast_follow.c
echo "link: $OUT/felucca_cast.wasm ($(wc -c < "$OUT/felucca_cast.wasm" | tr -d ' ') B)"
# (rc2) the converter of backups from before Skiff 2.0 (web/conv: the firmware's own code), as the original's Restore
sh web/conv/build.sh
mkdir -p web/gir-editor/public/conv
cp build/conv/conv.wasm web/gir-editor/public/conv/conv.wasm
