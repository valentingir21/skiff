<script lang="ts">
  import { ui, go, VIEWS } from "../lib/ui.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import { ic, VIEW_ICON } from "../ui/icons";
  import { device } from "../lib/model/device.svelte";
  import { isNarrow, isTiny } from "../lib/layout.svelte";
  const sel = $derived(device.sel);
  // (rc3) the labels kept as long as possible: full, then short (< 1180 px), then short without the icons (< 900 px);
  // each tab named for screen readers and tests whatever is shown
  // each length tried in turn, the longest that fits the bar kept (French is longer: it may need the short labels
  // sooner than the breakpoint)
  import { tick } from "svelte";
  import { win } from "../lib/layout.svelte";
  import { i18n } from "../lib/i18n/index.svelte";
  let level = $state(0);                       // 0 full, 1 short, 2 short without icons
  let bar: HTMLDivElement | undefined = $state();
  const short = $derived(level >= 1 || isNarrow());
  const tiny = $derived(level >= 2 || isTiny());
  async function fit() {
    if (!bar) return;
    level = 0; await tick();
    while (level < 2 && bar.scrollWidth > bar.clientWidth + 1) { level++; await tick(); }
  }
  $effect(() => { void win.w; void i18n.lang; void bar; fit(); });
</script>
<div class="tabs" role="tablist" aria-label="Views" bind:this={bar}>
  {#each VIEWS as v, i}
    <button role="tab" aria-selected={ui.view === v} class:on={ui.view === v} onclick={() => go(v)} title={`${t("view." + v)} (Alt+${i + 1})`} aria-label={t("view." + v)}>
      {#if !tiny}<i class="ic" aria-hidden="true">{ic(VIEW_ICON[v])}</i>{/if}<span aria-hidden="true">{short ? t("view.short." + v) : t("view." + v)}</span>
    </button>
  {/each}
  <span class="trk dot" style="color:var(--t{sel + 1})">T{sel + 1}</span>
</div>
<style>
  .tabs { display: flex; align-items: center; gap: 2px; padding: 8px 14px 0; border-bottom: 1px solid var(--line); flex: none; overflow-x: auto; scrollbar-width: none; }
  button { position: relative; display: flex; align-items: center; gap: 7px; height: 38px; padding: 0 12px; background: none; border: 0; color: var(--dim); font-weight: 600; font-size: 13px; white-space: nowrap; border-radius: 8px 8px 0 0; }
  button:hover { color: var(--fg); }
  button.on { color: var(--fg); }
  button.on::after { content: ""; position: absolute; left: 10px; right: 10px; bottom: -1px; height: 2px; border-radius: 2px; background: var(--accent); }
  .ic { font-size: 15px; opacity: 0.85; }
  .trk { margin-left: auto; font-size: 15px; padding-right: 6px; }
  @media (max-width: 1279px) { .trk { display: none; } }
  @media (max-width: 1179px) { button { padding: 0 9px; gap: 5px; } .tabs { padding: 8px 8px 0; } }
</style>
