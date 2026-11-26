<script lang="ts">
  // an LFO's wave by its name (SIN TRI SAW SQR S&H, the device's), with its rate as cycles shown
  interface Props { wave: string; cycles?: number; color?: string; phase?: number }
  let { wave, cycles = 2, color = "var(--accent)", phase = 0 }: Props = $props();
  const W = 260, H = 60;
  const path = $derived.by(() => {
    const n = 160, out: string[] = [];
    let seed = 7;
    const rnd = () => ((seed = (seed * 9301 + 49297) % 233280) / 233280) * 2 - 1;
    const holds = Array.from({ length: 64 }, rnd);
    for (let i = 0; i <= n; i++) {
      const x = i / n, ph = (x * cycles + phase / 128) % 1;
      const w = wave.toUpperCase();
      const y = w.startsWith("SIN") ? Math.sin(ph * 2 * Math.PI) : w.startsWith("TRI") ? 1 - 4 * Math.abs(ph - 0.5) :
        w.startsWith("SAW") ? 1 - 2 * ph : w.startsWith("SQ") ? (ph < 0.5 ? 1 : -1) : holds[Math.floor(x * cycles * 4) % 64];
      out.push(`${i ? "L" : "M"}${(x * (W - 8) + 4).toFixed(1)} ${(H / 2 - y * (H / 2 - 6)).toFixed(1)}`);
    }
    return out.join(" ");
  });
</script>
<svg viewBox="0 0 {W} {H}" class="g" style="--c:{color}" role="img" aria-label={wave}>
  <line x1="4" x2={W - 4} y1={H / 2} y2={H / 2} class="axis" />
  <path d={path} class="line" />
</svg>
<style>
  .g { width: 100%; height: 60px; display: block; }
  .axis { stroke: var(--line-2); stroke-dasharray: 2 4; }
  .line { fill: none; stroke: var(--c); stroke-width: 2; stroke-linejoin: round; }
</style>
