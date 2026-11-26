# Gir Editor 1.0 — suivi (reprise après coupure)

Lire ce fichier d'abord. Branche `gir-editor` (depuis le tag `gir-v1.0`). Docs : `../livraison/EDITOR/`
(DECISIONS.md = journal des décisions). Règles : pas de flash, pas de changement dans `firmware/`, pas de merge.

## Étape en cours
Phase 0 → Phase 1 (squelette de l'éditeur).

## Fait
- [x] Branche `gir-editor` créée depuis `gir-v1.0` (ac64866).
- [x] Fausse FM-1 : `web/gir-editor/emu/felucca_link.c` (firmware complet + editor.c en WASM), build
      `web/gir-editor/emu/build.sh` → `web/gir-editor/public/emu/felucca_link.wasm`. Répond à INFO/DUMP/DESC/NAMES,
      PANEL actif (INFO 59 01 = 3).

## Reste
Voir le plan des phases dans `../livraison/EDITOR/DECISIONS.md` (section « Plan »).

## Commandes
- Fausse FM-1 : `web/gir-editor/emu/build.sh` (après `./build.sh`, emcc requis)
