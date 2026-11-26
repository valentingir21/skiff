<script lang="ts">
  // Gir's rumble over one beat: the kick (white), then the tail (the track's colour) as R.SLICE cuts it, R.DECAY its
  // length, R.ATTACK its start, R.DUCK its return under the kick; RUMBLE its height
  interface Props { mix: number; slice: number; decay: number; atk: number; duck: number; tone: number; color?: string }
  let { mix, slice, decay, atk, duck, tone, color = "var(--accent)" }: Props = $props();
  const W = 300, H = 84, B = 4;           // two beats
  const path = $derived.by(() => {
    const n = 240, pts: string[] = [];
    const amp = mix ? 0.18 + 0.82 * Math.sqrt(mix / 127) : 0;   // (drawn so a low RUMBLE still shows its shape)
    const tau = 0.25 + (decay / 127) * 2.5;          // in beats
    for (let i = 0; i <= n; i++) {
      const x = (i / n) * 2;                         // beats
      const tb = x % 1;                              // since the beat's kick
      const sx = Math.floor(tb * 4);                 // its sixteenth (0 = the kick's)
      const st = tb * 4 - sx;
      let env = Math.exp(-tb / tau);
      const back = duck <= 0 ? 1 : Math.min(1, tb / (0.02 + (duck / 127) * 0.6));
      env *= back;
      if (slice === 0) env *= Math.min(1, tb / Math.max(0.001, (atk / 127) * 0.3 + 0.001));
      else {
        const on = slice === 1 ? sx === 2 : slice === 2 ? sx === 1 || sx === 3 : sx >= 1;
        const gate = slice === 4 ? st < 0.5 : true;
        const rise = Math.min(1, st / Math.max(0.02, (atk / 127) * 0.5));
        env = on && gate ? env * rise * Math.max(0, 1 - Math.max(0, st - 0.92) / 0.08) : 0;
      }
      const y = H - B - env * amp * (H - 2 * B) * 0.9;
      pts.push(`${i ? "L" : "M"}${((x / 2) * (W - 8) + 4).toFixed(1)} ${y.toFixed(1)}`);
    }
    return pts.join(" ");
  });
  const kicks = [0, 1].map((b) => (b / 2) * (W - 8) + 4);
</script>
<svg viewBox="0 0 {W} {H}" class="g" style="--c:{color}; --o:{0.35 + (tone / 127) * 0.65}" role="img" aria-label="Rumble">
  {#each [0, 1, 2, 3, 4, 5, 6, 7] as s}<line x1={(s / 8) * (W - 8) + 4} x2={(s / 8) * (W - 8) + 4} y1={B} y2={H - B} class="grid" class:beat={s % 4 === 0} />{/each}
  <path d={path + ` L${W - 4} ${H - B} L4 ${H - B} Z`} class="area" />
  <path d={path} class="line" />
  {#each kicks as x}<rect x={x} y={B + 2} width="3" height={H - 2 * B - 2} rx="1.5" class="kick" /><circle cx={x + 1.5} cy={B + 4} r="4" class="kick" />{/each}
</svg>
<style>
  .g { width: 100%; height: 84px; display: block; }
  .grid { stroke: var(--line); }
  .grid.beat { stroke: var(--line-2); }
  .area { fill: color-mix(in srgb, var(--c) 18%, transparent); opacity: var(--o); }
  .line { fill: none; stroke: var(--c); stroke-width: 2; }
  .kick { fill: var(--fg); opacity: 0.85; }
</style>
