<script lang="ts">
  // Under the grid: ACC and SLD per step (ACID's accent and slide; every engine has them), then one value lane to paint
  // with the mouse: VELOCITY, CHANCE, RATCHET or NUDGE
  import { device } from "../../lib/model/device.svelte";
  import type { Step } from "../../lib/protocol/types";
  import Seg from "../../ui/components/Seg.svelte";
  import { t } from "../../lib/i18n/index.svelte";
  interface Props { k: number; len: number; color: string; zoom: number; sel: Set<number> }
  let { k, len, color, zoom, sel }: Props = $props();
  const steps = $derived(device.steps[k] ?? []);
  const CW = $derived(Math.round(26 * zoom));
  type Lane = "vel" | "chance" | "ratchet" | "nudge";
  let lane = $state<Lane>("vel");
  const R: Record<Lane, [number, number]> = { vel: [1, 127], chance: [0, 100], ratchet: [1, 4], nudge: [-32, 31] };
  const val = (s: Step) => (lane === "vel" ? s.vel : lane === "chance" ? s.chance : lane === "ratchet" ? s.ratchet : s.nudge);
  const playable = (s: Step | undefined) => !!s && s.time === 0 && (s.n > 0 || s.hit > 0);
  function flag(i: number, bit: number) {
    const s = { ...steps[i], notes: [...steps[i].notes] };
    if (!playable(s)) return;
    s.flags ^= bit;
    device.editSteps(k, { [i]: s }, bit === 1 ? "accent" : "slide");
  }
  let box: HTMLDivElement;
  let painting = false;
  let pending: Record<number, Step> = {};
  function paint(e: PointerEvent) {
    const r = box.getBoundingClientRect();
    const i = Math.floor((e.clientX - r.left) / CW);
    if (i < 0 || i >= len || !playable(steps[i])) return;
    const [lo, hi] = R[lane];
    const v = Math.round(lo + (1 - Math.max(0, Math.min(1, (e.clientY - r.top) / r.height))) * (hi - lo));
    const s = { ...(pending[i] ?? steps[i]), notes: [...steps[i].notes] };
    if (lane === "vel") s.vel = v; else if (lane === "chance") s.chance = v; else if (lane === "ratchet") s.ratchet = v; else s.nudge = v;
    pending[i] = s;
    steps[i].vel = s.vel; steps[i].chance = s.chance; steps[i].ratchet = s.ratchet; steps[i].nudge = s.nudge;   // shown at once
  }
  function down(e: PointerEvent) { painting = true; pending = {}; box.setPointerCapture(e.pointerId); paint(e); }
  function move(e: PointerEvent) { if (painting) paint(e); }
  function up() { if (!painting) return; painting = false; if (Object.keys(pending).length) device.editSteps(k, pending, lane); pending = {}; }
  const h = (s: Step) => { const [lo, hi] = R[lane]; return lane === "nudge" ? Math.abs(val(s)) / 32 : (val(s) - lo) / (hi - lo); };
</script>

<div class="lanes" style="--c:{color}; --cw:{CW}px">
  {#each [[1, "ACC", t("seq.acc")], [2, "SLD", t("seq.sld")]] as [bit, nm, title]}
    <div class="row">
      <span class="ln dot" title={String(title)}>{nm}</span>
      <div class="cells" style="width:{CW * 64}px">
        {#each Array(64) as _, i}
          {@const s = steps[i]}
          <button class="flag" class:on={!!s && playable(s) && !!(s.flags & Number(bit))} class:dead={!playable(s)} class:out={i >= len} class:sel={sel.has(i)}
                  aria-label={`${nm} step ${i + 1}`} aria-pressed={!!s && !!(s.flags & Number(bit))} disabled={!playable(s) || i >= len}
                  onclick={() => flag(i, Number(bit))}></button>
        {/each}
      </div>
    </div>
  {/each}
  <div class="vhead">
    <Seg size="s" label="lane" value={lane} onchange={(v) => (lane = v)} {color}
         options={[{ value: "vel", label: t("seq.velocity") }, { value: "chance", label: t("seq.chance") }, { value: "ratchet", label: t("seq.ratchet") }, { value: "nudge", label: t("seq.nudge") }]} />
  </div>
  <div class="row">
    <span class="ln"></span>
    <div class="vals" bind:this={box} style="width:{CW * 64}px" role="presentation" onpointerdown={down} onpointermove={move} onpointerup={up}>
      {#if lane === "nudge"}<div class="mid"></div>{/if}
      {#each Array(64) as _, i}
        {@const s = steps[i]}
        {#if s && playable(s) && i < len}
          <div class="bar" class:neg={lane === "nudge" && s.nudge < 0} class:sel={sel.has(i)}
               style="left:{i * CW + 3}px; width:{CW - 6}px; height:{(lane === 'nudge' ? 50 : 100) * h(s)}%"
               title={`${i + 1}: ${val(s)}`}></div>
        {/if}
      {/each}
    </div>
  </div>
</div>

<style>
  .lanes { border: 1px solid var(--line); border-radius: var(--r-l); background: var(--surf); padding: 6px 0 8px; overflow-x: auto; }
  .row { display: flex; align-items: center; }
  .ln { width: 44px; flex: none; padding-left: 10px; font-size: 12px; color: var(--dim); position: sticky; left: 0; background: var(--surf); z-index: 1; }
  .cells { display: flex; flex: none; }
  .flag { width: var(--cw); height: 20px; background: none; border: 0; position: relative; padding: 0; }
  .flag::before { content: ""; position: absolute; inset: 5px calc(var(--cw) / 2 - 5px); border-radius: 3px; border: 1px solid var(--line-2); }
  .flag.on::before { background: var(--c); border-color: var(--c); }
  .flag.dead::before { border-color: transparent; background: var(--surf-2); inset: 9px calc(var(--cw) / 2 - 1px); }
  .flag.out { opacity: 0.25; }
  .flag.sel::after { content: ""; position: absolute; inset: 1px 1px; border: 1px solid var(--line-2); border-radius: 4px; }
  .vhead { padding: 8px 10px 6px 44px; position: sticky; left: 0; }
  .vals { position: relative; height: 84px; flex: none; cursor: ns-resize; background: repeating-linear-gradient(90deg, transparent 0 calc(var(--cw) * 4 - 1px), var(--line) calc(var(--cw) * 4 - 1px) calc(var(--cw) * 4)); touch-action: none; }
  .mid { position: absolute; left: 0; right: 0; top: 50%; border-top: 1px dashed var(--line-2); }
  .bar { position: absolute; bottom: 0; border-radius: 3px 3px 0 0; background: color-mix(in srgb, var(--c) 75%, transparent); border-top: 2px solid var(--c); }
  .lanes:has(.mid) .bar { bottom: 50%; }
  .lanes:has(.mid) .bar.neg { bottom: auto; top: 50%; border-top: 0; border-bottom: 2px solid var(--c); border-radius: 0 0 3px 3px; }
  .bar.sel { background: var(--c); }
</style>
