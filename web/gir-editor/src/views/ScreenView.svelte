<script lang="ts">
  // The FM-1's panel, as Try Gir draws it (web/panel.js), working the unit: its buttons and keys held while pressed
  // (mouse, finger or the computer's keyboard), its knobs turned by dragging, the wheel or the arrows.
  // The screen: in demo mode the virtual FM-1's own; on a real unit the live copy (cast.svelte.ts: the original
  // editor's mechanism, a backup, the unit's restart, its screen redrawn by the firmware itself in the page).
  import { onDestroy, onMount } from "svelte";
  import { device } from "../lib/model/device.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import { ask } from "../lib/ui.svelte";
  import { cast, canCast, castScreen, startCast, stopCast } from "../lib/cast.svelte";
  import "../../../panel.js";
  import Button from "../ui/components/Button.svelte";

  const PANEL = (window as any).PANEL;
  let svg: SVGSVGElement;
  let canvas: HTMLCanvasElement | undefined = $state();
  let wrap: HTMLDivElement;
  let geo: any = $state(null);
  const held = { btn: new Map<string, number>(), key: new Map<string, number>() };
  let sentB = -1, sentK = -1;
  let leds = $state<number[]>([0, 0]);
  const emu = $derived(device.emu);
  const casting = $derived(cast.state === "on");
  const live = $derived(!!emu || casting);
  let off: (() => void) | null = null;
  let isFull = $state(false);

  function sync() {
    let b = 0, k = 0;
    for (const x of held.btn.values()) b |= 1 << x;
    for (const x of held.key.values()) k |= 1 << x;
    if (geo) {
      for (const x of geo.buttons) x.g.classList.toggle("held", !!((b >> x.bit) & 1));
      geo.keys.forEach((x: any, i: number) => x?.g.classList.toggle("held", !!((k >> i) & 1)));
    }
    if (b === sentB && k === sentK) return;
    sentB = b; sentK = k;
    if (emu) { emu.panel({ type: "buttons", mask: b }); emu.panel({ type: "keys", mask: k }); }
    else device.panel(b, k).catch(() => {});
  }
  function turn(role: number, n: number) {
    if (emu) emu.panel({ type: "enc", role, n });
    else device.panelTurn(role, n).catch(() => {});
  }
  function draw565(fb: Uint16Array) {
    const c = canvas?.getContext("2d");
    if (!c) return;
    const img = c.createImageData(240, 240), d = img.data;
    for (let i = 0; i < 240 * 240; i++) {
      const v = ((fb[i] >> 8) | (fb[i] << 8)) & 0xffff;          // the panel's byte order -> RGB565 (as Try Gir)
      d[4 * i] = ((v >> 11) * 255) / 31; d[4 * i + 1] = (((v >> 5) & 63) * 255) / 63; d[4 * i + 2] = ((v & 31) * 255) / 31; d[4 * i + 3] = 255;
    }
    c.putImageData(img, 0, 0);
  }

  // the computer's keyboard, as Try Gir's and the original's: F3..A#4 on the Z (white) and A (black) rows, B4..G5 on
  // Q..O by semitone, the twelve buttons on the number row in the panel's order, [ ] OCT, the arrows SELECT / ALGORITHM
  const KEYMAP: Record<string, { key?: number; btn?: number; enc?: number; n?: number }> = {};
  {
    const { B, EN, WHITE, ROW1, ROW2 } = PANEL;
    ["KeyZ", "KeyX", "KeyC", "KeyV", "KeyB", "KeyN", "KeyM", "Comma", "Period", "Slash"].forEach((c, i) => { KEYMAP[c] = { key: WHITE[i] }; });
    ([["KeyS", 1], ["KeyD", 3], ["KeyF", 5], ["KeyH", 8], ["KeyJ", 10], ["KeyL", 13], ["Semicolon", 15], ["Quote", 17]] as [string, number][]).forEach(([c, k]) => { KEYMAP[c] = { key: k }; });
    ["KeyQ", "KeyW", "KeyE", "KeyR", "KeyT", "KeyY", "KeyU", "KeyI", "KeyO"].forEach((c, i) => { KEYMAP[c] = { key: 18 + i }; });
    ["Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6", "Digit7", "Digit8", "Digit9", "Digit0", "Minus", "Equal"]
      .forEach((c, i) => { KEYMAP[c] = { btn: B[[...ROW1, ...ROW2][i]] }; });
    Object.assign(KEYMAP, { BracketLeft: { btn: B["OCT-"] }, BracketRight: { btn: B["OCT+"] },
      ArrowUp: { enc: EN.SELECT, n: 1 }, ArrowDown: { enc: EN.SELECT, n: -1 }, ArrowRight: { enc: EN.ALGORITHM, n: 1 }, ArrowLeft: { enc: EN.ALGORITHM, n: -1 } });
  }
  const typing = (x: any) => x?.isContentEditable || /^(TEXTAREA|SELECT)$/.test(x?.tagName) || (x?.tagName === "INPUT" && !/^(checkbox|radio|button|range)$/.test(x.type));
  function onKey(e: KeyboardEvent) {
    if (!kbd || e.metaKey || e.ctrlKey || e.altKey || typing(e.target)) return;
    const m = KEYMAP[e.code];
    if (!m) return;
    e.preventDefault(); e.stopPropagation();
    if (m.enc !== undefined) { turn(m.enc, m.n!); return; }
    if (e.repeat) return;
    if (m.key !== undefined) held.key.set("k" + e.code, m.key); else held.btn.set("k" + e.code, m.btn!);
    sync();
  }
  function onKeyUp(e: KeyboardEvent) { const id = "k" + e.code; if (held.btn.delete(id) || held.key.delete(id)) sync(); }
  function letGo() { held.btn.clear(); held.key.clear(); sync(); }
  let kbd = $state(true);

  onMount(() => {
    geo = PANEL.draw(svg, "wide", { knobFont: "DotGothic16" });   // (rc3) the unit's own layout, as Try Gir
    for (const b of geo.buttons) {
      b.g.style.cursor = "pointer";
      b.g.addEventListener("pointerdown", (e: PointerEvent) => {
        (e.currentTarget as Element).setPointerCapture?.(e.pointerId);
        const id = "p" + e.pointerId;
        if (e.shiftKey || e.button === 2) { held.btn.has("l" + b.bit) ? held.btn.delete("l" + b.bit) : held.btn.set("l" + b.bit, b.bit); }   // latched
        else held.btn.set(id, b.bit);
        sync();
      });
      b.g.addEventListener("contextmenu", (e: Event) => e.preventDefault());
      const up = (e: PointerEvent) => { if (held.btn.delete("p" + e.pointerId)) sync(); };
      b.g.addEventListener("pointerup", up); b.g.addEventListener("pointercancel", up);
    }
    geo.keys.forEach((kk: any, k: number) => {
      if (!kk) return;
      kk.g.style.cursor = "pointer";
      kk.g.addEventListener("pointerdown", (e: PointerEvent) => { (e.currentTarget as Element).setPointerCapture?.(e.pointerId); held.key.set("p" + e.pointerId, k); sync(); });
      const up = (e: PointerEvent) => { if (held.key.delete("p" + e.pointerId)) sync(); };
      kk.g.addEventListener("pointerup", up); kk.g.addEventListener("pointercancel", up);
    });
    for (const kn of geo.knobs) {
      if (kn.role === "master") continue;
      kn.g.style.cursor = "ns-resize";
      kn.g.addEventListener("wheel", (e: WheelEvent) => { e.preventDefault(); turn(kn.role, e.deltaY < 0 ? 1 : -1); }, { passive: false });
      let y0: number | null = null;
      kn.g.addEventListener("pointerdown", (e: PointerEvent) => { y0 = e.clientY; (e.currentTarget as Element).setPointerCapture?.(e.pointerId); });
      kn.g.addEventListener("pointermove", (e: PointerEvent) => { if (y0 == null) return; const d = Math.trunc((y0 - e.clientY) / 8); if (d) { turn(kn.role, d); y0 -= d * 8; } });
      kn.g.addEventListener("pointerup", () => (y0 = null));
    }
    if (emu) {
      if (emu.lastFb) queueMicrotask(() => { if (!casting) draw565(emu.lastFb!); });
      off = emu.onFrame((f) => { if (casting) return; if (f.fb) draw565(f.fb); if (f.leds) leds = f.leds; });
    }
    addEventListener("keydown", onKey, true);
    addEventListener("keyup", onKeyUp, true);
    addEventListener("blur", letGo);
    document.addEventListener("fullscreenchange", onFs);
  });
  const onFs = () => (isFull = document.fullscreenElement === wrap);
  onDestroy(() => {
    off?.();
    removeEventListener("keydown", onKey, true);
    removeEventListener("keyup", onKeyUp, true);
    removeEventListener("blur", letGo);
    document.removeEventListener("fullscreenchange", onFs);
    held.btn.clear(); held.key.clear();
    if (!emu && device.ready) device.panel(0, 0).catch(() => {});
    else emu?.panel({ type: "buttons", mask: 0 });
  });
  $effect(() => {                            // the copy's screen, each time it draws
    void cast.draws;
    if (casting) { const fb = castScreen(); if (fb) draw565(fb); }
  });
  $effect(() => {                            // the lit buttons and keys (the copy's while casting)
    const l = casting ? cast.leds : leds;
    if (!geo) return;
    for (const b of geo.buttons) {
      const green = b.bit === PANEL.B.PLAY && !!((l[0] >> PANEL.PLAY_GREEN) & 1);     // PLAY lit green while it plays
      b.g.classList.toggle("green", green);
      b.g.classList.toggle("lit", !green && !!((l[0] >> b.bit) & 1));
    }
    geo.keys.forEach((kk: any, k: number) => kk?.g.classList.toggle("lit", !!((l[1] >> k) & 1)));
  });
  const scr = $derived(geo ? geo.screen : [0, 0, 0]);
  function full() { if (document.fullscreenElement) document.exitFullscreen().catch(() => {}); else wrap.requestFullscreen?.().catch(() => {}); }

  async function start() {
    if (!(await ask({ title: t("cast.title"), body: t("cast.explain"), ok: t("cast.go") }))) return;
    await startCast();
  }
  const status = $derived(cast.say ? t(cast.say, cast.vars) : "");
</script>

<div class="screen">
  <header>
    <div class="lead">
      {#if emu}
        <p>{t("screen.demoOnly")}</p>
      {:else if casting}
        <p class="on"><span class="dotlive"></span>{t("cast.liveOn")}</p>
      {:else}
        <p>{t("cast.lead")}</p>
      {/if}
      {#if status}<p class="st" class:bad={cast.bad} data-testid="cast-status">{status}</p>{/if}
    </div>
    {#if !emu || cast.state !== "idle"}
      {#if cast.state === "idle"}
        <Button size="s" variant="primary" onclick={start} disabled={!device.ready || !canCast()}>{t("cast.start")}</Button>
      {:else}
        <Button size="s" onclick={() => stopCast()}>{cast.state === "on" ? t("cast.stop") : t("dlg.cancel")}</Button>
      {/if}
    {/if}
    <label class="kb"><input type="checkbox" bind:checked={kbd} /> {t("screen.keyboard")}</label>
    <Button size="s" onclick={full}>{isFull ? t("screen.exitFull") : t("screen.full")}</Button>
  </header>
  <div class="wrap" class:full={isFull} bind:this={wrap}>
    <div class="unit">
      <svg bind:this={svg} class="panel" role="application" aria-label="FM-1"></svg>
      {#if geo && live}
        <canvas bind:this={canvas} data-lint-over width="240" height="240" aria-label="FM-1 screen" data-testid="screen-canvas"
                style="left:{(scr[0] / geo.L.w) * 100}%; top:{(scr[1] / geo.L.h) * 100}%; width:{(scr[2] / geo.L.w) * 100}%"></canvas>
      {:else if geo}
        <div class="noscr" data-lint-over style="left:{(scr[0] / geo.L.w) * 100}%; top:{(scr[1] / geo.L.h) * 100}%; width:{(scr[2] / geo.L.w) * 100}%; height:{(scr[2] / geo.L.h) * 100}%">
          {#if cast.state === "idle"}<span class="dot">GIR 1.0</span>{:else}<span class="spin"></span>{/if}
        </div>
      {/if}
    </div>
    {#if isFull && status}<p class="fst">{status}</p>{/if}
  </div>
  <p class="keys">{t("screen.keysHelp")}</p>
</div>

<style>
  .screen { display: flex; flex-direction: column; gap: 12px; }
  header { display: flex; align-items: center; gap: 12px; flex-wrap: wrap; }
  .lead { flex: 1; min-width: 260px; display: flex; flex-direction: column; gap: 3px; }
  .lead p { margin: 0; color: var(--fg-2); font-size: 13px; line-height: 1.45; }
  .lead .st { color: var(--dim); font-size: 12.5px; }
  .lead .st.bad { color: var(--warn); }
  .on { display: flex; align-items: center; gap: 8px; color: var(--fg) !important; }
  .dotlive { width: 8px; height: 8px; border-radius: 50%; background: var(--ok); box-shadow: 0 0 0 3px color-mix(in srgb, var(--ok) 25%, transparent); }
  .kb { display: flex; align-items: center; gap: 6px; font-size: 12.5px; color: var(--fg-2); }
  /* (rc3) Try Gir's panel: its layout (the unit's), its colours */
  .wrap { position: relative; width: 100%; max-width: 1180px; --body: #1d1d1f; --tray: #141415; --part: #2a2a2d; --edge: #3a3a3e; --label: #d6d6d8; --lit: #fff4d6; background: var(--bg-2); }
  .unit { position: relative; width: 100%; }
  .wrap.full { display: flex; flex-direction: column; align-items: center; justify-content: center; background: #000; max-width: none; }
  .wrap.full .unit { width: min(100vw, calc(100vh * 1000 / 624)); }
  .fst { color: #8a8a90; font-size: 12px; margin: 8px 0 0; }
  .panel { width: 100%; height: auto; display: block; user-select: none; touch-action: none; }
  .panel :global(.face) { transition: fill 0.05s; }
  .panel :global(.face.key) { fill: #0c0c0d; stroke: var(--edge); stroke-width: 1.5; }
  .panel :global(.slit) { fill: #3a3a3c; }
  .panel :global(.lit .face.btn) { fill: var(--lit); }
  .panel :global(.lit text) { fill: #1d1d1f; }
  .panel :global(.lit .slit) { fill: #ececee; filter: drop-shadow(0 0 3px rgb(255 255 255 / 60%)); }
  .panel :global(.green .face.btn) { fill: #5fe08a; }
  .panel :global(.green text) { fill: #0d2414; }
  .panel :global(.held .face) { stroke: var(--lit); stroke-width: 3; }
  canvas { position: absolute; image-rendering: pixelated; aspect-ratio: 1; border-radius: 4px; }
  .noscr { position: absolute; display: grid; place-items: center; background: #080808; color: var(--mute); border-radius: 4px; font-size: 28px; }
  .spin { width: 28px; height: 28px; border-radius: 50%; border: 3px solid #333; border-top-color: var(--accent); animation: sp 0.9s linear infinite; }
  @keyframes sp { to { transform: rotate(360deg); } }
  .keys { margin: 0; color: var(--dim); font-size: 12px; }
  @media (prefers-reduced-motion: reduce) { .spin { animation: none; } }
</style>
