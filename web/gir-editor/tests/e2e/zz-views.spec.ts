// SPDX-License-Identifier: GPL-3.0-only
// (rc2) Every view at 1280 x 800 and 1920 x 1080, and the compact window, in English and French (SHOTS=1 only):
// livraison/EDITOR/screens/rc2/. Checks on the way: nothing wider than the window, no text cut off in a button.
import { test, expect } from "@playwright/test";
import { mkdirSync } from "node:fs";

const DIR = new URL("../../../../../livraison/EDITOR/screens/rc2/", import.meta.url).pathname;
const VIEWS = ["sound", "seq", "song", "mixer", "library", "projects", "samples", "fm6", "screen", "settings"];
for (const [w, h] of [[1280, 800], [1920, 1080]] as const)
  for (const lang of ["en", "fr"])
    test(`every view at ${w}x${h} (${lang}): no horizontal overflow, no cut button`, async ({ page }) => {
      test.skip(process.env.SHOTS !== "1" && lang === "fr", "screens only");
      mkdirSync(DIR, { recursive: true });
      await page.setViewportSize({ width: w, height: h });
      await page.addInitScript((l) => { localStorage.setItem("gir.lang", JSON.stringify(l)); localStorage.setItem("gir.view", JSON.stringify("sound")); }, lang);
      await page.goto("/");
      await page.getByRole("button", { name: /Try without an FM-1|Essayer sans FM-1/ }).click();
      await page.waitForFunction(() => (window as any).__gir?.device.status === "ready", null, { timeout: 60_000 });
      const bad: string[] = [];
      for (const v of VIEWS) {
        await page.evaluate((v) => ((window as any).__gir.ui.view = v), v);
        await page.waitForTimeout(v === "screen" || v === "song" ? 900 : 400);
        const r = await page.evaluate(() => {
          const out: string[] = [];
          if (document.documentElement.scrollWidth > innerWidth + 1) out.push("page wider than the window");
          for (const b of document.querySelectorAll<HTMLElement>("button, .tab, label")) {
            const st = getComputedStyle(b);
            if (b.offsetParent && b.scrollWidth > b.clientWidth + 2 && st.overflow !== "visible" && st.textOverflow !== "ellipsis" && b.clientWidth > 0)
              out.push("cut: " + (b.textContent || "").trim().slice(0, 40));
          }
          return out;
        });
        bad.push(...r.map((x) => `${v}: ${x}`));
        if (process.env.SHOTS === "1") await page.screenshot({ path: `${DIR}${w}-${lang}-${v}.png` });
      }
      console.log(`${w}x${h} ${lang}`, JSON.stringify(bad));
      expect(bad).toEqual([]);
    });

test("the compact window (560 x 460)", async ({ page }) => {
  mkdirSync(DIR, { recursive: true });
  await page.setViewportSize({ width: 560, height: 460 });
  await page.goto("/?compact=1&demo=1");
  await page.waitForFunction(() => (window as any).__gir?.device.status === "ready", null, { timeout: 60_000 });
  await page.waitForTimeout(600);
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1)).toBe(true);
  if (process.env.SHOTS === "1") await page.screenshot({ path: `${DIR}compact.png` });
});
