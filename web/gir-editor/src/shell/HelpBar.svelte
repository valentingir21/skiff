<script lang="ts">
  // What the pointer is over: the parameter's role (the manual), range, default, MIDI CC, where it is on the FM-1
  import { ui } from "../lib/ui.svelte";
  import { t } from "../lib/i18n/index.svelte";
</script>
<footer class="help" aria-live="polite">
  {#if ui.hint}
    <span class="lb dot" style="color:{ui.hint.color}">{ui.hint.label}</span>
    {#if ui.hint.text}<span class="tx">{ui.hint.text}</span>{/if}
    <span class="meta">
      <span>{t("param.range")} <b>{ui.hint.range}</b></span>
      <span>{t("param.default")} <b>{ui.hint.def}</b></span>
      {#if ui.hint.cc != null}<span><b>{t("param.cc", { cc: ui.hint.cc })}</b></span>{/if}
      {#if ui.hint.where}<span class="where">{ui.hint.where}</span>{/if}
    </span>
  {:else}
    <span class="idle">{t("param.reset")} · {t("param.fine")} · {t("param.type")}</span>
  {/if}
</footer>
<style>
  .help { flex: none; min-height: 40px; display: flex; align-items: center; gap: 14px; padding: 8px 20px; border-top: 1px solid var(--line); font-size: 12.5px; background: var(--bg-2); border-bottom-left-radius: 0; }
  .lb { font-size: 15px; white-space: nowrap; }
  .tx { color: var(--fg-2); flex: 1; min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
  .meta { display: flex; gap: 14px; color: var(--dim); white-space: nowrap; margin-left: auto; }
  .meta b { color: var(--fg-2); font-weight: 600; }
  .where { color: var(--fg); background: var(--surf-3); border-radius: 6px; padding: 2px 8px; font-weight: 600; }
  .idle { color: var(--mute); }
</style>
