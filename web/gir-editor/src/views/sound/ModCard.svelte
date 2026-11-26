<script lang="ts">
  // LFO › MOD: the seven fixed routes (LFO to PITCH CUT SHP AMP, ENV to PITCH CUT SHP: only their amount) and the
  // matrix's four slots (SOURCE, DEST, AMOUNT), all at once instead of the device's route knob
  import Param from "../../ui/Param.svelte";
  import { pathOf, type Page } from "../../lib/model/tables";
  import { device } from "../../lib/model/device.svelte";
  interface Props { page: Page; k: number; color: string }
  let { page, k, color }: Props = $props();
  const e = $derived(device.tracks[k]?.engine ?? 0);
  const w = (slot: number) => pathOf(page, slot, e);
</script>
<article class="card" style="--c:{color}">
  <header><h3 class="dot">MOD</h3></header>
  <div class="routes">
    <div class="rl">LFO →</div>
    <div class="row">{#each [13, 14, 15, 16] as id}<Param track={k} {id} {color} size={40} where={w(3)} label={["PITCH", "CUT", "SHP", "AMP"][[13, 14, 15, 16].indexOf(id)]} />{/each}</div>
    <div class="rl">ENV →</div>
    <div class="row">{#each [6, 5, 7] as id, i}<Param track={k} {id} {color} size={40} where={w(3)} label={["PITCH", "CUT", "SHP"][i]} />{/each}</div>
  </div>
  <div class="matrix">
    {#each [0, 1, 2, 3] as s}
      <div class="slot"><span class="sn dot">{s + 1}</span>
        <Param track={k} id={49 + 3 * s} {color} size={36} where={w(1)} />
        <Param track={k} id={50 + 3 * s} {color} size={36} where={w(2)} />
        <Param track={k} id={51 + 3 * s} {color} size={36} where={w(3)} />
      </div>
    {/each}
  </div>
</article>
<style>
  .card { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 12px; display: flex; flex-direction: column; gap: 10px; grid-column: span 2; }
  h3 { margin: 0; font-size: 15px; font-weight: 400; }
  .routes { display: grid; grid-template-columns: 52px 1fr; align-items: center; row-gap: 4px; }
  .rl { font-size: 12px; font-weight: 700; color: var(--dim); }
  .row { display: flex; gap: 2px; }
  .matrix { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; border-top: 1px solid var(--line); padding-top: 10px; }
  .slot { display: flex; align-items: center; gap: 0; background: var(--surf-2); border-radius: var(--r-m); padding: 6px 4px; }
  .sn { width: 18px; text-align: center; color: var(--dim); font-size: 13px; }
</style>
