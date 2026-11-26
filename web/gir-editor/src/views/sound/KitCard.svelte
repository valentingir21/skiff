<script lang="ts">
  // EDIT › SOUND on a DRUM-X track: the project's kit, 8 sounds, each a patch A and a patch B (MORPH moves between
  // them), read with KIT_GET and written with KIT_PUT (the device pushes nothing: read again after editing there)
  import { onMount } from "svelte";
  import { device } from "../../lib/model/device.svelte";
  import { req, DX } from "../../lib/legacy/proto.js";
  import Knob from "../../ui/components/Knob.svelte";
  import Seg from "../../ui/components/Seg.svelte";
  import { t } from "../../lib/i18n/index.svelte";
  interface Lane { lane: number; wave: number; filt: number; snap: boolean; pmod: number; drive: boolean; a: number[]; b: number[] }
  interface Props { color: string }
  let { color }: Props = $props();
  let kit = $state<Lane[]>([]);
  let lane = $state(0);
  async function read() { const out: Lane[] = []; for (let l = 0; l < 8; l++) out.push(await device.ask<Lane>(req.kitGet(l))); kit = out; }
  onMount(() => { if (device.info?.kitLanes) read().catch(() => {}); });
  async function put(l: Lane) { try { const r = await device.ask<Lane>(req.kitPut(l), { key: "kit:" + l.lane }); kit[l.lane] = r; } catch { /* toast via model */ } }
  const cur = $derived(kit[lane]);
</script>
<article class="card" style="--c:{color}">
  <header><h3 class="dot">SOUND</h3><span class="sub">{t("sound.kit")}</span>
    <button class="re" onclick={read}>{t("seq.reload")}</button></header>
  {#if cur}
    <div class="lanes" role="tablist">{#each DX.lanes as n, l}<button role="tab" aria-selected={l === lane} class:on={l === lane} class="dot" onclick={() => (lane = l)}>{n}</button>{/each}</div>
    <div class="opts">
      <Seg size="s" label="WAVE" options={DX.waves.map((w: string, i: number) => ({ value: i, label: w }))} value={cur.wave} onchange={(v) => put({ ...cur, wave: v })} {color} />
      <Seg size="s" label="FILTER" options={DX.filters.map((w: string, i: number) => ({ value: i, label: w }))} value={cur.filt} onchange={(v) => put({ ...cur, filt: v })} {color} />
      <Seg size="s" label="P MOD" options={DX.shapes.map((w: string, i: number) => ({ value: i, label: w }))} value={cur.pmod} onchange={(v) => put({ ...cur, pmod: v })} {color} />
      <Seg size="s" label="SNAP" options={[{ value: 0, label: "SNAP OFF" }, { value: 1, label: "SNAP ON" }]} value={+cur.snap} onchange={(v) => put({ ...cur, snap: !!v })} {color} />
      <Seg size="s" label="DRIVE" options={[{ value: 0, label: "DRIVE OFF" }, { value: 1, label: "DRIVE ON" }]} value={+cur.drive} onchange={(v) => put({ ...cur, drive: !!v })} {color} />
    </div>
    {#each ["a", "b"] as side}
      <div class="ab"><span class="sd dot">{side.toUpperCase()}</span>
        {#each DX.values as nm, i}
          <Knob value={(cur as any)[side][i]} min={0} max={127} def={64} label={nm} text={String((cur as any)[side][i])} {color} size={38}
                onchange={(v) => { const x = { ...cur, a: [...cur.a], b: [...cur.b] }; (x as any)[side][i] = v; kit[lane] = x; put(x); }} />
        {/each}
      </div>
    {/each}
  {:else}
    <p class="muted">…</p>
  {/if}
</article>
<style>
  .card { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 12px; display: flex; flex-direction: column; gap: 10px; grid-column: span 2; }
  header { display: flex; align-items: baseline; gap: 10px; }
  h3 { margin: 0; font-size: 15px; font-weight: 400; }
  .sub { color: var(--dim); font-size: 12px; flex: 1; }
  .re { background: none; border: 0; color: var(--dim); font-size: 12px; }
  .re:hover { color: var(--fg); }
  .lanes { display: flex; flex-wrap: wrap; gap: 4px; }
  .lanes button { background: var(--surf-2); border: 1px solid var(--line-2); border-radius: 6px; padding: 3px 8px; font-size: 13px; color: var(--dim); }
  .lanes button.on { color: #0b0b0b; background: var(--c); border-color: var(--c); }
  .opts { display: flex; flex-wrap: wrap; gap: 6px; }
  .ab { display: flex; align-items: center; gap: 2px; }
  .sd { width: 22px; color: var(--c); font-size: 16px; }
  .muted { color: var(--dim); }
</style>
