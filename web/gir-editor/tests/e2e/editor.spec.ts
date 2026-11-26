// SPDX-License-Identifier: GPL-3.0-only
// Gir Editor end to end, on the virtual FM-1 (the Gir 1.0 firmware in WebAssembly, answering the very SysEx a unit
// does): every check reads the device back through the protocol, so what the editor shows and what the "FM-1" holds
// are compared, both ways. Screenshots of each view go to ../../../livraison/EDITOR/screens/ (SHOTS=1).
import { test, expect, type Page } from "@playwright/test";
import { mkdirSync, readFileSync } from "node:fs";
import tables from "../../src/generated/tables.json" with { type: "json" };

const T = tables as any;
const ID = T.ID;
const SHOTS = process.env.SHOTS === "1";
const SHOT_DIR = new URL("../../../../../livraison/EDITOR/screens/", import.meta.url).pathname;
if (SHOTS) mkdirSync(SHOT_DIR, { recursive: true });
const shot = async (page: Page, name: string) => { if (SHOTS) await page.screenshot({ path: SHOT_DIR + name + ".png" }); };

async function start(page: Page, lang = "en", seed = false) {
  await page.addInitScript(([l, seed]) => { try { localStorage.setItem("gir.lang", JSON.stringify(l)); localStorage.setItem("gir.view", JSON.stringify("sound")); localStorage.setItem("gir.noseed", JSON.stringify(seed === false)); } catch {} }, [lang, seed] as const);
  await page.goto("/");
  await page.getByRole("button", { name: /Try without an FM-1|Essayer sans FM-1/ }).click();
  await page.waitForSelector(".top", { timeout: 30_000 });
  await page.waitForFunction(() => (window as any).__gir.device.status === "ready");
}
const tp = (page: Page, k: number, id: number) =>
  page.evaluate(([k, id]) => { const g = (window as any).__gir; return g.device.ask(g.req.trackParam(k, id)).then((r: any) => r.value); }, [k, id]);
const sel = (page: Page) => page.evaluate(() => (window as any).__gir.device.sel);
const engOf = (page: Page, k: number) => page.evaluate((k) => { const g = (window as any).__gir; return g.device.ask(g.req.trackDump(k)).then((r: any) => g.device.engineName(r.engine)); }, k);
const view = (page: Page, name: string) => page.getByRole("tab", { name }).click();
async function track(page: Page, k: number) { await page.getByRole("button", { name: `Select track ${k + 1}` }).click(); await expect.poll(() => sel(page)).toBe(k); }
async function engine(page: Page, key: string) {
  await page.locator("button.engine").click();
  await page.locator(`.picker .eng:has(.key:text-is("${key}"))`).click();
}
async function knobUp(page: Page, name: string, n: number) {
  const k = page.getByRole("slider", { name, exact: true }).first();
  await k.focus();
  for (let i = 0; i < n; i++) await k.press("ArrowUp");
}

test("connects to the virtual FM-1, Gir 1.0 tables, six tracks, live", async ({ page }) => {
  await page.goto("/");
  await shot(page, "01-accueil");
  await start(page);
  await expect(page.locator(".status")).toContainText("Gir 1.0");
  await expect(page.locator(".col .trk")).toHaveCount(6);
  expect(await page.evaluate(() => (window as any).__gir.device.tablesMatch)).toBe(true);
  expect(await page.evaluate(() => (window as any).__gir.device.live)).toBe(true);
  await shot(page, "02-son");
});

test("the demo's first time writes a techno loop and saves it as project A", async ({ page }) => {
  await start(page, "en", true);
  const st = await page.evaluate(async () => {
    const g = (window as any).__gir, d = g.device;
    const p = await d.ask(g.req.projName());
    let acid = 0; for (let i = 0; i < 16; i++) { const s = await d.ask(g.req.trackStep(1, i)); if (s.time === 0 && s.n) acid++; }
    return { name: p.slots[0].name, e: [0, 1, 2, 3].map((k) => d.engineName(d.tracks[k].engine)), acid, bpm: d.g[0] };
  });
  expect(st.name).toBe("GIR DEMO");
  expect(st.e).toEqual(["SUPERSAW", "ACID", "MINI", "DRUM"]);
  expect(st.acid).toBeGreaterThan(3);
  expect(st.bpm).toBe(132);
  await view(page, "Song");
  await page.waitForTimeout(700);
  await shot(page, "10b-song-demo");
  await view(page, "Sequencer");
  await page.waitForTimeout(1200);
  await shot(page, "08b-sequenceur-demo");
});

test("every Gir engine, from the engine picker, reaches the device", async ({ page }) => {
  await start(page);
  await track(page, 1);
  for (const [key, name] of [["B3", "SUPERSAW"], ["D4", "ACID"], ["F4", "MINI"], ["C5", "HOOVER"], ["F5", "INDUS"]]) {
    await engine(page, key);
    await expect(page.locator(".en")).toContainText(name);
    await expect.poll(() => engOf(page, 1)).toBe(name);
  }
  await engine(page, "F4");
  await expect(page.locator(".card h3", { hasText: "OSC 1" })).toBeVisible();
  await shot(page, "03-son-mini");
  await page.locator("button.engine").click();
  await page.waitForTimeout(300);
  await shot(page, "04-choix-moteur");
});

test("the Gir pages: RUMBLE, RUMBLE 2, KICK knobs change the device; EQ and DEPTH in the mixer", async ({ page }) => {
  await start(page);
  await track(page, 3);
  await engine(page, "D5");                                        // DRUM
  await expect(page.locator(".card h3", { hasText: /^RUMBLE$/ })).toBeVisible();
  const before = await tp(page, 3, ID.P_RB_MIX);
  await knobUp(page, "RUMBLE", 12);
  await expect.poll(() => tp(page, 3, ID.P_RB_MIX)).toBe(before + 12);
  const tune = await tp(page, 3, ID.P_RB_TUNE);
  await knobUp(page, "R.TUNE", 2);
  await expect.poll(() => tp(page, 3, ID.P_RB_TUNE)).toBe(tune + 2);
  const click = await tp(page, 3, ID.P_KCLICK);
  await knobUp(page, "K.CLICK", 5);
  await expect.poll(() => tp(page, 3, ID.P_KCLICK)).toBe(click + 5);
  await page.locator(".card h3", { hasText: /^RUMBLE$/ }).scrollIntoViewIfNeeded();
  await shot(page, "05-son-drum-rumble");
  await view(page, "Mixer");
  const strip = page.locator(".strip").nth(2);
  const low = await tp(page, 2, ID.P_EQ_LOW);
  await strip.getByRole("slider", { name: "LOW", exact: true }).focus();
  await page.keyboard.press("ArrowDown");
  await expect.poll(() => tp(page, 2, ID.P_EQ_LOW)).toBe(low - 1);
  const depth = await tp(page, 2, ID.P_DEPTH);
  await strip.getByRole("slider", { name: "DEPTH", exact: true }).focus();
  await page.keyboard.press("ArrowUp");
  await expect.poll(() => tp(page, 2, ID.P_DEPTH)).toBe(depth + 1);
  await shot(page, "06-mixer");
});

test("KILL cuts the band while held and gives it back", async ({ page }) => {
  await start(page);
  await view(page, "Mixer");
  const kill = page.locator(".strip").nth(0).locator(".kill").first();
  const low = await tp(page, 0, ID.P_EQ_LOW);
  const box = (await kill.boundingBox())!;
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down();
  await expect.poll(() => tp(page, 0, ID.P_EQ_LOW)).toBe(0);
  await page.mouse.up();
  await expect.poll(() => tp(page, 0, ID.P_EQ_LOW)).toBe(low);
  await kill.click({ modifiers: ["Shift"] });                    // locked
  await expect.poll(() => tp(page, 0, ID.P_EQ_LOW)).toBe(0);
  await kill.click({ modifiers: ["Shift"] });
  await expect.poll(() => tp(page, 0, ID.P_EQ_LOW)).toBe(low);
});

test("presets: search, categories, load; ↑ ↓ audition; undo", async ({ page }) => {
  await start(page);
  await page.getByRole("radio", { name: "All engines" }).click();
  await page.getByPlaceholder("Search presets").fill("bs 303");
  await page.locator(".pb li", { hasText: "BS 303" }).first().locator("button.nm").click();
  await expect.poll(() => engOf(page, 0)).toBe("ACID");
  await expect(page.locator(".pv")).toHaveText(/BS 303/);
  await page.getByPlaceholder("Search presets").fill("");
  await page.locator(".cats button", { hasText: /^BS$/ }).click();
  await shot(page, "07-presets-categories");
  const before = await page.locator(".pv").textContent();
  await page.locator(".pb .list").focus();
  await page.keyboard.press("ArrowDown");
  await expect(page.locator(".pv")).not.toHaveText(before!);
  await page.keyboard.press("Meta+z");
  await expect(page.locator(".pv")).toHaveText(before!);
});

test("sequencer: a note, its ACC and SLD, on the device; the acid generator writes a line", async ({ page }) => {
  await start(page);
  await track(page, 1);
  await engine(page, "D4");                                        // ACID
  await view(page, "Sequencer");
  await page.waitForSelector(".roll .grid");
  const grid = page.locator(".roll .grid");
  const gb = (await grid.boundingBox())!;
  // step 1, the row centred in view
  const sc = (await page.locator(".roll .body").boundingBox())!;
  await page.mouse.click(gb.x + 10, sc.y + sc.height / 2);
  await expect.poll(() => page.evaluate(() => { const g = (window as any).__gir; return g.device.ask(g.req.trackStep(1, 0)).then((s: any) => s.n); })).toBe(1);
  await page.locator(".lanes .flag").nth(0).click();               // ACC of step 1
  await page.locator(".lanes .row").nth(1).locator(".flag").nth(0).click();   // SLD of step 1
  await expect.poll(() => page.evaluate(() => { const g = (window as any).__gir; return g.device.ask(g.req.trackStep(1, 0)).then((s: any) => s.flags & 3); })).toBe(3);
  await page.getByRole("button", { name: "New acid line" }).click();
  await expect(page.locator(".toast", { hasText: "New acid line" })).toBeVisible({ timeout: 15_000 });
  const notes = await page.evaluate(async () => { const g = (window as any).__gir; let n = 0; for (let i = 0; i < 16; i++) { const s = await g.device.ask(g.req.trackStep(1, i)); if (s.time === 0 && s.n) n++; } return n; });
  expect(notes).toBeGreaterThan(3);
  await page.waitForTimeout(400);
  await shot(page, "08-sequenceur-acid");
});

test("drum grid: a hit and an accent on the device", async ({ page }) => {
  await start(page);
  await track(page, 3);
  await view(page, "Sequencer");
  await expect(page.locator(".dg")).toBeVisible();
  await page.getByRole("button", { name: "SNARE step 5", exact: true }).click();
  await page.getByRole("button", { name: /^SNARE step 13/ }).click({ modifiers: ["Shift"] });   // a hit, accented
  await expect.poll(() => page.evaluate(() => { const g = (window as any).__gir; return g.device.ask(g.req.trackStep(3, 4)).then((s: any) => s.hit & 2); })).toBe(2);
  await expect.poll(() => page.evaluate(() => { const g = (window as any).__gir; return g.device.ask(g.req.trackStep(3, 12)).then((s: any) => s.acc & 2); })).toBe(2);
  await shot(page, "09-grille-batterie");
});

test("song: NEW then COPY+ from the entry's menu", async ({ page }) => {
  await start(page);
  await view(page, "Song");
  await page.locator(".tl .row").nth(0).locator(".ent").nth(0).click({ button: "right" });
  await page.getByRole("menuitem", { name: "New pattern" }).click();
  await expect.poll(() => page.evaluate(() => (window as any).__gir.device.lists.tracks[0].ent[0].pat)).toBeGreaterThan(0);
  await page.locator(".tl .row").nth(0).locator(".ent").nth(0).click({ button: "right" });
  await page.getByRole("menuitem", { name: "Copy after the block" }).click();
  await expect.poll(() => page.evaluate(() => { const g = (window as any).__gir; return g.device.ask(g.req.list(0, 0, 0, null, true)).then((l: any) => l.tracks[0].ent[1].pat); })).toBeGreaterThan(0);
  await shot(page, "10-song");
});

test("device -> editor: a knob turned on the FM-1's panel shows in the editor", async ({ page }) => {
  await start(page);
  const before = await page.evaluate(() => [...(window as any).__gir.device.tracks[0].p]);
  await page.evaluate(() => (window as any).__gir.device.emu.panel({ type: "enc", role: 3, n: 7 }));
  await expect.poll(() => page.evaluate((b) => (window as any).__gir.device.tracks[0].p.findIndex((v: number, i: number) => v !== b[i]), before)).toBeGreaterThanOrEqual(0);
});

test("projects: save as, then a full backup, a change, a restore (with the backup before it) brings it back", async ({ page }) => {
  await start(page);
  await view(page, "Projects");
  await page.locator(".slots .slot").nth(0).getByRole("button", { name: "Save here" }).click();
  await page.locator(".dlg input").fill("NIGHT ACID");
  await page.locator(".dlg").getByRole("button", { name: "Save", exact: true }).click();
  await expect(page.locator(".slots .slot").nth(0).locator(".nm")).toHaveText("NIGHT ACID", { timeout: 10_000 });
  await shot(page, "11-projets");
  const dl = page.waitForEvent("download");
  await page.getByRole("button", { name: "Back up to a file" }).click();
  const file = await (await dl).path();
  expect(file).toBeTruthy();
  // the original editor's own reader (web/fm1backup.js) takes it: the same format, every object, every CRC
  const { readBackup } = await import("../../../fm1backup.js");
  const archive = readBackup(JSON.parse(readFileSync(file!, "utf8")));
  expect(archive.objects.length).toBeGreaterThanOrEqual(29);
  await page.evaluate(() => { const g = (window as any).__gir; return g.device.setGlobal(0, 133); });
  await expect.poll(() => page.evaluate(() => { const g = (window as any).__gir; return g.device.ask(g.req.get(1, 0)).then((r: any) => r.value); })).toBe(133);
  const chooser = page.waitForEvent("filechooser");
  await page.getByRole("button", { name: "Restore from a file…" }).click();
  await (await chooser).setFiles(file!);
  const before = page.waitForEvent("download");
  await page.locator(".dlg").getByRole("button", { name: "Restore", exact: true }).click();
  expect((await before).suggestedFilename()).toMatch(/^Skiff-backup-before-restore-/);
  await expect(page.locator(".toast", { hasText: "Restore finished." })).toBeVisible({ timeout: 60_000 });
  await expect.poll(() => page.evaluate(() => { const g = (window as any).__gir; return g.device.ask(g.req.get(1, 0)).then((r: any) => r.value); })).toBe(120);
});

test("other views render: library, samples, FM6, screen (live), settings (MENU), command palette, shortcuts, French", async ({ page }) => {
  await start(page);
  await view(page, "Library");
  await expect(page.locator(".dev .slot")).toHaveCount(32);
  await shot(page, "12-bibliotheque");
  await view(page, "Samples");
  await expect(page.locator(".smp .slot")).toHaveCount(3);
  await shot(page, "13-samples");
  await track(page, 1);
  await view(page, "FM6");
  await expect(page.locator(".fm6 h2")).toBeVisible();
  await shot(page, "14-fm6");
  await view(page, "Screen");
  await expect(page.locator("canvas")).toBeVisible();
  await page.waitForTimeout(600);
  await shot(page, "15-ecran");
  await view(page, "Settings");
  await expect(page.locator(".tab .item").first()).toBeVisible();
  await shot(page, "16-reglages");
  await page.keyboard.press("Meta+k");
  await page.getByPlaceholder(/Type a command/).fill("acid 303");
  await shot(page, "17-palette");
  await page.keyboard.press("Escape");
  await page.locator("body").press("?");
  await expect(page.getByRole("dialog", { name: "Keyboard shortcuts" })).toBeVisible();
  await shot(page, "18-raccourcis");
  await page.keyboard.press("Escape");
  await view(page, "Settings");
  await page.getByRole("radio", { name: "Français" }).click();
  await expect(page.getByRole("tab", { name: "Séquenceur" })).toBeVisible();
  await page.getByRole("tab", { name: "Son", exact: true }).click();
  await shot(page, "19-son-francais");
});
