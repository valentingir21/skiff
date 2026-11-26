<script lang="ts">
  import { ui, go, VIEWS } from "../lib/ui.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import { ic, VIEW_ICON } from "../ui/icons";
  import { device } from "../lib/model/device.svelte";
  const sel = $derived(device.sel);
</script>
<div class="tabs" role="tablist" aria-label="Views">
  {#each VIEWS as v, i}
    <button role="tab" aria-selected={ui.view === v} class:on={ui.view === v} onclick={() => go(v)} title={`${t("view." + v)} (Alt+${i + 1})`}>
      <i class="ic" aria-hidden="true">{ic(VIEW_ICON[v])}</i><span>{t("view." + v)}</span>
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
  @media (max-width: 1240px) { button span { display: none; } button { padding: 0 11px; } .ic { font-size: 17px; } }
</style>
