// (techno-engines) the new engines in the browser build: EDIT + white key, a held note, its peak; a screenshot each
// node tools/emu_engines.mjs build/emu/felucca.wasm OUTDIR '["ANALOG", ...]' (the display order: descdump ORDER)
import fs from "fs";
const [wasmPath, outDir] = process.argv.slice(2);
const bytes = fs.readFileSync(wasmPath);
const W = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];   // the white keys F3 .. G5
const EDIT = 4;
const { instance } = await WebAssembly.instantiate(bytes, {});
const ex = instance.exports, mem = ex.memory;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
let peak = 0;
const render = (ms) => { const n = Math.round(ms * 44.1); for (let k = 0; k < n; k += 128) { const m = Math.min(128, n - k); ex.web_render(m);
  const l = new Float32Array(mem.buffer, ex.web_out_l(), m); for (let i = 0; i < m; i++) peak = Math.max(peak, Math.abs(l[i])); } };
const shot = (name) => fs.writeFileSync(`${outDir}/${name}.raw`, Buffer.from(new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240).slice().buffer));
render(1500);
const ORDER = JSON.parse(process.argv[4]);
let fails = 0;
for (const name of ["SUPERSAW", "ACID", "MINI", "HOOVER", "INDUS"]) {
  const rank = ORDER.indexOf(name);
  ex.web_buttons(1 << EDIT); render(80); ex.web_keys(1 << W[rank]); render(80); ex.web_keys(0); render(80); ex.web_buttons(0); render(400);
  shot(`${name}_edit`);
  peak = 0; render(300);
  const quiet = peak;
  peak = 0;
  const chord = name === "SUPERSAW" ? (1 << 7) | (1 << 11) | (1 << 14) : (1 << 7);   // C4 (E4 G4: a chord)
  ex.web_keys(chord); render(700); shot(`${name}_play`); ex.web_keys(0); render(600);
  const ok = peak > 0.02 && quiet < 0.001;
  if (!ok) fails++;
  console.log(`${name.padEnd(9)} EDIT + white key ${rank + 1}: silent before (${quiet.toFixed(4)}), a ${name === "SUPERSAW" ? "chord" : "note"} sounds, peak ${peak.toFixed(3)}  ${ok ? "ok" : "FAIL"}`);
  render(2500);
}
process.exit(fails ? 1 : 0);
