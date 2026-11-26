# Gir Editor

The editor of **Gir** (the FM-1 techno fork of Skiff 2.7) for the M-VAVE FM-1: every sound, pattern, song, mix,
user sound, sample and project, live both ways, in the browser (Chrome, Edge) or as a Mac / Windows app.

- Web: <https://valentingir21.github.io/skiff/editor/> (demo with a virtual FM-1, no hardware needed)
- Apps: GitHub releases, `gir-editor-v*`
- Guide (French): `livraison/EDITOR/GUIDE-EDITEUR.md` in the delivery folder

## Develop

```
npm install
npm run dev          # http://localhost:5174  (?demo=1 starts the demo, ?view=demo the design system page)
npm test             # unit tests against the virtual FM-1 (Node)
npm run e2e          # end-to-end (Playwright, Chromium)
npm run check        # types
npm run app          # the desktop app (Electron)
npm run dist:mac     # universal .dmg   ·   npm run dist:win   # NSIS installer + portable .exe
```

- `src/lib/legacy/proto.js`: generated from the original editor's protocol section (`web/editor.html`), not edited.
- `src/generated/`: `tools/gen_editor_tables.sh` (from the firmware's sources and the Gir manual).
- `public/emu/felucca_link.wasm`: the virtual FM-1, `emu/build.sh` (Emscripten, after one `./build.sh`).
- `ci/gir-editor-desktop.yml`: the desktop builds' workflow, to move to `.github/workflows/`.

GPL-3.0-only. Built on Skiff (illuminus5) and Felucca (Leo Kuroshita, Hügelton Instruments).
