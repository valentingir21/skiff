<script lang="ts">
  import { ui } from "../../lib/ui.svelte";
  import { t } from "../../lib/i18n/index.svelte";
  import Button from "./Button.svelte";
  let text = $state("");
  let input: HTMLInputElement | undefined = $state();
  let okBtn: HTMLElement | undefined = $state();
  $effect(() => {
    const c = ui.confirm;
    if (!c) return;
    text = c.input?.value ?? "";
    queueMicrotask(() => { if (input) { input.focus(); input.select(); } else (document.querySelector(".dlg .primary, .dlg .danger") as HTMLElement | null)?.focus(); });
  });
  function done(ok: boolean) { const c = ui.confirm; if (!c) return; c.resolve(ok ? (c.input ? text : true) : null); }
</script>
{#if ui.confirm}
  {@const c = ui.confirm}
  <div class="scrim" role="presentation" onclick={() => done(false)}></div>
  <div class="dlg" role="alertdialog" aria-modal="true" aria-labelledby="dlg-t" aria-describedby="dlg-b" tabindex="-1"
       onkeydown={(e) => { if (e.key === "Escape") done(false); if (e.key === "Enter" && !(e.target instanceof HTMLButtonElement)) done(true); }}>
    <h2 id="dlg-t">{c.title}</h2>
    <p id="dlg-b">{c.body}</p>
    {#if c.input}
      <label class="field"><span>{c.input.label}</span>
        <input bind:this={input} bind:value={text} maxlength={c.input.max ?? 12} spellcheck="false" />
      </label>
    {/if}
    <div class="row">
      <Button variant="ghost" onclick={() => done(false)}>{t("dlg.cancel")}</Button>
      <Button variant={c.danger ? "danger" : "primary"} onclick={() => done(true)}>{c.ok}</Button>
    </div>
  </div>
{/if}
<style>
  .scrim { position: fixed; inset: 0; background: rgb(0 0 0 / 55%); z-index: 90; animation: f var(--t-med); }
  @keyframes f { from { opacity: 0; } }
  .dlg { position: fixed; top: 22%; left: 50%; transform: translateX(-50%); z-index: 91; width: min(460px, 92vw); background: var(--surf-2);
         border-radius: var(--r-xl); box-shadow: var(--shadow-pop); padding: 22px 22px 18px; animation: p var(--t-med) var(--ease); }
  @keyframes p { from { opacity: 0; transform: translate(-50%, 6px); } }
  h2 { margin: 0 0 8px; font-size: var(--fs-xl); font-weight: 700; }
  p { margin: 0 0 16px; color: var(--fg-2); line-height: 1.5; }
  .field { display: flex; flex-direction: column; gap: 6px; margin-bottom: 16px; font-size: 12px; color: var(--dim); }
  input { height: 38px; border-radius: var(--r-m); border: 1px solid var(--line-2); background: var(--bg); padding: 0 12px; font-family: var(--font-dot); font-size: 16px; text-transform: uppercase; }
  input:focus { border-color: var(--accent); }
  .row { display: flex; justify-content: flex-end; gap: 8px; }
</style>
