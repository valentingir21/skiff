<script lang="ts">
  // EDIT › LANES (DRUM: each lane's LEVEL, TUNE, DECAY) and LANE MIX (DRUM, DRUM-X: LEVEL, PAN), every lane at once
  // instead of the device's lane knob
  import Param from "../../ui/Param.svelte";
  import { ID, pathOf, T, type Page } from "../../lib/model/tables";
  import { device } from "../../lib/model/device.svelte";
  import { t } from "../../lib/i18n/index.svelte";
  interface Props { k: number; color: string }
  let { k, color }: Props = $props();
  const e = $derived(device.tracks[k]?.engine ?? 0);
  const drum = $derived(device.engineName(e) === "DRUM");
  const lanes = ["KICK", "SNARE", "CLAP", "CL HAT", "OP HAT", "TOM", "RIM", "BELL"];
  const pg = (title: string) => T.PAGES.find((p) => p.title === title)!;
  const where = (title: string, slot: number) => pathOf(pg(title), slot, e) + " (" + t("sound.lane") + ")";
</script>
<article class="card" style="--c:{color}">
  <header><h3 class="dot">{drum ? "LANES · LANE MIX" : "LANE MIX"}</h3></header>
  <div class="grid" style="--cols:{drum ? 4 : 2}">
    <span></span>
    <span class="h">{t("sound.level")}</span>
    {#if drum}<span class="h">{t("sound.tune")}</span><span class="h">{t("sound.decay")}</span>{/if}
    <span class="h">{t("sound.pan")}</span>
    {#each lanes as ln, l}
      <span class="ln dot">{ln}</span>
      <Param track={k} id={ID.P_LN0 + l} {color} size={34} label={`${ln} ${t("sound.level")}`} hideLabel layout="row" where={where(drum ? "LANES" : "LANE MIX", 1)} />
      {#if drum}
        <Param track={k} id={ID.P_LTUNE0 + l} {color} size={34} label={`${ln} ${t("sound.tune")}`} hideLabel layout="row" where={where("LANES", 2)} />
        <Param track={k} id={ID.P_LDECAY0 + l} {color} size={34} label={`${ln} ${t("sound.decay")}`} hideLabel layout="row" where={where("LANES", 3)} />
      {/if}
      <Param track={k} id={ID.P_LPAN0 + l} {color} size={34} label={`${ln} ${t("sound.pan")}`} hideLabel layout="row" where={where("LANE MIX", 2)} />
    {/each}
  </div>
</article>
<style>
  .card { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 12px; display: flex; flex-direction: column; gap: 8px; }
  h3 { margin: 0; font-size: 15px; font-weight: 400; }
  .grid { display: grid; grid-template-columns: 70px repeat(var(--cols), minmax(0, 1fr)); align-items: center; justify-items: start; row-gap: 8px; column-gap: 8px; }
  .h { font-size: 11px; font-weight: 700; color: var(--dim); padding-left: 4px; }
  .ln { justify-self: start; font-size: 13px; color: var(--fg-2); }

</style>
