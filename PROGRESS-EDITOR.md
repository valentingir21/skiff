# Gir Editor 1.0 — suivi (reprise après coupure)

Lire ce fichier d'abord. Branche `gir-editor` (depuis le tag `gir-v1.0`). Docs : `../livraison/EDITOR/`
(DECISIONS.md = journal des décisions). Règles : pas de flash, pas de changement dans `firmware/`, pas de merge.

## Étape en cours
Phase 1 : tests Playwright, revue de design, publication gh-pages.

## Fait
- [x] Branche `gir-editor` créée depuis `gir-v1.0` (ac64866).
- [x] Fausse FM-1 : `web/gir-editor/emu/felucca_link.c` (firmware complet + editor.c en WASM), build
      `web/gir-editor/emu/build.sh` → `web/gir-editor/public/emu/felucca_link.wasm`. Répond à INFO/DUMP/DESC/NAMES,
      PANEL actif (INFO 59 01 = 3).

- [x] Phase 0 : `../livraison/EDITOR/ETUDE-EDITEUR-ORIGINE.md`, `FIRMWARE-A-PREVOIR.md` (NAMES coupé à 600 octets).
- [x] Tables générées : `tools/gen_editor_tables.sh` (C contre le firmware + aide depuis le manuel), test `--check`.
- [x] Modèle + protocole + transports (Web MIDI, émulateur), 12 tests unitaires contre la fausse FM-1 (`npm test`).
- [x] Interface : toutes les vues (Son, Séquenceur, Song, Mixer, Bibliothèque, Projets, Samples, FM6, Écran, Réglages),
      palette ⌘K, aide ?, undo/redo, i18n en/fr, galerie `?view=demo`.

## Reste
- Tests Playwright (e2e) + captures `../livraison/EDITOR/screens/` + 2 passes de revue de design.
- Build + publication `gh-pages` sous `/skiff/editor/` (worktree `../work/ghp`), vérifier `/skiff/try/`.
- Phase 2 Electron (+ workflow GitHub Actions, pré-release `gir-editor-v1.0-rc1`).
- Phase 3 étude plugin ; Phase 4 docs (GUIDE, RAPPORT-NUIT, LICENCES).
Voir le plan des phases dans `../livraison/EDITOR/DECISIONS.md` (section « Plan »).

## Commandes
- Dev : `cd web/gir-editor && npm install && npm run dev` (http://localhost:5174, `?demo=1` démarre la démo, `?view=demo` la galerie)
- Tests : `npm test` (unitaires), `npm run e2e` (Playwright), `npm run check` (types)
- Fausse FM-1 : `web/gir-editor/emu/build.sh` (après `./build.sh`, emcc requis)
