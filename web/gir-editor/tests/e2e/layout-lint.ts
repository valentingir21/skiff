// SPDX-License-Identifier: GPL-3.0-only
// (rc3) The layout lint: what is wrong on the screen as it stands, found from the boxes the browser lays out. It runs in
// the page (page.evaluate(lint, opts)) and returns each defect with its rule, the element and where it is. The rules:
//   overlap   two controls, labels, values or texts overlap by more than 1 px
//   gap       less than 8 px between a knob's value and what is under it
//   cut       a text cut off or spilling out of its box (an ellipsis is meant: allowed; a line clamp too)
//   clipped   something cut by the edge of its frame or of the window, or hidden to the side, where no sideways
//             scrolling is meant (a frame meant to scroll sideways says so: data-scroll-x)
//   card      something sticking out of its card
//   size      a knob of another size than the app's two (normal, compact)
//   row       knobs side by side whose tops differ; a row's label off its knobs' middle
//   columns   two cards side by side, as wide, whose knobs are not in the same columns
//   steps     a step's boxes not in one column from row to row (drum grid, flags, lanes)
//   name      a control without an accessible name (aria-label, a label or its own text: a title is not one)
//   chips     a row of chips that wraps and leaves one alone on a line
//   desc      a description that is empty (the engine's, the preset's)
//   presets   the Sound view without its presets list in sight, or a button to open it
export interface Defect { rule: string; el: string; text: string; x: number; y: number; w: number; h: number; more?: string }
export interface LintOpts { root?: string; knobSizes: number[]; view: string }

export function lint(o: LintOpts): Defect[] {
  const out: Defect[] = [];
  const root: Element = (o.root && document.querySelector(o.root)) || document.body;
  const VW = document.documentElement.clientWidth, VH = document.documentElement.clientHeight;
  type R = { l: number; t: number; r: number; b: number };
  const rr = (d: DOMRect): R => ({ l: d.left, t: d.top, r: d.right, b: d.bottom });
  const inter = (a: R, b: R): R => ({ l: Math.max(a.l, b.l), t: Math.max(a.t, b.t), r: Math.min(a.r, b.r), b: Math.min(a.b, b.b) });
  const W = (a: R) => Math.max(0, a.r - a.l), H = (a: R) => Math.max(0, a.b - a.t);
  const desc = (el: Element) => {
    const h = el as HTMLElement;
    const cls = typeof h.className === "string" && h.className ? "." + h.className.trim().split(/\s+/).filter((c) => !c.startsWith("svelte-")).slice(0, 2).join(".") : "";
    const lab = h.getAttribute?.("aria-label");
    let p = el.parentElement, ctx = "";
    for (let i = 0; p && i < 6; i++, p = p.parentElement) {
      const l = p.getAttribute("aria-label") || (p.matches(".card, article, section") ? (p.querySelector("h3, h2")?.textContent || "").trim() : "");
      if (l) { ctx = l.slice(0, 24) + " › "; break; }
    }
    return ctx + el.tagName.toLowerCase() + (cls === "." ? "" : cls) + (lab ? `[${lab.slice(0, 30)}]` : "");
  };
  const txt = (el: Element) => ((el as HTMLElement).innerText ?? el.textContent ?? "").trim().replace(/\s+/g, " ").slice(0, 40);
  const add = (rule: string, el: Element, more = "", box?: R) => {
    const b = box ?? rr(el.getBoundingClientRect());
    out.push({ rule, el: desc(el), text: txt(el), x: Math.round(b.l), y: Math.round(b.t), w: Math.round(W(b)), h: Math.round(H(b)), more });
  };
  const shown = (el: Element) => {
    const h = el as HTMLElement;
    if (!h.getClientRects || h.getClientRects().length === 0) return false;
    const s = getComputedStyle(h);
    if (s.visibility === "hidden" || s.visibility === "collapse") return false;
    for (let p: Element | null = h; p && p !== document.body; p = p.parentElement) if (+getComputedStyle(p).opacity < 0.02) return false;
    return true;
  };
  const hiddenAria = (el: Element) => !!el.closest("[aria-hidden='true']");

  // ---- the visible part of each element: its box inside every ancestor that clips it (and the window)
  const clipCache = new Map<Element, { x: boolean; y: boolean; r: R; scrollX: boolean; scrollY: boolean; meant: boolean }>();
  function clipper(p: Element) {
    let c = clipCache.get(p);
    if (!c) {
      const s = getComputedStyle(p);
      const x = s.overflowX !== "visible", y = s.overflowY !== "visible";
      c = { x, y, r: rr(p.getBoundingClientRect()), scrollX: /auto|scroll/.test(s.overflowX), scrollY: /auto|scroll/.test(s.overflowY), meant: p.hasAttribute("data-scroll-x") };
      // a box with overflow hidden clips at its padding box: near enough its border box here
      clipCache.set(p, c);
    }
    return c;
  }
  interface Vis { v: R; r: R; cutX: Element | null; cutXmeant: boolean; cutY: Element | null; cutYscroll: boolean; fixed: Element | null }
  function visOf(el: Element): Vis {
    const r = rr(el.getBoundingClientRect());
    let v = { ...r }, cutX: Element | null = null, cutXmeant = false, cutY: Element | null = null, cutYscroll = true, fixed: Element | null = null;
    let free = getComputedStyle(el).position === "fixed";     // a fixed box is clipped by the window only
    for (let p = el.parentElement; p; p = p.parentElement) {
      const s = getComputedStyle(p);
      if (!fixed && (s.position === "fixed")) fixed = p;
      if (p === document.documentElement) break;
      if (free) continue;
      if (s.position === "fixed") free = true;
      const c = clipper(p);
      if (c.x) { const n = { ...v, l: Math.max(v.l, c.r.l), r: Math.min(v.r, c.r.r) }; if (W(n) < W(v) - 1 && !cutX) { cutX = p; cutXmeant = c.meant; } v = n; }
      if (c.y) { const n = { ...v, t: Math.max(v.t, c.r.t), b: Math.min(v.b, c.r.b) }; if (H(n) < H(v) - 1 && !cutY) { cutY = p; cutYscroll = c.scrollY; } v = n; }
    }
    if (getComputedStyle(el).position === "fixed") fixed = el;
    const win = { l: 0, t: 0, r: VW, b: VH };
    const n = inter(v, win);
    if (W(n) < W(v) - 1 && !cutX) { cutX = document.documentElement; cutXmeant = false; }
    if (H(n) < H(v) - 1 && !cutY) { cutY = document.documentElement; cutYscroll = true; }
    return { v: n, r, cutX, cutXmeant, cutY, cutYscroll, fixed };
  }

  // ---- the units: controls, knob parts, headings, texts (the outermost of them: a button's inner spans are its own)
  const ATOM = "button, a[href], input, select, textarea, [role=slider], [role=tab], [role=option], [role=radio], [role=checkbox], [role=switch], .knob .label, .knob .val, h1, h2, h3, h4, label, canvas, svg, [data-lint-unit]";
  const isAtom = (el: Element) => el.matches(ATOM) || [...el.childNodes].some((n) => n.nodeType === 3 && n.textContent!.trim().length > 0);
  const all = [...root.querySelectorAll("*")].filter((el) => !el.closest(".toasts, .helpbar, [data-lint-skip]") && el.tagName !== "OPTION" && shown(el));
  const units: { el: Element; vis: Vis }[] = [];
  for (const el of all) {
    if (!isAtom(el) || hiddenAria(el)) continue;
    let up = el.parentElement, inside = false;
    for (; up && up !== root.parentElement; up = up.parentElement) if (up !== el && up.matches(ATOM)) { inside = true; break; }
    if (inside) continue;
    const vis = visOf(el);
    if (W(vis.r) < 1 || H(vis.r) < 1) continue;
    units.push({ el, vis });
  }

  // ---- clipped: cut by a frame's side (or hidden to the side) where no sideways scroll is meant; cut at the bottom of
  // a frame that does not scroll
  for (const u of units) {
    const { v, cutX, cutXmeant, cutY, cutYscroll } = u.vis;
    if (cutX && !cutXmeant) add("clipped", u.el, (W(v) < 1 ? "hidden to the side by " : "cut on the side by ") + desc(cutX));
    if (cutY && !cutYscroll && H(u.vis.v) > 0) add("clipped", u.el, "cut at the bottom by " + desc(cutY));
  }
  const seen = units.filter((u) => W(u.vis.v) >= 1 && H(u.vis.v) >= 1);

  // ---- overlap (same layer: a fixed overlay against itself only)
  const ov: string[] = [];
  const byX = [...seen].sort((a, b) => a.vis.v.l - b.vis.v.l);
  for (let i = 0; i < byX.length; i++) {
    const a = byX[i];
    for (let j = i + 1; j < byX.length; j++) {
      const b = byX[j];
      if (b.vis.v.l >= a.vis.v.r - 1) break;
      if (a.vis.fixed !== b.vis.fixed) continue;
      if (a.el.contains(b.el) || b.el.contains(a.el)) continue;
      if (a.el.closest("[data-lint-over]") || b.el.closest("[data-lint-over]")) continue;   // drawn over on purpose (the screen on the panel)
      const n = inter(a.vis.v, b.vis.v);
      if (W(n) > 1 && H(n) > 1) {
        const key = desc(a.el) + "|" + desc(b.el);
        if (ov.length < 400) ov.push(key);
        add("overlap", a.el, "with " + desc(b.el) + ` "${txt(b.el)}" (${Math.round(W(n))}x${Math.round(H(n))})`, n);
      }
    }
  }

  // ---- gap under a knob's value
  for (const val of seen.filter((u) => u.el.matches(".knob .val"))) {
    const k = val.el.closest(".knob");
    let best = 1e9, who: Element | null = null;
    for (const u of seen) {
      if (u === val || (k && k.contains(u.el)) || u.vis.fixed !== val.vis.fixed) continue;
      if (u.vis.v.t < val.vis.v.b - 0.5 || u.vis.v.r <= val.vis.v.l || u.vis.v.l >= val.vis.v.r) continue;
      const g = u.vis.v.t - val.vis.v.b;
      if (g < best) { best = g; who = u.el; }
    }
    if (who && best < 8) add("gap", val.el, `${Math.round(best)} px above ${desc(who)}`);
  }

  // ---- cut texts
  for (const el of all) {
    if (hiddenAria(el)) continue;
    const h = el as HTMLElement;
    const own = [...h.childNodes].some((n) => n.nodeType === 3 && n.textContent!.trim().length > 0);
    if (!own && !h.matches("input, select, button")) continue;
    const s = getComputedStyle(h);
    if (s.display === "inline" || h.clientWidth <= 1 || h.matches(".sr-only") || h.closest("svg")) continue;   // (SVG text has no box to spill out of)
    if (/auto|scroll/.test(s.overflowX) || /auto|scroll/.test(s.overflowY)) continue;
    if (h.tagName === "INPUT" || h.tagName === "TEXTAREA") continue;
    if (h.scrollWidth > h.clientWidth + 1 && s.textOverflow !== "ellipsis") add("cut", el, `text ${h.scrollWidth} px in ${h.clientWidth} px`);
    const clamp = s.getPropertyValue("-webkit-line-clamp");
    if (s.overflowY !== "visible" && h.scrollHeight > h.clientHeight + 2 && (!clamp || clamp === "none")) add("cut", el, `text ${h.scrollHeight} px high in ${h.clientHeight} px`);
  }

  // ---- out of its card
  for (const u of seen) {
    const card = u.el.parentElement?.closest(".card, article, section.strip, [data-card]");
    if (!card || !root.contains(card)) continue;
    let scroll = false;
    for (let p = u.el.parentElement; p && p !== card; p = p.parentElement) { const c = clipper(p); if (c.scrollX || c.scrollY || c.x) { scroll = true; break; } }
    if (scroll || u.el.closest(".pick, [role=listbox][data-pop]")) continue;
    const c = rr(card.getBoundingClientRect()), r = u.vis.r;
    if (r.l < c.l - 1 || r.r > c.r + 1 || r.t < c.t - 1 || r.b > c.b + 1) add("card", u.el, "outside " + desc(card));
  }

  // ---- knobs: sizes, rows, columns
  const dials = seen.filter((u) => u.el.matches(".knob .dial"));
  for (const d of dials) {
    const w = Math.round(d.vis.r.r - d.vis.r.l);
    if (!o.knobSizes.some((s) => Math.abs(s - w) <= 1)) add("size", d.el, `${w} px (the app's: ${o.knobSizes.join(", ")})`);
  }
  const cardOf = (el: Element) => el.closest(".card, article, section.strip, [data-card]") ?? el.closest("section, div");
  const byCard = new Map<Element, typeof dials>();
  for (const d of dials) { const c = cardOf(d.el); if (c) byCard.set(c, [...(byCard.get(c) ?? []), d]); }
  for (const [, ds] of byCard)
    for (let i = 0; i < ds.length; i++)
      for (let j = i + 1; j < ds.length; j++) {
        const a = ds[i].vis.r, b = ds[j].vis.r;
        const ovY = Math.min(a.b, b.b) - Math.max(a.t, b.t);
        if (ovY > Math.min(H(a), H(b)) * 0.3 && Math.abs(a.t - b.t) > 1.5 && Math.abs(W(a) - W(b)) < 1) add("row", ds[j].el, `top ${Math.round(b.t)} beside a knob at ${Math.round(a.t)}`);
      }
  for (const lab of seen.filter((u) => u.el.matches("[data-row-label], .routes .rl"))) {
    const l = lab.vis.r, mid = (l.t + l.b) / 2;
    const row = dials.filter((d) => d.vis.r.t <= mid + 30 && d.vis.r.b >= mid - 30 && d.vis.r.l > l.l && cardOf(d.el) === cardOf(lab.el));
    if (!row.length) continue;
    const dm = row.map((d) => (d.vis.r.t + d.vis.r.b) / 2).sort((a, b) => Math.abs(a - mid) - Math.abs(b - mid))[0];
    if (Math.abs(dm - mid) > 4) add("row", lab.el, `${Math.round(mid - dm)} px off its knobs' middle`);
  }
  const cards = [...byCard.keys()].filter((c) => c.matches(".card, article, [data-card]"));
  for (let i = 0; i < cards.length; i++)
    for (let j = i + 1; j < cards.length; j++) {
      const A = rr(cards[i].getBoundingClientRect()), B = rr(cards[j].getBoundingClientRect());
      if (Math.abs(A.t - B.t) > 1 || Math.abs(W(A) - W(B)) > 2 || cards[i].parentElement !== cards[j].parentElement) continue;
      const first = (ds: typeof dials, c: R) => { const t0 = Math.min(...ds.map((d) => d.vis.r.t)); return ds.filter((d) => d.vis.r.t < t0 + 4).map((d) => (d.vis.r.l + d.vis.r.r) / 2 - c.l).sort((a, b) => a - b); };
      const a = first(byCard.get(cards[i])!, A), b = first(byCard.get(cards[j])!, B);
      for (let n = 0; n < Math.min(a.length, b.length); n++)
        if (Math.abs(a[n] - b[n]) > 2) { add("columns", cards[j], `knob ${n + 1} at ${Math.round(b[n])} px, beside ${Math.round(a[n])} px`); break; }
    }

  // ---- step columns
  const steps = new Map<number, number[]>();
  for (const u of seen) {
    const m = /\bstep (\d+)\b/.exec(u.el.getAttribute("aria-label") || "");
    if (!m || u.el.closest("[role=grid]")) continue;
    steps.set(+m[1], [...(steps.get(+m[1]) ?? []), (u.vis.r.l + u.vis.r.r) / 2]);
  }
  for (const [i, xs] of steps) if (Math.max(...xs) - Math.min(...xs) > 1.5) {
    const el = seen.find((u) => new RegExp(`\\bstep ${i}\\b`).test(u.el.getAttribute("aria-label") || ""))!.el;
    add("steps", el, `step ${i}'s boxes from x ${Math.round(Math.min(...xs))} to ${Math.round(Math.max(...xs))}`);
  }

  // ---- accessible names
  const NAMED = "button, a[href], input:not([type=hidden]), select, textarea, [role=slider], [role=tab], [role=option], [role=radio], [role=checkbox], [role=switch], [tabindex]:not([tabindex='-1'])";
  const ownText = (el: Element): string => {
    let s = "";
    const walk = (n: Node) => {
      if (n.nodeType === 3) { s += n.textContent; return; }
      if (n.nodeType !== 1) return;
      const e = n as HTMLElement;
      if (e.getAttribute("aria-hidden") === "true") return;
      if (e.getClientRects().length === 0 && e.tagName !== "SPAN") return;
      if (getComputedStyle(e).display === "none") return;
      if (e !== el && e.getAttribute("aria-label")) { s += e.getAttribute("aria-label"); return; }
      e.childNodes.forEach(walk);
    };
    walk(el);
    return s.trim();
  };
  for (const el of all.filter((e) => e.matches(NAMED))) {
    if (hiddenAria(el)) continue;
    const h = el as HTMLElement;
    let name = h.getAttribute("aria-label")?.trim() || "";
    if (!name && h.getAttribute("aria-labelledby")) name = h.getAttribute("aria-labelledby")!.split(/\s+/).map((id) => document.getElementById(id)?.textContent ?? "").join(" ").trim();
    if (!name && (h as HTMLInputElement).labels?.length) name = [...(h as HTMLInputElement).labels!].map((l) => l.textContent).join(" ").trim();
    if (!name && !h.matches("input, select, textarea")) name = ownText(h);
    if (!name || !/[\p{L}\p{N}]/u.test(name)) add("name", el, h.title ? `title only: "${h.title.slice(0, 30)}"` : "no name");
  }

  // ---- chips that wrap and leave one alone
  for (const c of all.filter((e) => e.matches(".cats, [data-lint=chips]"))) {
    const kids = [...c.children].filter(shown);
    const tops = [...new Set(kids.map((k) => Math.round(k.getBoundingClientRect().top)))];
    if (tops.length < 2) continue;
    const last = kids.filter((k) => Math.round(k.getBoundingClientRect().top) === tops[tops.length - 1]);
    if (last.length === 1) add("chips", last[0], `${tops.length} lines, "${txt(last[0])}" alone on the last`);
  }

  // ---- descriptions
  for (const d of all.filter((e) => e.matches("[data-desc], .sound .head .pr"))) if (!txt(d)) add("desc", d, "empty description");
  if (o.view === "sound" && !o.root) {
    const list = [...document.querySelectorAll("[role=region]")].find((e) => /^(Presets|Les presets|Presets du moteur)$/i.test(e.getAttribute("aria-label") || ""));
    const inSight = (e: Element | undefined) => { if (!e || !shown(e)) return false; const v = visOf(e).v; return W(v) > 120 && H(v) > 120; };
    const btn = [...document.querySelectorAll("[data-presets-toggle]")].find((b) => { if (!shown(b)) return false; const v = visOf(b).v; return W(v) > 4 && H(v) > 4; });
    if (!inSight(list) && !btn) add("presets", document.querySelector(".sound") ?? document.body, "no presets list in sight, no button to open it");
  }
  return out;
}
