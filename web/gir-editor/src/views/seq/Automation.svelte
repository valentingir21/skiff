<script lang="ts">
  // The track's recorded knob moves (automation) and parameter locks (a value on one step only), as lanes under the
  // grid, one per parameter. Click a lane's step: an event there (Shift: a lock) at the value of the knob now;
  // drag a dot up / down: its value; right-click: delete. 64 records are shared by the six tracks.
  import { device } from "../../lib/model/device.svelte";
  import { fmtValue } from "../../lib/legacy/proto.js";
  import { t } from "../../lib/i18n/index.svelte";
  interface Props { k: number; len: number; color: string; zoom: number }
  let { k, len, color, zoom }: Props = $props();
  const CW = $derived(Math.round(26 * zoom));
  const m = $derived(device.motion[k]);
  const tr = $derived(device.tracks[k]);
  let extra = $state<number[]>([]);
  const ids = $derived([...new Set([...(m?.events.map((e) => e.param) ?? []), ...extra])].sort((a, b) => a - b));
  const d = (id: number) => (tr ? device.desc(id, tr.engine) : undefined);
  let add = $state("");
  const choices = $derived(tr ? Array.from({ length: device.info?.pcount ?? 0 }, (_, id) => id).filter((id) => d(id) && d(id)!.max > d(id)!.min && !ids.includes(id)) : []);
  let dragging: { id: number; step: number; lock: boolean; y0: number; v0: number; v: number } | null = $state(null);
  function lanePos(e: MouseEvent) { const r = (e.currentTarget as HTMLElement).getBoundingClientRect(); return Math.floor((e.clientX - r.left) / CW); }
  async function click(id: number, e: MouseEvent) {
    const step = lanePos(e);
    if (step < 0 || step >= len || !tr) return;
    if (m?.events.some((x) => x.step === step && x.param === id)) return;
    await device.setMotion(k, step, id, tr.p[id], e.shiftKey);
  }
  function dotDown(id: number, ev: { step: number; value: number; lock: boolean }, e: PointerEvent) {
    e.stopPropagation();
    if (e.button === 2) return;
    (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
    dragging = { id, step: ev.step, lock: ev.lock, y0: e.clientY, v0: ev.value, v: ev.value };
  }
  function dotMove(e: PointerEvent) {
    if (!dragging) return;
    const ds = d(dragging.id); if (!ds) return;
    dragging.v = Math.max(ds.min, Math.min(ds.max, Math.round(dragging.v0 + ((dragging.y0 - e.clientY) / 60) * (ds.max - ds.min))));
  }
  async function dotUp() { const x = dragging; dragging = null; if (x && x.v !== x.v0) await device.setMotion(k, x.step, x.id, x.v, x.lock); }
  const frac = (id: number, v: number) => { const ds = d(id); return ds ? (v - ds.min) / Math.max(1, ds.max - ds.min) : 0; };
</script>

<section class="auto" style="--c:{color}; --cw:{CW}px">
  <header>
    <h3>Automation</h3>
    {#if m}<span class="cnt">{m.events.length} · {m.events.filter((e) => e.lock).length} locks</span>
      <button class="pl" class:on={m.on} aria-pressed={m.on} onclick={() => device.motionPlay(k, !m.on)}>{m.on ? "PLAY ON" : "PLAY OFF"}</button>{/if}
    <select bind:value={add} onchange={() => { if (add !== "") extra = [...extra, +add]; add = ""; }} aria-label="Add a lane">
      <option value="">+ lane…</option>
      {#each choices as id}<option value={id}>{d(id)?.label} ({id})</option>{/each}
    </select>
    <span class="hint">{t("seq.autoHint")}</span>
  </header>
  {#each ids as id}
    {@const evs = m?.events.filter((e) => e.param === id) ?? []}
    <div class="lane">
      <span class="ln dot" title={String(id)}>{d(id)?.label ?? id}</span>
      <div class="track" style="width:{CW * 64}px" role="presentation" onclick={(e) => click(id, e)} onpointermove={dotMove} onpointerup={dotUp}>
        {#each evs as ev}
          {@const v = dragging && dragging.id === id && dragging.step === ev.step ? dragging.v : ev.value}
          <button class="dot-ev" class:lock={ev.lock} style="left:{ev.step * CW + CW / 2 - 6}px; bottom:{4 + frac(id, v) * 28}px"
                  title={`${ev.lock ? "LOCK" : "AUTO"} step ${ev.step + 1}: ${d(id) ? fmtValue(d(id), v).join(" ") : v}`} aria-label={`step ${ev.step + 1}`}
                  onpointerdown={(e) => dotDown(id, ev, e)} oncontextmenu={(e) => { e.preventDefault(); e.stopPropagation(); device.delMotion(k, ev.step, id); }}></button>
        {/each}
        <div class="out" style="left:{len * CW}px"></div>
      </div>
    </div>
  {:else}
    <p class="none">{t("seq.autoEmpty")}</p>
  {/each}
</section>
<style>
  .auto { border: 1px solid var(--line); border-radius: var(--r-l); background: var(--surf); padding: 10px 0; overflow-x: auto; }
  header { display: flex; align-items: center; gap: 10px; padding: 0 12px 8px; position: sticky; left: 0; }
  h3 { margin: 0; font-size: 13px; }
  .cnt { color: var(--dim); font-size: 12px; }
  .pl { border: 1px solid var(--line-2); background: var(--surf-3); color: var(--dim); border-radius: 6px; font-size: 11px; font-weight: 700; padding: 3px 8px; }
  .pl.on { color: #0b0b0b; background: var(--c); border-color: var(--c); }
  select { background: var(--surf-3); border: 1px solid var(--line-2); border-radius: 6px; height: 26px; font-size: 12px; }
  .hint { color: var(--mute); font-size: 11.5px; margin-left: auto; }
  .lane { display: flex; align-items: center; }
  .ln { width: 72px; flex: none; padding-left: 12px; font-size: 12px; color: var(--fg-2); position: sticky; left: 0; background: var(--surf); z-index: 1; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .track { position: relative; height: 40px; flex: none; border-top: 1px solid var(--line); cursor: copy;
           background: repeating-linear-gradient(90deg, transparent 0 calc(var(--cw) * 4 - 1px), var(--line) calc(var(--cw) * 4 - 1px) calc(var(--cw) * 4)); }
  .dot-ev { position: absolute; width: 12px; height: 12px; border-radius: 50%; border: 2px solid var(--c); background: var(--bg); padding: 0; cursor: ns-resize; }
  .dot-ev.lock { border-radius: 3px; background: var(--c); }
  .out { position: absolute; top: 0; bottom: 0; right: 0; background: rgb(0 0 0 / 40%); pointer-events: none; }
  .none { color: var(--dim); font-size: 12.5px; margin: 0; padding: 0 12px; }
</style>
