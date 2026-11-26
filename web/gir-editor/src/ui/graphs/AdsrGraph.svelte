<script lang="ts">
  // An ADSR (values 0..127; times on the device's exponential curve, 1 ms .. 10 s), drawn and editable: drag the
  // attack peak sideways, the decay / sustain corner both ways, the release end sideways
  interface Props { a: number; d: number; s: number; r: number; color?: string; onchange?: (which: "a" | "d" | "s" | "r", v: number) => void }
  let { a, d, s, r, color = "var(--accent)", onchange }: Props = $props();
  const W = 260, H = 84, P = 6;
  // the time axis is the values themselves (the curve is exponential: this is how the knobs feel), each segment a quarter
  const seg = (v: number) => (v / 127) * ((W - 2 * P) / 3.4);
  const pts = $derived.by(() => {
    const x0 = P, x1 = x0 + seg(a), x2 = x1 + seg(d), hold = (W - 2 * P) * 0.18, x3 = x2 + hold, x4 = x3 + seg(r);
    const yT = P, yB = H - P, yS = yB - (s / 127) * (yB - yT);
    return { x0, x1, x2, x3, x4, yT, yB, yS };
  });
  let svg: SVGSVGElement;
  let drag: "a" | "ds" | "r" | null = null;
  function loc(e: PointerEvent) { const b = svg.getBoundingClientRect(); return { x: ((e.clientX - b.left) / b.width) * W, y: ((e.clientY - b.top) / b.height) * H }; }
  const inv = (px: number) => Math.max(0, Math.min(127, Math.round((px / ((W - 2 * P) / 3.4)) * 127)));
  function down(which: typeof drag, e: PointerEvent) { if (!onchange) return; drag = which; svg.setPointerCapture(e.pointerId); e.stopPropagation(); }
  function move(e: PointerEvent) {
    if (!drag || !onchange) return;
    const { x, y } = loc(e), p = pts;
    if (drag === "a") onchange("a", inv(x - p.x0));
    else if (drag === "ds") { onchange("d", inv(x - p.x1)); onchange("s", Math.max(0, Math.min(127, Math.round(((p.yB - y) / (p.yB - p.yT)) * 127)))); }
    else if (drag === "r") onchange("r", inv(x - p.x3));
  }
</script>
<svg bind:this={svg} viewBox="0 0 {W} {H}" class="g" style="--c:{color}" onpointermove={move} onpointerup={() => (drag = null)} role="img" aria-label="ADSR">
  <path class="area" d="M{pts.x0} {pts.yB} L{pts.x1} {pts.yT} L{pts.x2} {pts.yS} L{pts.x3} {pts.yS} L{pts.x4} {pts.yB} Z" />
  <path class="line" d="M{pts.x0} {pts.yB} L{pts.x1} {pts.yT} L{pts.x2} {pts.yS} L{pts.x3} {pts.yS} L{pts.x4} {pts.yB}" />
  {#if onchange}
    <circle class="h" role="slider" tabindex="-1" aria-label="Attack" aria-valuenow={a} cx={pts.x1} cy={pts.yT} r="5" onpointerdown={(e) => down("a", e)} />
    <circle class="h" role="slider" tabindex="-1" aria-label="Decay, sustain" aria-valuenow={s} cx={pts.x2} cy={pts.yS} r="5" onpointerdown={(e) => down("ds", e)} />
    <circle class="h" role="slider" tabindex="-1" aria-label="Release" aria-valuenow={r} cx={pts.x4} cy={pts.yB} r="5" onpointerdown={(e) => down("r", e)} />
  {/if}
</svg>
<style>
  .g { width: 100%; height: 84px; display: block; touch-action: none; }
  .area { fill: color-mix(in srgb, var(--c) 14%, transparent); }
  .line { fill: none; stroke: var(--c); stroke-width: 2; stroke-linejoin: round; }
  .h { fill: var(--bg); stroke: var(--c); stroke-width: 2; cursor: grab; }
  .h:hover { fill: var(--c); }
</style>
