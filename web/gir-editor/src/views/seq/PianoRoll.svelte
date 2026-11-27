<script lang="ts">
  // The piano roll of a track's 64 steps: a note is a NOTE step's pitch, its length the TIE steps after it. Click an
  // empty cell: a note there (a step holds up to 4: a chord); drag a note: move it (pitch, step); drag its right edge:
  // its length; right-click or Delete: remove it. Click the ruler and drag: select steps; ⌘C ⌘V copy them, Delete
  // clears, ↑ ↓ transpose (Shift: an octave), ← → move. Every edit is written at once and undoes as one.
  import { device } from "../../lib/model/device.svelte";
  import type { Step } from "../../lib/protocol/types";
  import { NOTE_NAMES } from "../../lib/legacy/proto.js";

  interface Props { k: number; len: number; color: string; cw: number; n: number; sel: Set<number>; onselect: (s: Set<number>, focus?: number) => void }
  let { k, len, color, cw, n, sel, onselect }: Props = $props();
  const steps = $derived(device.steps[k] ?? []);
  const RH = 15, LO = 24, HI = 108;                // rows: C1 .. C8
  const CW = $derived(cw);
  const rows = HI - LO + 1;
  const y = (p: number) => (HI - p) * RH;

  interface N { i: number; j: number; p: number; len: number; vel: number; flags: number }
  const notes = $derived.by((): N[] => {
    const out: N[] = [];
    for (let i = 0; i < steps.length; i++) {
      const s = steps[i];
      if (!s || s.time !== 0 || !s.n) continue;
      let l = 1;
      while (i + l < steps.length && steps[i + l]?.time === 1) l++;
      for (let j = 0; j < s.n; j++) out.push({ i, j, p: s.notes[j], len: l, vel: s.vel, flags: s.flags });
    }
    return out;
  });

  const clone = (s: Step): Step => ({ ...s, notes: [...s.notes] });
  const rest = (s: Step): Step => ({ ...s, n: 0, notes: [0, 0, 0, 0], time: 2, flags: 0, hit: 0, acc: 0, ties: 0 });
  /** the steps of the tail after step i (TIE), up to the next NOTE */
  function tail(i: number) { let l = 0; while (i + 1 + l < steps.length && steps[i + 1 + l]?.time === 1) l++; return l; }

  function addNote(i: number, p: number) {
    const s = clone(steps[i]);
    const ch: Record<number, Step> = {};
    if (s.time === 0 && s.n > 0) {
      if (s.n >= 4 || s.notes.slice(0, s.n).includes(p)) return;
      s.notes[s.n] = p; s.n++;
    } else {
      s.time = 0; s.n = 1; s.notes = [p, 0, 0, 0]; s.vel = s.vel || 100; s.flags = 0;
    }
    ch[i] = s;
    device.editSteps(k, ch, "note");
    onselect(new Set([i]), i);
  }
  function removeNote(n: N) {
    const s = clone(steps[n.i]), ch: Record<number, Step> = {};
    const ps = s.notes.slice(0, s.n).filter((_, j) => j !== n.j);
    if (!ps.length) { ch[n.i] = rest(s); for (let t = 1; t < n.len; t++) ch[n.i + t] = rest(steps[n.i + t]); }
    else { s.n = ps.length; s.notes = [...ps, 0, 0, 0, 0].slice(0, 4); ch[n.i] = s; }
    device.editSteps(k, ch, "delete");
  }
  function setLen(i: number, l: number) {
    const cur = 1 + tail(i), ch: Record<number, Step> = {};
    l = Math.max(1, Math.min(l, len - i));
    for (let t = 1; t < Math.max(cur, l); t++) {
      const s = steps[i + t];
      if (!s) break;
      if (t < l) { if (s.time === 0 && s.n > 0) { l = t; break; } if (s.time !== 1) ch[i + t] = { ...rest(s), time: 1 }; }
      else if (s.time === 1) ch[i + t] = rest(s);
    }
    if (Object.keys(ch).length) device.editSteps(k, ch, "length");
  }
  function move(n: N, di: number, dp: number) {
    const ch: Record<number, Step> = {};
    const src = clone(steps[n.i]);
    if (di === 0) {
      const p = Math.max(0, Math.min(127, n.p + dp));
      if (src.notes.slice(0, src.n).includes(p)) return;
      src.notes[n.j] = p; ch[n.i] = src;
      device.editSteps(k, ch, "move");
      return;
    }
    const to = n.i + di;
    if (to < 0 || to + n.len > len) return;
    // the whole step moves (a chord with it), its tail too; what it lands on is replaced
    const moved = clone(src);
    moved.notes = moved.notes.map((x, j) => (j === n.j ? Math.max(0, Math.min(127, x + dp)) : x));
    for (let t = 0; t < n.len; t++) ch[n.i + t] = rest(steps[n.i + t]);
    ch[to] = moved;
    for (let t = 1; t < n.len; t++) ch[to + t] = { ...rest(steps[to + t]), time: 1 };
    device.editSteps(k, ch, "move");
    onselect(new Set([to]), to);
  }

  // pointer: a note body (move), its edge (length), the grid (add), the ruler (select)
  let grid: HTMLDivElement;
  let drag: null | { kind: "move" | "len" | "sel"; n?: N; x0: number; y0: number; di: number; dp: number; dl: number; a?: number } = $state(null);
  const at = (e: PointerEvent) => { const r = grid.getBoundingClientRect(); return { i: Math.floor((e.clientX - r.left) / CW), p: HI - Math.floor((e.clientY - r.top) / RH) }; };
  function noteDown(n: N, e: PointerEvent) {
    if (e.button === 2) return;
    e.stopPropagation();
    const r = (e.currentTarget as HTMLElement).getBoundingClientRect();
    const edge = e.clientX > r.right - 7;
    grid.setPointerCapture(e.pointerId);
    drag = { kind: edge ? "len" : "move", n, x0: e.clientX, y0: e.clientY, di: 0, dp: 0, dl: 0 };
    onselect(e.shiftKey ? new Set([...sel, n.i]) : new Set([n.i]), n.i);
  }
  function gridDown(e: PointerEvent) {
    if (e.button !== 0) return;
    const { i, p } = at(e);
    if (i < 0 || i >= len || p < LO || p > HI) return;
    addNote(i, p);
  }
  function rulerDown(e: PointerEvent) {
    const r = grid.getBoundingClientRect(), i = Math.max(0, Math.min(len - 1, Math.floor((e.clientX - r.left) / CW)));
    (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
    drag = { kind: "sel", x0: e.clientX, y0: 0, di: 0, dp: 0, dl: 0, a: i };
    onselect(new Set([i]), i);
  }
  function pmove(e: PointerEvent) {
    if (!drag) return;
    if (drag.kind === "sel") {
      const r = grid.getBoundingClientRect(), i = Math.max(0, Math.min(len - 1, Math.floor((e.clientX - r.left) / CW)));
      const [a, b] = [Math.min(drag.a!, i), Math.max(drag.a!, i)];
      onselect(new Set(Array.from({ length: b - a + 1 }, (_, x) => a + x)), drag.a);
      return;
    }
    drag.di = Math.round((e.clientX - drag.x0) / CW);
    drag.dp = -Math.round((e.clientY - drag.y0) / RH);
    drag.dl = drag.di;
  }
  function pup() {
    const d = drag; drag = null;
    if (!d || !d.n) return;
    if (d.kind === "len" && d.dl) setLen(d.n.i, d.n.len + d.dl);
    else if (d.kind === "move" && (d.di || d.dp)) move(d.n, d.di, d.dp);
  }
  function ghost(n: N) {
    if (!drag || drag.n !== n) return { i: n.i, p: n.p, len: n.len };
    if (drag.kind === "len") return { i: n.i, p: n.p, len: Math.max(1, n.len + drag.dl) };
    return { i: n.i + drag.di, p: n.p + drag.dp, len: n.len };
  }

  // the keyboard on the selection
  export function key(e: KeyboardEvent) {
    const mod = e.metaKey || e.ctrlKey;
    const idx = [...sel].sort((a, b) => a - b);
    if (!idx.length) return false;
    if (mod && e.key === "c") { clip = idx.map((i) => clone(steps[i])); return true; }
    if (mod && e.key === "v" && clip.length) {
      const ch: Record<number, Step> = {};
      clip.forEach((s, x) => { if (idx[0] + x < len) ch[idx[0] + x] = clone(s); });
      device.editSteps(k, ch, "paste");
      return true;
    }
    if (e.key === "Delete" || e.key === "Backspace") { const ch: Record<number, Step> = {}; for (const i of idx) ch[i] = rest(steps[i]); device.editSteps(k, ch, "clear"); return true; }
    if (e.key === "ArrowUp" || e.key === "ArrowDown") {
      const d = (e.key === "ArrowUp" ? 1 : -1) * (e.shiftKey ? 12 : 1), ch: Record<number, Step> = {};
      for (const i of idx) { const s = clone(steps[i]); if (s.time === 0 && s.n) { s.notes = s.notes.map((x, j) => (j < s.n ? Math.max(0, Math.min(127, x + d)) : x)); ch[i] = s; } }
      device.editSteps(k, ch, "transpose");
      return true;
    }
    if (e.key === "ArrowLeft" || e.key === "ArrowRight") {
      const d = e.key === "ArrowRight" ? 1 : -1;
      if (idx[0] + d < 0 || idx[idx.length - 1] + d >= len) return true;
      const ch: Record<number, Step> = {};
      for (const i of idx) ch[i] = rest(steps[i]);
      for (const i of idx) ch[i + d] = clone(steps[i]);
      device.editSteps(k, ch, "move");
      onselect(new Set(idx.map((i) => i + d)), idx[0] + d);
      return true;
    }
    return false;
  }
  let clip: Step[] = [];
  let scroller: HTMLDivElement;
  $effect(() => {                                  // centre the notes (or C4) when the track changes
    k;
    const ps = notes.map((n) => n.p);
    const mid = ps.length ? (Math.min(...ps) + Math.max(...ps)) / 2 : 60;
    queueMicrotask(() => { if (scroller) scroller.scrollTop = y(mid) - scroller.clientHeight / 2; });
  });
  const black = (p: number) => [1, 3, 6, 8, 10].includes(p % 12);
</script>

<div class="roll" style="--c:{color}; --cw:{CW}px; --rh:{RH}px">
  <div class="ruler" role="presentation" onpointerdown={rulerDown} onpointermove={pmove} onpointerup={pup}>
    <div class="kspace"></div>
    <div class="marks" style="width:{CW * n}px">
      {#each Array(n) as _, i}
        <span class="m" class:bar={i % 16 === 0} class:beat={i % 4 === 0} class:out={i >= len} class:sel={sel.has(i)} style="left:{i * CW}px">{i % 4 === 0 ? i / 4 + 1 : ""}</span>
      {/each}
    </div>
  </div>
  <div class="scroll body" data-scroll-x bind:this={scroller}>
    <div class="keys" style="height:{rows * RH}px">
      {#each Array(rows) as _, r}
        {@const p = HI - r}
        <div class="key" class:black={black(p)} class:c={p % 12 === 0}>{p % 12 === 0 ? NOTE_NAMES[0] + (p / 12 - 1) : ""}</div>
      {/each}
    </div>
    <div class="grid" bind:this={grid} role="grid" aria-label="Piano roll" tabindex="-1"
         style="width:{CW * n}px; height:{rows * RH}px; --len:{len * CW}px"
         onpointerdown={gridDown} onpointermove={pmove} onpointerup={pup} oncontextmenu={(e) => e.preventDefault()}>
      {#each Array(rows) as _, r}{#if black(HI - r)}<div class="brow" style="top:{r * RH}px"></div>{/if}{/each}
      {#each [...sel] as i}<div class="scol" style="left:{i * CW}px"></div>{/each}
      <div class="outside" style="left:{len * CW}px"></div>
      {#each notes as n (n.i + ":" + n.j)}
        {@const g = ghost(n)}
        <div class="note" class:sel={sel.has(n.i)} class:acc={n.flags & 1} class:sld={n.flags & 2} role="gridcell" tabindex="-1"
             aria-label={`${NOTE_NAMES[n.p % 12]}${Math.floor(n.p / 12) - 1}, step ${n.i + 1}`}
             style="left:{g.i * CW + 1}px; top:{y(g.p) + 1}px; width:{g.len * CW - 2}px; --o:{0.45 + (n.vel / 127) * 0.55}"
             onpointerdown={(e) => noteDown(n, e)} oncontextmenu={(e) => { e.preventDefault(); e.stopPropagation(); removeNote(n); }}
             ondblclick={(e) => { e.stopPropagation(); removeNote(n); }}>
          <span>{NOTE_NAMES[g.p % 12]}{Math.floor(g.p / 12) - 1}</span>
        </div>
      {/each}
    </div>
  </div>
</div>

<style>
  .roll { --kw: var(--lw, 72px); display: flex; flex-direction: column; border: 1px solid var(--line); border-radius: var(--r-l); overflow: hidden; background: var(--surf); }
  .ruler { display: flex; height: 24px; background: var(--surf-2); border-bottom: 1px solid var(--line); cursor: col-resize; overflow: hidden; }
  .kspace { width: var(--kw); flex: none; }
  .marks { position: relative; flex: none; }
  .m { position: absolute; top: 0; bottom: 0; width: var(--cw); font-size: 11px; color: var(--dim); padding: 5px 4px; border-left: 1px solid transparent; font-family: var(--font-dot); }
  .m.beat { border-left-color: var(--line-2); }
  .m.bar { color: var(--fg-2); border-left-color: var(--dim); }
  .m.out { opacity: 0.35; }
  .m.sel { background: color-mix(in srgb, var(--c) 30%, transparent); }
  .body { display: flex; height: 420px; overflow: auto; }
  .keys { width: var(--kw); flex: none; position: sticky; left: 0; z-index: 2; background: var(--surf-2); border-right: 1px solid var(--line); }
  .key { height: var(--rh); font-size: 9.5px; color: var(--dim); padding-left: 4px; line-height: var(--rh); border-bottom: 1px solid transparent; background: #d9d6dc; }
  .key.black { background: #2a2a2f; }
  .key.c { border-bottom-color: var(--line-2); color: #333; font-weight: 700; }
  .grid { position: relative; flex: none; cursor: crosshair;
          background-image: repeating-linear-gradient(90deg, transparent 0 calc(var(--cw) - 1px), var(--line) calc(var(--cw) - 1px) var(--cw)),
                            repeating-linear-gradient(90deg, transparent 0 calc(var(--cw) * 4 - 1px), var(--line-2) calc(var(--cw) * 4 - 1px) calc(var(--cw) * 4)),
                            repeating-linear-gradient(0deg, transparent 0 calc(var(--rh) - 1px), rgb(255 255 255 / 3%) calc(var(--rh) - 1px) var(--rh)); }
  .brow { position: absolute; left: 0; right: 0; height: var(--rh); background: rgb(0 0 0 / 22%); pointer-events: none; }
  .scol { position: absolute; top: 0; bottom: 0; width: var(--cw); background: color-mix(in srgb, var(--c) 9%, transparent); pointer-events: none; }
  .outside { position: absolute; top: 0; bottom: 0; right: 0; background: rgb(0 0 0 / 45%); pointer-events: none; }
  .note { position: absolute; height: calc(var(--rh) - 2px); border-radius: 3px; background: color-mix(in srgb, var(--c) calc(var(--o) * 100%), var(--surf-3)); cursor: grab; display: flex; align-items: center; overflow: hidden; box-shadow: inset -5px 0 0 rgb(0 0 0 / 22%); }
  .note span { font-size: 9px; font-weight: 700; color: #0b0b0b; padding-left: 3px; white-space: nowrap; }
  .note.sel { outline: 2px solid var(--fg); outline-offset: 0; }
  .note.acc::before { content: ""; position: absolute; left: 0; top: 0; bottom: 0; width: 3px; background: #fff; }
  .note.sld { border-top-right-radius: 8px; border-bottom-right-radius: 8px; }
  .note:hover { filter: brightness(1.12); }
</style>
