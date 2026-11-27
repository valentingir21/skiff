// SPDX-License-Identifier: GPL-3.0-only
// (rc3) The presets' descriptions: the "Usage" column of Gir 1.0's bank (livraison/FINAL/PRESETS.md, the tables by
// category), as src/generated/presets.fr.json { "ENGINE/NAME": usage }. Their English is src/generated/presets.en.json
// (hand-made; tests/unit/describe.test.ts fails when one is missing). node scripts/gen-preset-desc.mjs [PRESETS.md]
import { readFileSync, writeFileSync } from "node:fs";
const src = process.argv[2] || new URL("../../../../livraison/FINAL/PRESETS.md", import.meta.url).pathname;
const out = {};
let on = false;
for (const line of readFileSync(src, "utf8").split("\n")) {
  if (line.startsWith("## ")) on = /^## (PLUCKS|STABS|BASSES|LEADS|PADS|DRONES|EFFETS|PERCUSSIONS|KITS)/.test(line);
  if (!on || !line.startsWith("| **")) continue;
  const c = line.split("|").map((x) => x.trim());
  const name = c[1].replace(/\*\*/g, "").replace(/\s*\*\(.*?\)\*\s*/g, "").trim();
  const eng = c[2], usage = c[5].replace(/\*\*/g, "").trim();
  out[`${eng}/${name}`] = usage;
}
writeFileSync(new URL("../src/generated/presets.fr.json", import.meta.url), JSON.stringify(out, null, 0).replace(/","/g, '",\n"') + "\n");
console.log(Object.keys(out).length, "descriptions");
