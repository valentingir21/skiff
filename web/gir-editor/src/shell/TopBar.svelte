<script lang="ts">
  import { device } from "../lib/model/device.svelte";
  import { ID } from "../lib/model/tables";
  import { ui, ask, toastKey } from "../lib/ui.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import { ic } from "../ui/icons";

  const bpm = $derived(device.g[ID.G_BPM] ?? 120);
  const live = $derived(device.live && device.status === "ready");
  const demo = $derived(device.kind === "emulator");
  const proj = $derived(device.proj);
  const slotName = (s: number) => "ABCD"[s] ?? "?";
  let bpmEdit = $state(false);
  let bpmDraft = $state("");
  let drag: { y: number; v: number } | null = null;

  function bpmDown(e: PointerEvent) { (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId); drag = { y: e.clientY, v: bpm }; }
  function bpmMove(e: PointerEvent) { if (drag) device.setGlobal(ID.G_BPM, Math.max(40, Math.min(240, drag.v + Math.round((drag.y - e.clientY) / (e.altKey ? 8 : 3))))); }
  function bpmUp(e: PointerEvent) {
    if (drag && Math.abs(e.clientY - drag.y) < 3) { bpmDraft = String(bpm); bpmEdit = true; }
    drag = null;
  }
  function bpmCommit() { bpmEdit = false; const n = parseInt(bpmDraft, 10); if (n >= 40 && n <= 240) device.setGlobal(ID.G_BPM, n); }

  async function save() {
    const p = device.proj;
    const slot = p && p.cur >= 0 ? p.cur : Math.max(0, p?.slots.findIndex((s) => !s.used) ?? 0);
    const name = await ask({ title: t("top.save"), body: t("top.saveTip") + ` (${t("proj.slot", { s: slotName(slot) })})`, ok: t("dlg.save"),
                             input: { label: t("proj.name"), value: p?.name || "", max: 12 } });
    if (name == null) return;
    try { await device.saveProject(slot, String(name)); toastKey("ok", "toast.saved", { s: slotName(slot) }); }
    catch (e) { toastKey("err", String((e as Error).message) === "refused" ? "toast.full" : "toast.error", { detail: String((e as Error).message) }); }
  }
</script>

<header class="top">
  <div class="brand" title="Gir Editor"><span class="mark" aria-hidden="true"></span><span class="dot">GIR</span></div>

  <div class="status" title={t("status.tip")}>
    <span class="led" class:live class:demo aria-hidden="true"></span>
    <span class="st">{demo ? t("status.demo") : live ? t("status.live") : t("status." + device.status)}</span>
    <span class="port">{device.portName} · {device.tablesMatch ? "Gir 1.0" : device.info?.version ?? ""}</span>
  </div>

  <div class="transport" role="group" aria-label="Transport">
    <button class="tb" title={t("top.restart")} aria-label={t("top.restart")} onclick={() => device.restart()}>
      <svg viewBox="0 0 16 16" aria-hidden="true"><rect x="2" y="3" width="2" height="10" rx="1"/><path d="M6 8l7-5v10z"/></svg>
    </button>
    <button class="tb play" class:on={device.playing} title={t("top.play") + " (Space)"} aria-label={t("top.play")} aria-pressed={device.playing} onclick={() => device.play()}>
      <i class="ic">{ic("control_play_f")}</i>
    </button>
    <button class="tb" title={t("top.stop")} aria-label={t("top.stop")} onclick={() => device.stop()}><i class="ic">{ic("control_stop_f")}</i></button>
    <button class="tb rec" class:on={!!(device.recMask & (1 << device.sel))} title={t("top.rec")} aria-label={t("top.rec")} onclick={() => device.rec()}>
      <i class="ic">{ic("control_rec_f")}</i>
    </button>
  </div>

  <div class="bpm" title={t("top.bpm")}>
    {#if bpmEdit}
      <!-- svelte-ignore a11y_autofocus -->
      <input class="dot" bind:value={bpmDraft} autofocus onblur={bpmCommit} onkeydown={(e) => { if (e.key === "Enter") bpmCommit(); if (e.key === "Escape") bpmEdit = false; }} aria-label={t("top.bpm")} />
    {:else}
      <div class="bv dot num" role="slider" tabindex="0" aria-label={t("top.bpm")} aria-valuenow={bpm} aria-valuemin={40} aria-valuemax={240}
           onpointerdown={bpmDown} onpointermove={bpmMove} onpointerup={bpmUp}
           onwheel={(e) => { e.preventDefault(); device.setGlobal(ID.G_BPM, Math.max(40, Math.min(240, bpm + (e.deltaY < 0 ? 1 : -1)))); }}
           onkeydown={(e) => { if (e.key === "ArrowUp") device.setGlobal(ID.G_BPM, Math.min(240, bpm + 1)); if (e.key === "ArrowDown") device.setGlobal(ID.G_BPM, Math.max(40, bpm - 1)); }}>{bpm}</div>
    {/if}
    <span class="u">BPM</span>
  </div>

  <div class="proj">
    <span class="slot dot">{proj && proj.cur >= 0 ? slotName(proj.cur) : "–"}</span>
    <span class="pn">{proj?.name || (proj && proj.cur >= 0 ? t("proj.slot", { s: slotName(proj.cur) }) : t("top.noproject"))}</span>
    {#if proj?.changed}<span class="dirty" title={t("top.unsaved")} aria-label={t("top.unsaved")}></span>{/if}
  </div>

  <div class="spacer"></div>

  <div class="tools">
    <button class="tb" disabled={!device.canUndo} title={t("top.undo") + " (⌘Z)"} aria-label={t("top.undo")} onclick={() => device.undo()}>
      <svg viewBox="0 0 16 16" aria-hidden="true"><path d="M5 4L2 7l3 3M2.5 7H10a3.5 3.5 0 010 7H7" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/></svg>
    </button>
    <button class="tb" disabled={!device.canRedo} title={t("top.redo") + " (⇧⌘Z)"} aria-label={t("top.redo")} onclick={() => device.redo()}>
      <svg viewBox="0 0 16 16" aria-hidden="true"><path d="M11 4l3 3-3 3M13.5 7H6a3.5 3.5 0 000 7h3" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/></svg>
    </button>
    <button class="kbd" title={t("top.commands")} onclick={() => (ui.palette = true)}><span>{t("top.commands")}</span><kbd>⌘K</kbd></button>
    <button class="tb" title={t("top.help") + " (?)"} aria-label={t("top.help")} onclick={() => (ui.help = true)}>?</button>
    <button class="save" onclick={save} title={t("top.saveTip")}>{t("top.save")}</button>
    <button class="tb" title={t("top.disconnect")} aria-label={t("top.disconnect")} onclick={() => device.disconnect()}>
      <svg viewBox="0 0 16 16" aria-hidden="true"><path d="M6 3H3v10h3M10 5l3 3-3 3M13 8H6" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/></svg>
    </button>
  </div>
</header>

<style>
  .top { height: var(--topbar-h); flex: none; display: flex; align-items: center; gap: 18px; padding: 0 14px 0 18px; }
  .brand { display: flex; align-items: center; gap: 9px; font-size: 17px; width: calc(var(--tracks-w) - 32px); }
  .mark { width: 18px; height: 18px; border-radius: 5px; background: conic-gradient(var(--t1) 0 60deg, var(--t2) 0 120deg, var(--t3) 0 180deg, var(--t4) 0 240deg, var(--t5) 0 300deg, var(--t6) 0); }
  .status { display: flex; align-items: center; gap: 8px; font-size: 12.5px; padding: 6px 11px; border-radius: 999px; background: var(--surf-2); border: 1px solid var(--line); }
  .led { width: 8px; height: 8px; border-radius: 50%; background: var(--mute); }
  .led.live { background: var(--ok); box-shadow: 0 0 0 3px rgb(164 230 49 / 18%); }
  .led.demo { background: var(--info); box-shadow: 0 0 0 3px rgb(32 194 180 / 18%); }
  .st { font-weight: 700; }
  .port { color: var(--dim); }
  .transport { display: flex; gap: 4px; padding: 3px; background: var(--surf-2); border-radius: 11px; border: 1px solid var(--line); }
  .tb { width: 34px; height: 32px; border-radius: 8px; background: none; border: 0; color: var(--fg-2); display: grid; place-items: center; font-size: 15px; font-weight: 700; }
  .tb svg { width: 15px; height: 15px; fill: currentColor; }
  .tb:hover:not(:disabled) { background: var(--surf-4); color: var(--fg); }
  .tb:disabled { opacity: 0.3; cursor: default; }
  .play.on { background: color-mix(in srgb, var(--ok) 22%, transparent); color: var(--ok); }
  .rec.on { background: color-mix(in srgb, var(--err) 22%, transparent); color: var(--err); }
  .bpm { display: flex; align-items: baseline; gap: 6px; }
  .bv { font-size: 24px; min-width: 46px; text-align: right; cursor: ns-resize; border-radius: 6px; padding: 0 4px; }
  .bv:hover { background: var(--surf-2); }
  .bpm input { width: 56px; height: 30px; font-size: 20px; background: var(--surf); border: 1px solid var(--accent); border-radius: 6px; text-align: right; }
  .u { font-size: 11px; color: var(--dim); font-weight: 700; }
  .proj { display: flex; align-items: center; gap: 8px; min-width: 0; }
  .slot { width: 24px; height: 24px; display: grid; place-items: center; border-radius: 6px; background: var(--surf-3); font-size: 14px; }
  .pn { font-weight: 600; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; max-width: 18ch; }
  .dirty { width: 7px; height: 7px; border-radius: 50%; background: var(--warn); }
  .spacer { flex: 1; }
  .tools { display: flex; align-items: center; gap: 4px; }
  .kbd { display: flex; align-items: center; gap: 8px; height: 32px; padding: 0 6px 0 10px; border-radius: 8px; background: var(--surf-2); border: 1px solid var(--line); color: var(--dim); font-size: 12.5px; margin: 0 4px; }
  .kbd:hover { color: var(--fg); }
  .save { height: 32px; padding: 0 16px; margin-left: 6px; border-radius: 9px; border: 0; background: var(--accent); color: #0b0b0b; font-weight: 700; }
  .save:hover { filter: brightness(1.08); }
  @media (max-width: 1180px) { .kbd span, .port { display: none; } .brand { width: auto; } }
</style>
