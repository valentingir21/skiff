<script lang="ts">
  // LFO › MOD: the seven fixed routes (LFO to PITCH CUT SHP AMP, ENV to PITCH CUT SHP: only their amount) and the
  // matrix's four slots (SOURCE, DEST, AMOUNT), all at once instead of the device's route knob.
  // (rc3) a real matrix: the targets in columns, one row for LFO →, one for ENV →, their labels on the knobs' middle;
  // the four slots side by side across the card's whole width.
  import Param from "../../ui/Param.svelte";
  import { pathOf, type Page } from "../../lib/model/tables";
  import { device } from "../../lib/model/device.svelte";
  import { t } from "../../lib/i18n/index.svelte";
  interface Props { page: Page; k: number; color: string }
  let { page, k, color }: Props = $props();
  const e = $derived(device.tracks[k]?.engine ?? 0);
  const w = (slot: number) => pathOf(page, slot, e);
  const TARGETS = ["PITCH", "CUT", "SHP", "AMP"];
  const ROUTES: [string, (number | null)[]][] = [["LFO →", [13, 14, 15, 16]], ["ENV →", [6, 5, 7, null]]];
</script>
<article class="card" style="--c:{color}">
  <header><h3 class="dot">MOD</h3></header>
  <div class="mtop">
    <div class="routes" role="table" aria-label="MOD routes">
      <span></span>
      {#each TARGETS as tg}<span class="th">{tg}</span>{/each}
      {#each ROUTES as [src, ids]}
        <span class="rl" data-row-label>{src}</span>
        {#each ids as id, i}
          {#if id != null}<Param track={k} {id} {color} where={w(3)} label={`${src} ${TARGETS[i]}`} hideLabel />
          {:else}<span></span>{/if}
        {/each}
      {/each}
    </div>
    <p class="note">{t("sound.modHelp")}</p>
  </div>
  <div class="matrix" data-lint="chips">
    {#each [0, 1, 2, 3] as s}
      <div class="slot" role="group" aria-label={`MOD ${s + 1}`}>
        <span class="sn dot">{s + 1}</span>
        <Param track={k} id={49 + 3 * s} {color} size={34} where={w(1)} label="SRC" />
        <Param track={k} id={50 + 3 * s} {color} size={34} where={w(2)} label="DEST" />
        <Param track={k} id={51 + 3 * s} {color} size={34} where={w(3)} label="AMT" />
      </div>
    {/each}
  </div>
</article>
<style>
  .card { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 12px; display: flex; flex-direction: column; gap: 12px; container-type: inline-size; }
  h3 { margin: 0; font-size: 15px; font-weight: 400; }
  .mtop { display: flex; gap: 24px; align-items: flex-start; flex-wrap: wrap; }
  .routes { display: grid; grid-template-columns: 56px repeat(4, 80px); justify-items: center; align-items: start; row-gap: 10px; }
  .th { font-size: 10.5px; font-weight: 700; letter-spacing: 0.03em; color: var(--dim); }
  .rl { justify-self: start; height: 48px; display: flex; align-items: center; font-size: 12px; font-weight: 700; color: var(--fg-2); white-space: nowrap; }
  .note { flex: 1; min-width: 200px; margin: 18px 0 0; padding-left: 18px; border-left: 1px solid var(--line); font-size: 12px; line-height: 1.45; color: var(--dim); }
  /* the four slots on one row, or two rows of two: never one alone */
  .matrix { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 8px; border-top: 1px solid var(--line); padding-top: 12px; }
  .slot { display: flex; align-items: flex-start; justify-content: space-around; background: var(--surf-2); border-radius: var(--r-m); padding: 8px 6px; }
  .sn { width: 18px; height: 34px; display: flex; align-items: center; justify-content: center; color: var(--dim); font-size: 13px; flex: none; }
  @container (max-width: 940px) { .matrix { grid-template-columns: repeat(2, minmax(0, 1fr)); } }
</style>
