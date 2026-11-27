// SPDX-License-Identifier: GPL-3.0-only
// (rc3) Sums the layout lint's defects (livraison/EDITOR/screens/layout/<label>/parts/*.json): by rule, by view, by
// window, and the most frequent ones. node scripts/layout-report.mjs <label> [--md] [--top N]
import { readdirSync, readFileSync } from "node:fs";
const label = process.argv[2] || "apres";
const md = process.argv.includes("--md");
const top = Number(process.argv[process.argv.indexOf("--top") + 1]) || 25;
const dir = new URL(`../../../../livraison/EDITOR/screens/layout/${label}/parts/`, import.meta.url).pathname;
const rows = [];
for (const f of readdirSync(dir).filter((f) => f.endsWith(".json"))) for (const d of JSON.parse(readFileSync(dir + f, "utf8"))) rows.push({ ...d, ctx: f.replace(".json", "") });
const count = (key) => { const m = {}; for (const r of rows) m[key(r)] = (m[key(r)] ?? 0) + 1; return Object.entries(m).sort((a, b) => b[1] - a[1]); };
const view = (r) => r.case.replace(/^sound-.*/, "sound (15 moteurs)").replace(/^dlg-.*/, "boîtes de dialogue");
const size = (r) => r.ctx.replace(/-z\d+-(en|fr)$/, "");
const out = (title, list) => { console.log(md ? `\n| ${title} | défauts |\n|---|---|` : `\n== ${title}`); for (const [k, n] of list) console.log(md ? `| ${k} | ${n} |` : `${String(n).padStart(7)}  ${k}`); };
console.log(`${label}: ${rows.length} défauts, ${readdirSync(dir).length} combinaisons`);
out("règle", count((r) => r.rule));
out("vue", count(view));
out("taille", count(size));
out("zoom", count((r) => r.ctx.match(/z\d+/)[0]));
out("langue", count((r) => r.ctx.slice(-2)));
out("vue × règle", count((r) => `${view(r)} · ${r.rule}`).slice(0, top));
out("les plus fréquents", count((r) => `${r.case.replace(/^sound-.*/, "sound")} ${r.rule} ${r.el.replace(/\[.*?\]/g, "[…]")} ${String(r.more ?? "").replace(/\d+/g, "#")}`).slice(0, top));
