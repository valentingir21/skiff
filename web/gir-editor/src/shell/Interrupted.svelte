<script lang="ts">
  // (rc2) a write to the FM-1's flash (a restore, a project save, a sample, a user sound) that the unit went away in
  // the middle of: what it means for the unit, and the same write again once it is back (Resume)
  import { device } from "../lib/model/device.svelte";
  import { t } from "../lib/i18n/index.svelte";
  let busy = $state(false);
  const it = $derived(device.interrupted);
  async function resume() {
    if (!it?.resume) return;
    busy = true;
    try { await it.resume(); if (device.interrupted === it) device.interrupted = null; } catch { /* said by the write */ }
    finally { busy = false; }
  }
</script>

{#if it}
  <div class="int" role="alert" data-testid="interrupted">
    <span class="msg">{t("write.banner." + it.what)}{#if !device.ready} {t("write.waiting")}{/if}</span>
    {#if it.resume}
      <button class="go" disabled={!device.ready || busy} onclick={resume}>{it.what === "restore" ? t("write.resumeRestore") : t("write.resume")}</button>
    {/if}
    <button class="x" onclick={() => (device.interrupted = null)}>{t("write.dismiss")}</button>
  </div>
{/if}

<style>
  .int { position: fixed; bottom: 48px; left: 50%; transform: translateX(-50%); z-index: 81; display: flex; align-items: center; gap: 10px; max-width: min(760px, 92vw);
         padding: 10px 12px 10px 16px; background: color-mix(in srgb, var(--err) 16%, var(--surf-2)); border: 1px solid color-mix(in srgb, var(--err) 45%, transparent);
         border-radius: var(--r-l); box-shadow: var(--shadow-pop); line-height: 1.4; }
  .msg { flex: 1; }
  button { border: 0; border-radius: 6px; padding: 6px 11px; font-weight: 650; white-space: nowrap; }
  .go { background: var(--fg); color: var(--bg); }
  .go:disabled { opacity: 0.45; }
  .x { background: transparent; color: var(--fg-2); }
</style>
