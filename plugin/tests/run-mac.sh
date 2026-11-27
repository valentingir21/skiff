#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The plugin's tests, without a DAW or an FM-1 (after build-mac.sh, which installs the AU):
#   auval (arm64 and x86_64), Steinberg's VST3 validator, the test host (tests/host: VST3 and AU; the demo with the
#   layout lint at 900x600, 1200x800, 1440x900; the SysEx bridge on the host's virtual FM-1 "Gir Test FM-1"; a backup;
#   two instances and the lock), the desktop app and the plugin sharing the lock. Captures into
#   livraison/EDITOR/screens/plugin/. A real FM-1 plugged into the Mac is never opened (GIR_PLUGIN_TEST_PORT).
set -e
cd "$(dirname "$0")/.."
ROOT=$(pwd)
ART=build/GirEditorPlugin_artefacts/Release
VST3="$ART/VST3/Gir Editor.vst3"
HOST=build/tests/host/girhost_artefacts/Release/girhost.app/Contents/MacOS/girhost
SHOTS=${GIR_SHOTS:-$ROOT/../../livraison/EDITOR/screens/plugin}   # (livraison/: beside the repository)
OUT=${TMPDIR:-/tmp}/gir-plugin-tests
rm -rf "$OUT"; mkdir -p "$OUT" "$SHOTS"
fail=0
ok() { echo "PASS $1"; }
ko() { echo "FAIL $1"; fail=1; }

auval -v aufx GirE Girr > "$OUT/auval-arm64.log" 2>&1 && grep -q "AU VALIDATION SUCCEEDED" "$OUT/auval-arm64.log" && ok "auval arm64" || ko "auval arm64"
arch -x86_64 auval -v aufx GirE Girr > "$OUT/auval-x86_64.log" 2>&1 && grep -q "AU VALIDATION SUCCEEDED" "$OUT/auval-x86_64.log" && ok "auval x86_64" || ko "auval x86_64"
if [ ! -x ext/vst3sdk/build/bin/Release/validator ]; then
  (cd ext/vst3sdk && XCODE_VERSION=16 cmake -S . -B build -G Ninja -DXCODE_VERSION=16 -DCMAKE_BUILD_TYPE=Release \
    -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF -DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=ON >/dev/null && cmake --build build --target validator >/dev/null)
fi
ext/vst3sdk/build/bin/Release/validator "$VST3" > "$OUT/vst3-validator.log" 2>&1 || true
grep -q "Result: .* 0 tests failed" "$OUT/vst3-validator.log" && ok "VST3 validator ($(grep 'Result:' "$OUT/vst3-validator.log" | sed 's/^Result: //'))" || ko "VST3 validator"

run() {  # format scenario instances
  d="$OUT/$1-$2"
  if [ "$1" = au ]; then src="--au"; else src="--vst3"; fi
  if [ "$1" = au ]; then "$HOST" --au --scenario "$2" --instances "$3" --emu --out "$d" --timeout 200 > "$d.log" 2>&1 || true
  else "$HOST" --vst3 "$VST3" --scenario "$2" --instances "$3" --emu --out "$d" --timeout 200 > "$d.log" 2>&1 || true; fi
  if grep -q "girhost: all passed" "$d.log"; then ok "host $1 $2 ($(grep -c '"check"' "$d.log") checks)"; else ko "host $1 $2"; grep '"ok": false' "$d.log" | cut -c1-300; fi
}
for f in vst3 au; do run $f demo 1; run $f bridge 1; run $f lock 2; done
# the backup the plugin saved: a whole backup file
b=$(ls "$OUT"/vst3-bridge/saved/Skiff-backup-*.json 2>/dev/null | head -1)
[ -n "$b" ] && node -e 'const f=JSON.parse(require("fs").readFileSync(process.argv[1],"utf8")); if(!f||typeof f!=="object") process.exit(1)' "$b" && ok "backup file ($(wc -c < "$b" | tr -d ' ') B)" || ko "backup file"
(cd ../web/gir-editor && npx playwright test -c playwright.desktop.config.ts tests/desktop/lock.spec.ts > "$OUT/app-lock.log" 2>&1) && ok "desktop app + plugin lock" || ko "desktop app + plugin lock"
cp "$OUT"/vst3-demo/*.png "$OUT"/vst3-bridge/*.png "$OUT"/vst3-lock/*.png "$SHOTS"/ 2>/dev/null || true
echo "logs: $OUT"
exit $fail
