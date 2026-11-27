<script lang="ts">
  // The 15 engines in the EDIT layer's order (F3 .. F5), each with its key, a line on what it is, its voices; Gir's
  // marked. A light track (T5, T6) can't play GRAIN or PHYS: those are shown, greyed, with why.
  import { device } from "../../lib/model/device.svelte";
  import { EDIT_KEYS, GIR_ENGINES, HELP, helpText, shownEngines, T } from "../../lib/model/tables";
  import { t } from "../../lib/i18n/index.svelte";
  interface Props { k: number; onclose: () => void; color: string }
  let { k, onclose, color }: Props = $props();
  const cur = $derived(device.tracks[k]?.engine);
  const light = (e: number) => !!device.info && !!(device.info.light.tracks & (1 << k)) && !!(device.info.light.engines & (1 << e));
  async function pick(e: number) { if (light(e)) return; onclose(); if (e !== cur) await device.setEngine(k, e); }
</script>
<div class="scrim" role="presentation" onclick={onclose}></div>
<div class="picker" role="dialog" aria-modal="true" aria-label={t("sound.changeEngine")} style="--c:{color}"
     onkeydown={(e) => e.key === "Escape" && onclose()} tabindex="-1">
  <header><h2>{t("sound.changeEngine")} · T{k + 1}</h2><button class="x" onclick={onclose} aria-label={t("dlg.close")}>×</button></header>
  <div class="grid">
    {#each shownEngines() as e, i}
      {@const name = device.engineName(e)}
      {@const h = HELP.engines[name]}
      <button class="eng" class:cur={e === cur} class:off={light(e)} disabled={light(e)} onclick={() => pick(e)}
              title={light(e) ? t("sound.lightTrack") : helpText(h?.principle)}>
        <span class="key dot">{EDIT_KEYS[i]}</span>
        <span class="nm dot">{name}</span>
        {#if GIR_ENGINES.has(name)}<span class="gir">Gir</span>{/if}
        <span class="pr">{helpText(h?.principle)}</span>
        <span class="meta">{T.ENG[e].presets.length} presets · {t("sound.voices", { n: T.ENG[e].oneshot ? 8 : T.ENG[e].poly })}</span>
      </button>
    {/each}
  </div>
</div>
<style>
  .scrim { position: fixed; inset: 0; background: rgb(0 0 0 / 50%); z-index: 70; }
  .picker { position: fixed; top: 8%; left: 50%; transform: translateX(-50%); width: min(1060px, 94vw); max-height: 84vh; overflow: auto; z-index: 71;
            background: var(--surf-2); border-radius: var(--r-xl); box-shadow: var(--shadow-pop); padding: 20px; animation: p var(--t-med) var(--ease); }
  @keyframes p { from { opacity: 0; transform: translate(-50%, 6px); } }
  header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 14px; }
  h2 { margin: 0; font-size: var(--fs-xl); }
  .x { background: none; border: 0; color: var(--dim); font-size: 24px; }
  .grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(190px, 1fr)); gap: 8px; }
  .eng { position: relative; text-align: left; display: flex; flex-direction: column; gap: 4px; padding: 12px; border-radius: var(--r-l); background: var(--surf); border: 1px solid var(--line); min-height: 112px; transition: border-color var(--t-fast), background var(--t-fast); }
  .eng:hover:not(:disabled) { border-color: var(--c); background: var(--surf-3); }
  .eng.cur { border-color: var(--c); box-shadow: inset 0 0 0 1px var(--c); }
  .eng.off { opacity: 0.35; cursor: not-allowed; }
  .key { position: absolute; top: 10px; right: 10px; font-size: 12px; color: var(--dim); }
  .nm { font-size: 19px; color: var(--fg); }
  .eng.cur .nm { color: var(--c); }
  .gir { align-self: flex-start; font-size: 10px; font-weight: 700; color: var(--c); border: 1px solid color-mix(in srgb, var(--c) 50%, transparent); border-radius: 4px; padding: 0 5px; }
  .pr { font-size: 12px; color: var(--fg-2); line-height: 1.35; flex: 1; display: -webkit-box; -webkit-line-clamp: 2; line-clamp: 2; -webkit-box-orient: vertical; overflow: hidden; }
  .meta { font-size: 11px; color: var(--dim); }
</style>
