// SPDX-License-Identifier: GPL-3.0-only
// (rc3) The window's breakpoints, one place for all views (CSS px, so a zoomed browser counts as a smaller window):
//   wide    >= 1440  everything in sight: the tracks, the content, the presets beside
//   mid     1180 .. 1440  the presets become a drawer on the right (a button opens it)
//   narrow  < 1180   the tracks column turns compact (number, colour, short name), the tabs keep short labels
export const BP = { wide: 1440, mid: 1180, tiny: 900 };
export const win = $state({ w: typeof window === "undefined" ? 1440 : window.innerWidth, drawer: false });
if (typeof window !== "undefined") addEventListener("resize", () => { win.w = window.innerWidth; if (win.w >= BP.wide) win.drawer = false; });
export const isWide = () => win.w >= BP.wide;
export const isNarrow = () => win.w < BP.mid;
/** under 900 px the tabs drop their icons, then use their short labels */
export const isTiny = () => win.w < BP.tiny;

/** (rc3) the Sound view's cards packed in rows of `cols` columns, every row full: an item that does not fit the row's
 *  end stretches the one before it; a family's last row ends on its last item, stretched */
export function pack<T>(items: T[], natural: (x: T) => number, cols: number): { item: T; span: number; stretched: boolean }[] {
  const out: { item: T; span: number; stretched: boolean }[] = [];
  let used = 0;
  for (const it of items) {
    const n = Math.min(cols, natural(it));
    if (used + n > cols && out.length) { const last = out[out.length - 1]; last.span += cols - used; last.stretched = last.span > natural(last.item); used = 0; }
    out.push({ item: it, span: n, stretched: false });
    used = (used + n) % cols;
  }
  if (used && out.length) { const last = out[out.length - 1]; last.span += cols - used; last.stretched = last.span > natural(last.item); }
  return out;
}
