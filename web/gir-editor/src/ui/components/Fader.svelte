<script lang="ts">
  // a vertical fader (the mixer's LEVEL): drag, wheel, arrows, double-click the default
  interface Props { value: number; min?: number; max?: number; def?: number; label: string; text: string; color?: string; height?: number; onchange: (v: number) => void }
  let { value, min = 0, max = 127, def = 100, label, text, color = "var(--accent)", height = 150, onchange }: Props = $props();
  let el: HTMLDivElement;
  let drag = false;
  const f = $derived((value - min) / (max - min));
  const set = (v: number) => { const c = Math.min(max, Math.max(min, Math.round(v))); if (c !== value) onchange(c); };
  function at(e: PointerEvent) { const r = el.getBoundingClientRect(); set(min + (1 - (e.clientY - r.top) / r.height) * (max - min)); }
  function down(e: PointerEvent) { el.setPointerCapture(e.pointerId); drag = true; at(e); }
  function move(e: PointerEvent) { if (drag) at(e); }
  function up() { drag = false; }
  function key(e: KeyboardEvent) {
    const d = e.shiftKey ? 10 : 1;
    if (e.key === "ArrowUp") set(value + d); else if (e.key === "ArrowDown") set(value - d); else return;
    e.preventDefault();
  }
</script>
<div class="fader" style="--c:{color}; --h:{height}px">
  <div class="rail" bind:this={el} role="slider" tabindex="0" aria-label={label} aria-valuemin={min} aria-valuemax={max} aria-valuenow={value}
       onpointerdown={down} onpointermove={move} onpointerup={up} onpointercancel={up} onkeydown={key}
       onwheel={(e) => { e.preventDefault(); set(value + (e.deltaY < 0 ? 1 : -1)); }} ondblclick={() => set(def)}>
    <div class="fill" style="height:{f * 100}%"></div>
    <div class="cap" style="bottom:calc({f * 100}% - 7px)"></div>
  </div>
  <div class="val num">{text}</div>
</div>
<style>
  .fader { display: flex; flex-direction: column; align-items: center; gap: 6px; }
  .rail { position: relative; width: 30px; height: var(--h); border-radius: 8px; background: var(--surf); border: 1px solid var(--line); cursor: ns-resize; touch-action: none; }
  .rail:focus-visible { box-shadow: var(--focus); }
  .fill { position: absolute; left: 12px; right: 12px; bottom: 0; border-radius: 3px; background: var(--c); opacity: 0.85; }
  .cap { position: absolute; left: 2px; right: 2px; height: 14px; border-radius: 4px; background: var(--fg); box-shadow: 0 2px 6px rgb(0 0 0 / 50%); }
  .val { font-family: var(--font-dot); font-size: 13px; color: var(--fg-2); min-height: 16px; }
</style>
