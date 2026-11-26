<script lang="ts">
  import { ui } from "../lib/ui.svelte";
  import { t } from "../lib/i18n/index.svelte";
  const groups: [string, [string, string][]][] = [
    ["help.general", [["⌘ K", "help.k.palette"], ["?", "help.k.help"], ["Esc", "help.k.close"]]],
    ["help.edit", [["⌘ Z", "help.k.undo"], ["⇧ ⌘ Z", "help.k.redo"], ["Alt", "help.k.fine"], ["2×", "help.k.reset"], ["↑ ↓", "help.k.presets"]]],
    ["help.transport", [["Space", "help.k.play"], ["1 … 6", "help.k.tracks"]]],
    ["help.views", [["Alt 1 … 9", "help.k.views"]]],
  ];
</script>
{#if ui.help}
  <div class="scrim" role="presentation" onclick={() => (ui.help = false)}></div>
  <div class="sh" role="dialog" aria-modal="true" aria-labelledby="sh-t">
    <h2 id="sh-t">{t("help.title")}</h2>
    <div class="cols">
      {#each groups as [g, rows]}
        <section><h3>{t(g)}</h3>
          {#each rows as [k, d]}<div class="r"><span>{t(d)}</span><kbd>{k}</kbd></div>{/each}
        </section>
      {/each}
    </div>
    <p class="note">Ctrl replaces ⌘ on Windows.</p>
  </div>
{/if}
<style>
  .scrim { position: fixed; inset: 0; background: rgb(0 0 0 / 50%); z-index: 95; }
  .sh { position: fixed; top: 14%; left: 50%; transform: translateX(-50%); width: min(720px, 94vw); z-index: 96; background: var(--surf-2); border-radius: var(--r-xl); box-shadow: var(--shadow-pop); padding: 24px; }
  h2 { margin: 0 0 18px; font-size: var(--fs-xl); }
  .cols { display: grid; grid-template-columns: 1fr 1fr; gap: 20px 32px; }
  h3 { margin: 0 0 8px; font-size: 12px; color: var(--dim); font-weight: 700; }
  .r { display: flex; justify-content: space-between; align-items: center; padding: 6px 0; border-bottom: 1px solid var(--line); }
  .note { margin: 16px 0 0; color: var(--mute); font-size: 12px; }
</style>
