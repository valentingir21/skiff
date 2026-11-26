<script lang="ts">
  // The FM-1's panel, as Try Gir draws it (web/panel.js), working the unit: its buttons and keys held while pressed,
  // its knobs turned by dragging or the wheel. In demo mode the virtual FM-1's screen is live in it. On a real unit
  // the panel works the same (PANEL, cmd 84); its screen needs the original editor's cast (see FIRMWARE-A-PREVOIR.md).
  import { onDestroy, onMount } from "svelte";
  import { device } from "../lib/model/device.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import "../../../panel.js";
  import Button from "../ui/components/Button.svelte";

  const PANEL = (window as any).PANEL;
  let svg: SVGSVGElement;
  let canvas: HTMLCanvasElement | undefined = $state();
  let wrap: HTMLDivElement;
  let geo: any = $state(null);
  let btnMask = 0, keyMask = 0;
  let leds = $state<number[]>([0, 0]);
  const emu = $derived(device.emu);
  let off: (() => void) | null = null;

  function send() {
    if (emu) { emu.panel({ type: "buttons", mask: btnMask }); emu.panel({ type: "keys", mask: keyMask }); }
    else device.panel(btnMask, keyMask).catch(() => {});
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
  onMount(() => {
    geo = PANEL.draw(svg, "side", { knobFont: "DotGothic16" });
    for (const b of geo.buttons) {
      b.g.style.cursor = "pointer";
      b.g.addEventListener("pointerdown", (e: PointerEvent) => { (e.currentTarget as Element).setPointerCapture?.(e.pointerId); btnMask |= 1 << b.bit; send(); });
      const up = () => { if (btnMask & (1 << b.bit)) { btnMask &= ~(1 << b.bit); send(); } };
      b.g.addEventListener("pointerup", up); b.g.addEventListener("pointercancel", up); b.g.addEventListener("pointerleave", up);
    }
    geo.keys.forEach((kk: any, k: number) => {
      if (!kk) return;
      kk.g.style.cursor = "pointer";
      kk.g.addEventListener("pointerdown", () => { keyMask |= 1 << k; send(); });
      const up = () => { if (keyMask & (1 << k)) { keyMask &= ~(1 << k); send(); } };
      kk.g.addEventListener("pointerup", up); kk.g.addEventListener("pointerleave", up);
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
      if (emu.lastFb) queueMicrotask(() => draw565(emu.lastFb!));
      off = emu.onFrame((f) => { if (f.fb) draw565(f.fb); if (f.leds) leds = f.leds; });
    }
  });
  onDestroy(() => { off?.(); btnMask = 0; keyMask = 0; if (!emu && device.ready) device.panel(0, 0).catch(() => {}); });
  $effect(() => {                            // the lit buttons and keys
    if (!geo) return;
    for (const b of geo.buttons) b.g.classList.toggle("lit", !!((leds[0] >> b.bit) & 1));
    geo.keys.forEach((kk: any, k: number) => kk?.g.classList.toggle("lit", !!((leds[1] >> k) & 1)));
  });
  const scr = $derived(geo ? geo.screen : [0, 0, 0]);
  function full() { wrap.requestFullscreen?.(); }
</script>

<div class="screen">
  <header>
    <p>{emu ? t("screen.demoOnly") : t("screen.device")} {t("screen.panel")}</p>
    <Button size="s" onclick={full}>{t("screen.full")}</Button>
  </header>
  <div class="wrap" bind:this={wrap}>
    <svg bind:this={svg} class="panel" role="application" aria-label="FM-1"></svg>
    {#if emu && geo}
      <canvas bind:this={canvas} width="240" height="240" aria-label="FM-1 screen"
              style="left:{(scr[0] / geo.L.w) * 100}%; top:{(scr[1] / geo.L.h) * 100}%; width:{(scr[2] / geo.L.w) * 100}%"></canvas>
    {:else if geo}
      <div class="noscr" style="left:{(scr[0] / geo.L.w) * 100}%; top:{(scr[1] / geo.L.h) * 100}%; width:{(scr[2] / geo.L.w) * 100}%; height:{(scr[2] / geo.L.h) * 100}%">
        <span class="dot">GIR 1.0</span>
      </div>
    {/if}
  </div>
</div>

<style>
  .screen { display: flex; flex-direction: column; gap: 12px; }
  header { display: flex; align-items: center; gap: 12px; }
  header p { margin: 0; color: var(--dim); font-size: 12.5px; flex: 1; }
  .wrap { position: relative; width: 100%; max-width: 1400px; --body: #1b1b1e; --tray: #131315; --part: #2a2a2f; --edge: #3a3a41; --label: #c9c6cd; background: var(--bg-2); }
  .wrap:fullscreen { display: grid; place-items: center; background: #000; }
  .panel { width: 100%; height: auto; display: block; user-select: none; touch-action: none; }
  .panel :global(.face.key) { fill: #e8e5ea; }
  .panel :global(.slit) { fill: #c4c1c6; }
  .panel :global(.lit .face.btn) { fill: color-mix(in srgb, var(--accent) 55%, #2a2a2f); }
  .panel :global(.lit .slit) { fill: var(--accent); }
  .panel :global(g[data-ctrl]:active .face) { filter: brightness(1.3); }
  canvas { position: absolute; image-rendering: pixelated; aspect-ratio: 1; border-radius: 4px; }
  .noscr { position: absolute; display: grid; place-items: center; background: #080808; color: var(--mute); border-radius: 4px; font-size: 28px; }
</style>
