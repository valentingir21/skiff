# Gir Editor 1.0 — suivi (reprise après coupure)

Lire ce fichier d'abord. Branche `gir-editor` (depuis le tag `gir-v1.0`). Docs : `../livraison/EDITOR/`
(DECISIONS.md = journal des décisions). Règles : pas de flash, pas de changement dans `firmware/`, pas de merge.

## Étape en cours
**rc2 publiée** (27/11/2026) : voir `../livraison/EDITOR/RAPPORT-RC2.md` (checklist matériel de 30 min). La rc1 :
`RAPPORT-NUIT.md`. Reste : la CI (droit `workflow` du jeton, puis `git mv web/gir-editor/ci/gir-editor-desktop.yml
.github/workflows/`), les tests sur la vraie FM-1, la finale `gir-editor-v1.0` après eux (pas sans l'utilisateur).

## rc2 (fait)
- Parité complète (`PARITE.md`) : miroir d'écran CAST, Samples (kits, enregistrement), thème et CUSTOM, APC mini mk2,
  conversion des vieilles sauvegardes. Robustesse : NAMES, autre firmware (Skiff 2.7 en fixture), outils SEQ avec
  HOLD, latence réaliste (`LATENCY=1`), débranchements (reprise), « Préparer une mise à jour ». Aide anglaise.
- Tests : 16 unitaires, 23 e2e (aussi en latence), vues aux 3 tailles (`zz-views`), appli lancée
  (`npx playwright test -c playwright.desktop.config.ts` après electron-builder).

## Fait
- Phase 0 : étude, `FIRMWARE-A-PREVOIR.md` (NAMES coupé à 600 octets : l'éditeur d'origine ne se connecte pas à Gir).
- Phase 1 : éditeur web complet, publié sur `gh-pages` `/skiff/editor/` (925d6fc), `/skiff/try/` intact et lié.
  Tests : 12 unitaires + 12 e2e verts ; tables `--check` ok ; `web/test_web.mjs` inchangé et vert.
- Phase 2 : Electron (`web/gir-editor/electron/`), .dmg universel + .exe NSIS + portable construits sur ce Mac,
  pré-release `gir-editor-v1.0-rc1` (avec SHA256SUMS). Workflow CI prêt dans `web/gir-editor/ci/` (non activé :
  jeton sans droit `workflow`).
- Phase 3 : `ETUDE-PLUGIN.md` (recommandation : appli compacte maintenant, Max for Live ensuite, JUCE plus tard).
- Phase 4 : `GUIDE-EDITEUR.md`, `RAPPORT-NUIT.md`, `LICENCES.md`, `DECISIONS.md`, `screens/`.

## Reste (avec l'utilisateur)
- Checklist matériel du rapport ; activer la CI ; décider release finale et merge.

## Commandes
- Dev : `cd web/gir-editor && npm install && npm run dev` (http://localhost:5174, `?demo=1` démarre la démo, `?view=demo` la galerie)
- Tests : `npm test` (unitaires), `npm run e2e` (Playwright ; `LATENCY=1` latence réaliste, `SHOTS=1` captures), `npm run check` (types)
- Fausse FM-1 : `web/gir-editor/emu/build.sh` (après `./build.sh`, emcc requis)
