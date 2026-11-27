// SPDX-License-Identifier: GPL-3.0-only
// (rc3) The layout lint over the whole matrix: 7 window sizes and the compact window, x 3 browser zooms (90, 100,
// 125 %: the same CSS window as a zoomed one, at its pixel density), x English and French, x every view (Sequencer in
// piano roll and in grid, Samples with a kit, FM6 with an FM6 track and without one), the dialogs (⌘K palette,
// shortcuts, a confirmation, the engine picker), and the Sound view on each of the 15 engines with a preset loaded, its
// whole column. Every case: the rules of layout-lint.ts, and a capture in livraison/EDITOR/screens/layout/<label>/ (the
// label: LAYOUT_LABEL, "apres" by default). The defects go to <label>/parts/*.json (npm run layout:report sums them).
// Run: npx playwright test -c playwright.layout.config.ts (4 at a time). LAYOUT_SHOTS=0: no captures.
import { test, expect, type Page } from "@playwright/test";
import { mkdirSync, writeFileSync } from "node:fs";
import { lint, type Defect } from "./layout-lint";

const LABEL = process.env.LAYOUT_LABEL || "apres";
const DIR = new URL(`../../../../../livraison/EDITOR/screens/layout/${LABEL}/`, import.meta.url).pathname;
const SHOTS = process.env.LAYOUT_SHOTS !== "0";
const KNOBS = [48, 34];
const SIZES: [number, number][] = [[1024, 768], [1180, 820], [1280, 800], [1440, 900], [1470, 956], [1512, 982], [1920, 1080]];
const ZOOMS = [0.9, 1, 1.25];
const LANGS = ["en", "fr"] as const;
const ONLY = process.env.LAYOUT_ONLY ? new RegExp(process.env.LAYOUT_ONLY) : null;

const g = (page: Page, fn: string, arg?: unknown) => page.evaluate(([fn, arg]) => new Function("g", "arg", fn)((window as any).__gir, arg), [fn, arg] as const);

/** a mono 16-bit WAV: tone bursts at the given seconds */
function wav(hits: number[], len = 1.2, sr = 22050, f = 220): Buffer {
  const n = Math.round(len * sr), b = Buffer.alloc(44 + n * 2);
  b.write("RIFF", 0); b.writeUInt32LE(36 + n * 2, 4); b.write("WAVEfmt ", 8); b.writeUInt32LE(16, 16); b.writeUInt16LE(1, 20); b.writeUInt16LE(1, 22);
  b.writeUInt32LE(sr, 24); b.writeUInt32LE(sr * 2, 28); b.writeUInt16LE(2, 32); b.writeUInt16LE(16, 34); b.write("data", 36); b.writeUInt32LE(n * 2, 40);
  for (let i = 0; i < n; i++) {
    let v = 0;
    for (const h of hits) { const t = i / sr - h; if (t >= 0 && t < 0.12) v += Math.sin(2 * Math.PI * f * t) * Math.exp(-t * 30); }
    b.writeInt16LE(Math.max(-32767, Math.min(32767, Math.round(v * 24000))), 44 + i * 2);
  }
  return b;
}

async function settle(page: Page, ms = 350) {
  await page.waitForTimeout(ms);
  await page.evaluate(() => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))));
}
async function setView(page: Page, v: string) {
  await g(page, "g.ui.view = arg; g.ui.palette = false; g.ui.help = false;", v);
  await settle(page, v === "screen" || v === "song" ? 900 : 400);
}
async function capture(page: Page, name: string) {
  if (!SHOTS) return;
  // the whole column, not only what the window shows: the view grows to its content for the capture
  await page.evaluate(() => document.documentElement.classList.add("lint-shot"));
  await page.screenshot({ path: `${DIR}${name}.jpg`, fullPage: true, type: "jpeg", quality: 55, scale: "css" });
  await page.evaluate(() => document.documentElement.classList.remove("lint-shot"));
}

for (const z of ZOOMS)
  for (const [w0, h0] of [...SIZES, [560, 460] as [number, number]])
    for (const lang of LANGS) {
      const compact = w0 === 560;
      const vw = Math.round(w0 / z), vh = Math.round(h0 / z);
      const id = `${compact ? "compact" : `${w0}x${h0}`}-z${Math.round(z * 100)}-${lang}`;
      test.describe(id, () => {
        test.use({ viewport: { width: vw, height: vh }, deviceScaleFactor: z });
        test(`layout lint ${id}`, async ({ page }) => {
          test.skip(!!ONLY && !ONLY.test(id), "LAYOUT_ONLY");
          test.setTimeout(600_000);
          mkdirSync(DIR + "parts", { recursive: true });
          await page.addInitScript((l) => { localStorage.setItem("gir.lang", JSON.stringify(l)); localStorage.setItem("gir.view", JSON.stringify("sound")); }, lang);
          if (compact) await page.goto("/?compact=1&demo=1");
          else {
            await page.goto("/");
            await page.getByRole("button", { name: /Try without an FM-1|Essayer sans FM-1/ }).click({ timeout: 30_000 });
          }
          await page.waitForFunction(() => (window as any).__gir?.device.status === "ready", null, { timeout: 60_000 });
          await page.addStyleTag({ content: "html.lint-shot, html.lint-shot body, html.lint-shot #app, html.lint-shot .app { height: auto !important; overflow: visible !important } html.lint-shot .view { overflow: visible !important; flex: none !important } html.lint-shot .body { min-height: auto !important }" });
          await settle(page, 1200);
          const found: (Defect & { case: string })[] = [];
          const check = async (name: string, root?: string, view = "") => {
            if (process.env.LAYOUT_DEBUG) console.log(id, name, new Date().toISOString().slice(11, 19));
            const d = await page.evaluate(lint, { root, knobSizes: KNOBS, view });
            found.push(...d.map((x) => ({ ...x, case: name })));
            await capture(page, `${id}-${name}`);
          };

          if (compact) {
            await check("compact");
          } else {
            const engineOf = (k: number) => g(page, "return g.device.engineName(g.device.tracks[arg].engine)", k) as Promise<string>;
            // the views as the demo has them
            for (const v of ["sound", "song", "mixer", "library", "projects", "screen", "settings"]) { await setView(page, v); await check(v, undefined, v); }
            // the sequencer: a melodic track in piano roll, a drum track in its grid
            const tracks = await g(page, "return g.device.tracks.map((t) => g.device.engineName(t.engine))") as string[];
            const mel = Math.max(0, tracks.findIndex((e) => e !== "DRUM" && e !== "DRUM-X"));
            const drm = tracks.findIndex((e) => e === "DRUM" || e === "DRUM-X");
            await g(page, "return g.device.selectTrack(arg)", mel); await setView(page, "seq"); await settle(page, 900); await check("seq-roll", undefined, "seq");
            if (drm >= 0) { await g(page, "return g.device.selectTrack(arg)", drm); await setView(page, "seq"); await settle(page, 900); await check("seq-grid", undefined, "seq"); }
            // FM6 without an FM6 track, then with one
            await g(page, "return g.device.selectTrack(arg)", mel);
            if ((await engineOf(mel)) !== "FM6") { await setView(page, "fm6"); await check("fm6-empty", undefined, "fm6"); }
            // samples with a kit loaded in USR1
            await setView(page, "samples");
            const slot = page.getByTestId("slot-0");
            await slot.getByRole("radio", { name: /Drum kit|Kit de batterie/ }).click();
            const fc = page.waitForEvent("filechooser");
            await slot.getByRole("button", { name: /Cut one WAV|Découper un WAV/ }).click();
            await (await fc).setFiles([{ name: "loop.wav", mimeType: "audio/wav", buffer: wav([0.05, 0.35, 0.65, 0.95]) }]);
            await expect(slot.locator(".lane canvas")).toHaveCount(4);
            await settle(page);
            await check("samples-kit", undefined, "samples");
            // the dialogs
            await setView(page, "sound");
            await g(page, "g.ui.palette = true"); await settle(page); await check("dlg-palette", ".palette, [role=dialog]");
            await g(page, "g.ui.palette = false; g.ui.help = true"); await settle(page); await check("dlg-shortcuts", "[role=dialog]");
            await g(page, "g.ui.help = false");
            await g(page, `g.ui.confirm = { title: arg === "fr" ? "Restaurer la sauvegarde ?" : "Restore the backup?",
              body: arg === "fr" ? "Tous les projets, sons utilisateur, patterns et samples de la FM-1 seront remplacés par ceux du fichier. Une sauvegarde de l'état actuel est enregistrée d'abord." : "Every project, user sound, pattern and sample on the FM-1 is replaced by the file's. A backup of how it is now is saved first.",
              ok: arg === "fr" ? "Restaurer" : "Restore", cancel: arg === "fr" ? "Annuler" : "Cancel", danger: true, resolve: () => { g.ui.confirm = null; } }`, lang);
            await settle(page); await check("dlg-confirm", "[role=alertdialog]");
            await g(page, "g.ui.confirm && g.ui.confirm.resolve(null)");
            await page.locator(".head .engine, [data-engine-button]").first().click();
            await settle(page); await check("dlg-engines", "[role=dialog]");
            await page.locator(".picker").getByRole("button", { name: /^(Close|Fermer)$/ }).click();
            await settle(page);
            // the presets' drawer (under 1440 px)
            const tog = page.locator("[data-presets-toggle]");
            if (await tog.count()) {
              await tog.click(); await settle(page); await check("dlg-presets", ".drawer");
              await page.locator(".drawer").getByRole("button", { name: /^(Close|Fermer)$/ }).click(); await settle(page);
            }
            // the Sound view on each engine of track 1, a preset loaded (one past the first: a bank one, mostly)
            await g(page, "return g.device.selectTrack(0)");
            const engines = await g(page, "return g.device.info ? [0,12,2,15,4,16,6,17,8,9,11,18,10,14,19] : []") as number[];
            for (const e of engines) {
              await g(page, "return g.device.setEngine(0, arg)", e);
              await g(page, "const n = (g.device.names[arg] || []).length; return n > 1 ? g.device.loadPreset(0, arg, Math.min(n - 1, 5)) : null", e);
              await setView(page, "sound"); await settle(page, 500);
              const name = await engineOf(0);
              await check(`sound-${name}`, undefined, "sound");
              if (name === "FM6") { await setView(page, "fm6"); await check("fm6", undefined, "fm6"); }
            }
          }
          writeFileSync(`${DIR}parts/${id}.json`, JSON.stringify(found));
          const sum: Record<string, number> = {};
          for (const d of found) sum[d.rule] = (sum[d.rule] ?? 0) + 1;
          console.log(id, found.length, JSON.stringify(sum));
          expect(found.map((d) => `${d.case} ${d.rule} ${d.el} ${d.more ?? ""} @${d.x},${d.y}`).slice(0, 40)).toEqual([]);
        });
      });
    }
