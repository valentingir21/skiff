<script lang="ts">
  // The four project slots (A..D) with their names, and the full backup / restore of the FM-1 in one file, the same
  // format as the original editor's (Skiff-backup-DATE.json, web/fm1backup.js). Every flash write asks first; a
  // restore always downloads a backup of the FM-1 as it is before it writes anything.
  import { onMount } from "svelte";
  import { device } from "../lib/model/device.svelte";
  import { ask, toast, toastKey } from "../lib/ui.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import { backup, restore, backupState } from "../lib/backup.svelte";
  import Button from "../ui/components/Button.svelte";
  import { ic } from "../ui/icons";

  onMount(() => { device.readProj().catch(() => {}); });
  const S = "ABCD";
  const p = $derived(device.proj);
  const busy = $derived(backupState.busy);

  async function load(s: number) {
    if (!(await ask({ title: t("proj.load") + " " + S[s], body: t("proj.loadConfirm", { s: S[s] }), ok: t("dlg.load") }))) return;
    try { await device.loadProject(s); toastKey("ok", "toast.loaded", { s: S[s] }); } catch (e) { toast("err", String((e as Error).message)); }
  }
  async function save(s: number) {
    const slot = p?.slots[s];
    if (slot?.used && !(await ask({ title: t("proj.save") + " " + S[s], body: t("proj.saveConfirm", { s: S[s], name: slot.name || S[s] }), ok: t("dlg.save"), danger: true }))) return;
    const name = await ask({ title: t("proj.saveAs") + " " + S[s], body: t("top.saveTip"), ok: t("dlg.save"), input: { label: t("proj.name"), value: p?.name || slot?.name || "", max: 12 } });
    if (name == null) return;
    try { await device.saveProject(s, String(name)); toastKey("ok", "toast.saved", { s: S[s] }); } catch (e) { toastKey("err", "toast.error", { detail: String((e as Error).message) }); }
  }
  async function rename(s: number) {
    const name = await ask({ title: t("proj.rename") + " " + S[s], body: "", ok: t("proj.rename"), input: { label: t("proj.name"), value: p?.slots[s].name || "", max: 12 } });
    if (name == null) return;
    try { await device.renameProject(s, String(name)); toastKey("ok", "toast.renamed"); } catch (e) { toastKey("err", "toast.error", { detail: String((e as Error).message) }); }
  }
</script>

<div class="proj">
  <section>
    <h2>{t("proj.title")}</h2>
    <div class="slots">
      {#each [0, 1, 2, 3] as s}
        {@const sl = p?.slots[s]}
        <article class="slot" class:cur={p?.cur === s} class:empty={!sl?.used}>
          <div class="top"><span class="l dot">{S[s]}</span>{#if p?.cur === s}<span class="now">{t("proj.now")}{p.changed ? " ·" : ""}{#if p.changed}<i class="dirty" title={t("top.unsaved")}></i>{/if}</span>{/if}</div>
          <div class="nm dot">{sl?.used ? sl.name || "—" : t("proj.empty")}</div>
          <div class="act">
            <Button size="s" disabled={!sl?.used || !!busy} onclick={() => load(s)}>{t("proj.load")}</Button>
            <Button size="s" disabled={!!busy} onclick={() => save(s)}>{t("proj.save")}</Button>
            <Button size="s" variant="ghost" disabled={!sl?.used || !!busy} onclick={() => rename(s)}>{t("proj.rename")}</Button>
          </div>
        </article>
      {/each}
    </div>
  </section>

  <section class="bk">
    <article>
      <h3><i class="ic">{ic("symbol_download")}</i>{t("proj.backup")}</h3>
      <p>{t("proj.backupWhat")}</p>
      <Button variant="primary" disabled={!!busy || !device.info?.backupCaps} onclick={() => backup()}>{t("proj.backupBtn")}</Button>
    </article>
    <article>
      <h3><i class="ic">{ic("symbol_upload")}</i>{t("proj.restore")}</h3>
      <p>{t("proj.restoreWhat")}</p>
      <Button variant="danger" disabled={!!busy || !(device.info && device.info.backupCaps & 2)} onclick={restore}>{t("proj.restoreBtn")}</Button>
    </article>
  </section>
  {#if busy}
    <div class="prog" role="progressbar" aria-valuenow={busy.pct} aria-valuemin={0} aria-valuemax={100}>
      <span>{t(busy.what, { pct: busy.pct })}</span><div class="b"><div style="width:{busy.pct}%"></div></div>
    </div>
  {/if}
</div>

<style>
  .proj { display: flex; flex-direction: column; gap: 28px; max-width: 1100px; }
  h2 { margin: 0 0 12px; font-size: 13px; color: var(--fg-2); }
  .slots { display: grid; grid-template-columns: repeat(4, 1fr); gap: 10px; }
  .slot { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 14px; display: flex; flex-direction: column; gap: 10px; }
  .slot.cur { border-color: var(--accent); box-shadow: inset 0 0 0 1px var(--accent); }
  .top { display: flex; align-items: center; justify-content: space-between; }
  .l { width: 32px; height: 32px; display: grid; place-items: center; border-radius: 8px; background: var(--surf-3); font-size: 18px; }
  .cur .l { background: var(--accent); color: #0b0b0b; }
  .now { font-size: 11.5px; font-weight: 700; color: var(--accent); display: flex; align-items: center; gap: 6px; }
  .dirty { width: 7px; height: 7px; border-radius: 50%; background: var(--warn); display: inline-block; }
  .nm { font-size: 20px; min-height: 26px; }
  .empty .nm { color: var(--mute); }
  .act { display: flex; gap: 6px; flex-wrap: wrap; }
  .bk { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
  .bk article { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 18px; display: flex; flex-direction: column; gap: 10px; align-items: flex-start; }
  h3 { margin: 0; font-size: 16px; display: flex; gap: 8px; align-items: center; }
  .bk p { margin: 0; color: var(--fg-2); line-height: 1.5; flex: 1; }
  .prog { display: flex; flex-direction: column; gap: 6px; }
  .b { height: 6px; background: var(--surf-3); border-radius: 4px; overflow: hidden; }
  .b div { height: 100%; background: var(--accent); transition: width var(--t-med); }
  @media (max-width: 1000px) { .slots { grid-template-columns: 1fr 1fr; } .bk { grid-template-columns: 1fr; } }
</style>
