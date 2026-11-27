<script lang="ts">
  // A sound's waveform with its two ends to drag (the start and the end of what is kept), as the original's trim:
  // the wheel zooms at the pointer, shift + wheel (or a drag away from the ends, zoomed in) moves the view, a double
  // click shows it all. Arrow keys move the focused end (shift: 10 times further).
  import { zoomView } from "../../lib/legacy/proto.js";
  import { RATE } from "../../lib/samples";

  let { raw, a = $bindable(), b = $bindable(), disabled = false, height = 64, label = "", onchange = () => {} }:
    { raw: Float64Array; a: number; b: number; disabled?: boolean; height?: number; label?: string; onchange?: () => void } = $props();
  let cv: HTMLCanvasElement;
  let v0 = $state(0), v1 = $state(0);
  let focus = $state<"a" | "b">("a");
  $effect(() => { if (v1 === 0 || v1 > raw.length) { v0 = 0; v1 = raw.length; } });
  const pk = $derived.by(() => { let p = 1e-9; for (const v of raw) p = Math.max(p, Math.abs(v)); return p; });
  const MIN = Math.round(RATE * 0.01);

  function draw() {
    if (!cv) return;
    const w = (cv.width = Math.max(100, Math.round(cv.clientWidth * devicePixelRatio))), H = (cv.height = Math.round(height * devicePixelRatio)), h = H - 6;
    const g = cv.getContext("2d")!, cs = getComputedStyle(cv);
    const fg = cs.getPropertyValue("--accent").trim() || "#fff", dim = cs.getPropertyValue("--mute").trim() || "#555", line = cs.getPropertyValue("--line").trim() || "#333";
    const n = raw.length, span = Math.max(1, v1 - v0), xa = ((a - v0) / span) * w, xb = ((b - v0) / span) * w;
    g.clearRect(0, 0, w, H);
    g.fillStyle = "rgba(255,255,255,0.04)";
    g.fillRect(Math.max(0, xa), 0, Math.max(0, Math.min(w, xb) - Math.max(0, xa)), h);
    for (let x = 0; x < w; x++) {
      const i0 = v0 + Math.floor((x / w) * span), i1 = Math.max(i0 + 1, v0 + Math.floor(((x + 1) / w) * span));
      let lo = 0, hi = 0;
      for (let i = i0; i < i1 && i < n; i++) { lo = Math.min(lo, raw[i]); hi = Math.max(hi, raw[i]); }
      g.fillStyle = x >= xa && x < xb ? fg : dim;
      g.fillRect(x, h / 2 - (hi / pk) * (h / 2 - 2), 1, Math.max(1, ((hi - lo) / pk) * (h / 2 - 2)));
    }
    const dpr = devicePixelRatio;
    g.fillStyle = fg;
    for (const [x, which] of [[xa, "a"], [xb, "b"]] as const) {
      if (x < -2 || x > w + 2) continue;
      const xx = Math.max(0, Math.min(w - 3 * dpr, x - 1.5 * dpr));
      g.fillRect(xx, 0, 3 * dpr, h);
      g.fillRect(xx - 3 * dpr, which === "a" ? 0 : h - 8 * dpr, 9 * dpr, 8 * dpr);   // a grip: top for the start, bottom for the end
    }
    g.fillStyle = line;
    g.fillRect(0, H - 2, w, 2);
    g.fillStyle = fg;
    g.fillRect((v0 / n) * w, H - 3, Math.max(3, (span / n) * w), 3);
  }
  $effect(() => { void a; void b; void v0; void v1; void raw; draw(); });

  const frac = (e: PointerEvent | WheelEvent) => { const r = cv.getBoundingClientRect(); return Math.max(0, Math.min(1, (e.clientX - r.left) / r.width)); };
  const at = (e: PointerEvent) => Math.round(v0 + frac(e) * (v1 - v0));
  const zoomed = () => v1 - v0 < raw.length;
  function near(e: PointerEvent): "a" | "b" | null {
    const r = cv.getBoundingClientRect(), span = v1 - v0;
    const da = Math.abs(e.clientX - (r.left + ((a - v0) / span) * r.width)), db = Math.abs(e.clientX - (r.left + ((b - v0) / span) * r.width));
    return Math.min(da, db) > 10 ? null : da <= db ? "a" : "b";
  }
  let end: "a" | "b" | null = null;
  let pan: { x: number; v0: number; span: number } | null = null;
  function down(e: PointerEvent) {
    if (disabled || e.button !== 0 || !e.isPrimary) return;
    cv.setPointerCapture(e.pointerId);
    end = near(e);
    if (!end && zoomed()) { pan = { x: e.clientX, v0, span: v1 - v0 }; return; }
    const p = at(e);
    end = end || (Math.abs(p - a) <= Math.abs(p - b) ? "a" : "b");
    focus = end;
    move(e);
  }
  function move(e: PointerEvent) {
    if (pan) {
      const r = cv.getBoundingClientRect();
      const x = Math.round(pan.v0 - ((e.clientX - pan.x) / r.width) * pan.span);
      v0 = Math.max(0, Math.min(raw.length - pan.span, x)); v1 = v0 + pan.span;
      return;
    }
    if (!end) { cv.style.cursor = near(e) || !zoomed() ? "ew-resize" : "grab"; return; }
    const p = at(e);
    if (end === "a") a = Math.max(0, Math.min(p, b - MIN)); else b = Math.min(raw.length, Math.max(p, a + MIN));
  }
  function up() { if (end) onchange(); end = null; pan = null; }
  function wheel(e: WheelEvent) {
    e.preventDefault();
    const d = e.deltaY || e.deltaX;
    if (!d) return;
    [v0, v1] = e.shiftKey || (!e.deltaY && e.deltaX)
      ? zoomView(raw.length, v0, v1, frac(e), 1, d > 0 ? 0.1 : -0.1)
      : zoomView(raw.length, v0, v1, frac(e), d > 0 ? 1.25 : 0.8, 0);
  }
  function key(e: KeyboardEvent) {
    if (disabled) return;
    const step = Math.round((v1 - v0) / 200) * (e.shiftKey ? 10 : 1) || 1;
    if (e.key === "Tab") return;
    if (e.key === "a" || e.key === "b") { focus = e.key; return; }
    const d = e.key === "ArrowRight" ? step : e.key === "ArrowLeft" ? -step : 0;
    if (!d) return;
    e.preventDefault();
    if (focus === "a") a = Math.max(0, Math.min(a + d, b - MIN)); else b = Math.min(raw.length, Math.max(b + d, a + MIN));
    onchange();
  }
</script>

<canvas bind:this={cv} class="wave" class:off={disabled} style="height:{height}px" tabindex="0" role="slider" aria-label={label}
        aria-valuemin={0} aria-valuemax={raw.length} aria-valuenow={focus === "a" ? a : b}
        aria-valuetext={`${(a / RATE).toFixed(3)} s – ${(b / RATE).toFixed(3)} s`}
        onpointerdown={down} onpointermove={move} onpointerup={up} onpointercancel={up} onwheel={wheel}
        ondblclick={() => { v0 = 0; v1 = raw.length; }} onkeydown={key}></canvas>

<style>
  .wave { width: 100%; display: block; border-radius: 6px; background: var(--surf-3); touch-action: none; cursor: ew-resize; }
  .wave:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
  .off { opacity: 0.5; pointer-events: none; }
</style>
