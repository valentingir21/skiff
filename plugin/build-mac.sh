#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Gir Editor plugin for macOS: AU + VST3 (+ CLAP), universal (arm64 + x86_64), macOS 12+, signed ad hoc.
#   plugin/build-mac.sh            build (plugin/build/), then install into ~/Library/Audio/Plug-Ins/
#   NO_INSTALL=1 plugin/build-mac.sh   build only (the CI)
#   NO_WEB=1 …                     keep the last web build (web/gir-editor/dist)
# Needs: cmake, ninja (brew install cmake ninja), the Command Line Tools, node (the editor's web build).
set -e
cd "$(dirname "$0")"
PLUGIN=$(pwd)
./fetch-deps.sh
VERSION_LABEL=$(node -p 'require("../web/gir-editor/package.json").version.replace(/-rc\d+$/, "")')-rc1
if [ "${NO_WEB:-0}" != 1 ]; then
  (cd ../web/gir-editor && npm run build >/dev/null)
fi
mkdir -p build
rm -f build/web.zip
(cd ../web/gir-editor/dist && zip -qr -X "$PLUGIN/build/web.zip" .)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DGIR_VERSION_LABEL="$VERSION_LABEL" \
  -DGIR_WEB_ZIP="$PLUGIN/build/web.zip" -DGIR_CLAP="${GIR_CLAP:-OFF}" >/dev/null
cmake --build build --target GirEditorPlugin_AU GirEditorPlugin_VST3 ${GIR_CLAP_TARGET:-} girhost
ART=build/GirEditorPlugin_artefacts/Release
for b in "$ART/AU/Gir Editor.component" "$ART/VST3/Gir Editor.vst3" "$ART/CLAP/Gir Editor.clap"; do
  [ -d "$b" ] || continue
  codesign --force --deep -s - "$b"
  echo "built: $b ($(lipo -archs "$b"/Contents/MacOS/* | tr '\n' ' '))"
done
if [ "${NO_INSTALL:-0}" != 1 ]; then
  C=~/Library/Audio/Plug-Ins/Components V=~/Library/Audio/Plug-Ins/VST3 L=~/Library/Audio/Plug-Ins/CLAP
  mkdir -p "$C" "$V" "$L"
  rm -rf "$C/Gir Editor.component" "$V/Gir Editor.vst3" "$L/Gir Editor.clap"
  ditto "$ART/AU/Gir Editor.component" "$C/Gir Editor.component"
  ditto "$ART/VST3/Gir Editor.vst3" "$V/Gir Editor.vst3"
  [ -d "$ART/CLAP/Gir Editor.clap" ] && ditto "$ART/CLAP/Gir Editor.clap" "$L/Gir Editor.clap"
  # the AU cache: macOS sees the new component without a log-out
  killall -9 AudioComponentRegistrar 2>/dev/null || true
  echo "installed: $C, $V, $L"
fi
