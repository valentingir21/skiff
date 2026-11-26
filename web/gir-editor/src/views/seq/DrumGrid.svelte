<script lang="ts">
  // A DRUM / DRUM-X track's grid: 8 lanes x the steps. Click a cell: a hit (Shift: accented, again: off); a hit lives
  // on a NOTE step (the device plays hits on NOTE steps only); a lane struck by a note shows as a hit and clicking it
  // takes that note off
  import { device } from "../../lib/model/device.svelte";
  import { stepLanes, stepAccents, drumLane } from "../../lib/legacy/proto.js";
  import type { Step } from "../../lib/protocol/types";
  interface Props { k: number; len: number; color: string; zoom: number; sel: Set<number>; onselect: (s: Set<number>, focus?: number) => void }
  let { k, len, color, zoom, sel, onselect }: Props = $props();
  const LANES = ["KICK", "SNARE", "CLAP", "CL HAT", "OP HAT", "TOM", "RIM", "BELL"];
  const steps = $derived(device.steps[k] ?? []);
  const CW = $derived(Math.round(26 * zoom));
  function toggle(i: number, l: number, accent: boolean) {
    const s: Step = { ...steps[i], notes: [...steps[i].notes] };
    const on = (stepLanes(s) >> l) & 1, acc = (stepAccents(s) >> l) & 1;
    if (accent && on) { s.acc = acc ? s.acc & ~(1 << l) : s.acc | (1 << l); if (!(s.hit >> l & 1)) s.hit |= 1 << l; }
    else if (on) {
      s.hit &= ~(1 << l); s.acc &= ~(1 << l);
      const keep = s.notes.slice(0, s.n).filter((n) => drumLane(n) !== l);
      s.n = keep.length; s.notes = [...keep, 0, 0, 0, 0].slice(0, 4);
      if (!s.n && !s.hit) { s.time = 2; s.flags = 0; }
    } else {
      s.hit |= 1 << l; if (accent) s.acc |= 1 << l;
      if (s.time !== 0) { s.time = 0; s.n = 0; s.notes = [0, 0, 0, 0]; s.vel = s.vel || 100; }
    }
    s.ties = 0;
    device.editSteps(k, { [i]: s }, "hit");
    onselect(new Set([i]), i);
  }
</script>
<div class="dg" style="--c:{color}; --cw:{CW}px">
  {#each LANES as ln, l}
    <div class="row">
      <span class="ln dot">{ln}</span>
      <div class="cells" style="width:{CW * 64}px">
        {#each Array(64) as _, i}
          {@const s = steps[i]}
          {@const on = s ? (stepLanes(s) >> l) & 1 : 0}
          {@const ac = s ? (stepAccents(s) >> l) & 1 : 0}
          <button class="cell" class:on={!!on} class:acc={!!ac} class:beat={i % 4 === 0} class:out={i >= len} class:sel={sel.has(i)}
                  aria-label={`${ln} step ${i + 1}${on ? (ac ? ", accent" : ", hit") : ""}`} aria-pressed={!!on}
                  disabled={i >= len} onclick={(e) => toggle(i, l, e.shiftKey)}></button>
        {/each}
      </div>
    </div>
  {/each}
</div>
<style>
  .dg { border: 1px solid var(--line); border-radius: var(--r-l); background: var(--surf); padding: 8px 0; overflow-x: auto; }
  .row { display: flex; align-items: center; }
  .ln { width: 72px; flex: none; padding-left: 12px; font-size: 12.5px; color: var(--fg-2); position: sticky; left: 0; background: var(--surf); z-index: 1; }
  .cells { display: flex; flex: none; }
  .cell { width: var(--cw); height: 30px; padding: 3px 2px; background: none; border: 0; position: relative; }
  .cell::before { content: ""; position: absolute; inset: 4px 2px; border-radius: 5px; background: var(--surf-3); transition: background var(--t-fast); }
  .cell.beat::before { background: var(--surf-4); }
  .cell:hover::before { background: color-mix(in srgb, var(--c) 35%, var(--surf-3)); }
  .cell.on::before { background: color-mix(in srgb, var(--c) 70%, var(--surf-3)); }
  .cell.acc::before { background: var(--c); box-shadow: 0 0 0 2px var(--fg) inset; }
  .cell.sel::after { content: ""; position: absolute; inset: 1px 0; border-radius: 6px; border: 1px solid var(--fg-2); }
  .cell.out { opacity: 0.25; }
</style>
