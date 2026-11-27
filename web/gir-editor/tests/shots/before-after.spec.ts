// SPDX-License-Identifier: GPL-3.0-only
// (rc3) Six before / after pictures for RAPPORT-LAYOUT.md: the same part of the editor in rc2 (served from its tag,
// BEFORE_URL) and now, side by side. npx playwright test -c playwright.shots.config.ts
import { test, type Page } from "@playwright/test";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";

const OUT = new URL("../../../../../livraison/EDITOR/screens/layout/avant-apres/", import.meta.url).pathname;
const BEFORE = process.env.BEFORE_URL || "http://localhost:5177";
const AFTER = process.env.AFTER_URL || "http://localhost:5175";
const g = (page: Page, fn: string, arg?: unknown) => page.evaluate(([fn, arg]) => new Function("g", "arg", fn)((window as any).__gir, arg), [fn, arg] as const);

async function open(page: Page, base: string, w: number, h: number) {
  await page.setViewportSize({ width: w, height: h });
  await page.addInitScript(() => { localStorage.setItem("gir.lang", JSON.stringify("fr")); localStorage.setItem("gir.view", JSON.stringify("sound")); });
  await page.goto(base + "/");
  await page.getByRole("button", { name: /Essayer sans FM-1/ }).click();
  await page.waitForFunction(() => (window as any).__gir?.device.status === "ready", null, { timeout: 60_000 });
  await page.waitForTimeout(1200);
}
interface Shot { name: string; title: string; w: number; h: number; prep: (p: Page) => Promise<void>; sel: string | null }
const SHOTS: Shot[] = [
  { name: "1-mod", title: "Panneau MOD (ANALOG, 1440 × 900)", w: 1440, h: 900, sel: "article:has(> header h3:text-is('MOD'))",
    prep: async (p) => { await g(p, "return g.device.selectTrack(0).then(() => g.device.setEngine(0, 0))"); await p.waitForTimeout(700); } },
  { name: "2-mixer", title: "Mixer (1024 × 768)", w: 1024, h: 768, sel: null, prep: async (p) => { await g(p, "g.ui.view = 'mixer'"); await p.waitForTimeout(700); } },
  { name: "3-mini", title: "Cartes Moteur et Preset, MINI (1440 × 900)", w: 1440, h: 900, sel: ".sound .head",
    prep: async (p) => { await g(p, "return g.device.selectTrack(0).then(() => g.device.setEngine(0, 17))"); await p.waitForTimeout(700); } },
  { name: "4-drumgrid", title: "Grille de batterie (1280 × 800)", w: 1280, h: 800, sel: ".seq .main",
    prep: async (p) => { await g(p, "const k = g.device.tracks.findIndex((t) => g.device.engineName(t.engine) === 'DRUM'); return g.device.selectTrack(k)"); await g(p, "g.ui.view = 'seq'"); await p.waitForTimeout(1200); } },
  { name: "5-1180", title: "Fenêtre de 1180 × 820, vue Son", w: 1180, h: 820, sel: null, prep: async (p) => { await g(p, "g.ui.view = 'sound'"); await p.waitForTimeout(500); } },
  { name: "6-screen", title: "Vue Écran (1440 × 900)", w: 1440, h: 900, sel: null, prep: async (p) => { await g(p, "g.ui.view = 'screen'"); await p.waitForTimeout(1200); } },
];

test("before / after", async ({ browser }) => {
  test.setTimeout(600_000);
  mkdirSync(OUT, { recursive: true });
  for (const s of SHOTS) {
    const files: string[] = [];
    for (const [tag, base] of [["avant", BEFORE], ["apres", AFTER]] as const) {
      const page = await browser.newPage();
      await open(page, base, s.w, s.h);
      await s.prep(page);
      const f = `${OUT}${s.name}-${tag}.png`;
      if (s.sel) await page.locator(s.sel).first().screenshot({ path: f });
      else await page.screenshot({ path: f });
      files.push(f);
      await page.close();
    }
    // side by side, in one picture
    const page = await browser.newPage({ viewport: { width: 1600, height: 800 } });
    const img = (f: string) => `data:image/png;base64,${readFileSync(f).toString("base64")}`;
    await page.setContent(`<body style="margin:0;background:#111;color:#ddd;font:15px -apple-system,sans-serif">
      <div style="padding:14px 18px;font-weight:600">${s.title}</div>
      <div style="display:grid;grid-template-columns:1fr 1fr;gap:16px;padding:0 18px 18px;align-items:start">
        <figure style="margin:0"><figcaption style="margin-bottom:6px;color:#ff8a70">Avant (rc2)</figcaption><img style="width:100%;border:1px solid #333" src="${img(files[0])}"></figure>
        <figure style="margin:0"><figcaption style="margin-bottom:6px;color:#a4e631">Après (rc3)</figcaption><img style="width:100%;border:1px solid #333" src="${img(files[1])}"></figure>
      </div></body>`);
    await page.waitForTimeout(300);
    await page.screenshot({ path: `${OUT}${s.name}.png`, fullPage: true });
    await page.close();
  }
  writeFileSync(`${OUT}README.txt`, "Avant (rc2, tag gir-editor-v1.0-rc2) / après (rc3), côte à côte : 1-mod … 6-screen.png ; les deux moitiés à part : *-avant.png, *-apres.png\n");
});
