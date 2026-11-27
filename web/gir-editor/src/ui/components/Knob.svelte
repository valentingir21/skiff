<script lang="ts">
  // A knob as the FM-1's: drag up / down (Alt or Option: fine), the wheel, the arrow keys (Shift: x10), double-click:
  // the default, click the value: type it (an enum: pick it from its list). Its arc starts at the minimum, or at 0
  // for a bipolar range. Every change calls onchange (the model coalesces what it sends).
  import { onDestroy } from "svelte";

  interface Props {
    value: number; min: number; max: number; def?: number;
    label: string; text: string; unit?: string;
    color?: string; size?: number; disabled?: boolean;
    /** (rc3) the label only for screen readers (a table's column says it); "row": the value beside the dial */
    hideLabel?: boolean; layout?: "col" | "row";
    names?: string[] | null;               // an enum's value names (min .. max)
    describe?: string;                     // tooltip text
    onchange: (v: number) => void;
    onhover?: (on: boolean) => void;
  }
  let { value, min, max, def = min, label, text, unit = "", color = "var(--accent)", size: asked = 48, disabled = false,
        names = null, describe = "", onchange, onhover, hideLabel = false, layout = "col" }: Props = $props();
  // (rc3) two sizes in the whole app, as the layout lint wants: normal (48) and compact (34)
  const compact = $derived(asked < 44);
  const size = $derived(compact ? 34 : 48);

  const range = $derived(Math.max(1, max - min));
  const frac = $derived((Math.min(max, Math.max(min, value)) - min) / range);
  const zero = $derived(min < 0 && max > 0 ? (0 - min) / range : 0);
  const A0 = -135, A1 = 135;
  const ang = (f: number) => A0 + (A1 - A0) * f;
  const R = 22, C = 28;
  function arc(f0: number, f1: number) {
    if (Math.abs(f1 - f0) < 0.001) return "";
    const [a, b] = f0 < f1 ? [f0, f1] : [f1, f0];
    const p = (f: number) => { const r = ((ang(f) - 90) * Math.PI) / 180; return `${C + R * Math.cos(r)} ${C + R * Math.sin(r)}`; };
    const large = (b - a) * 270 > 180 ? 1 : 0;
    return `M ${p(a)} A ${R} ${R} 0 ${large} 1 ${p(b)}`;
  }

  let el: HTMLDivElement;
  let dragging = $state(false);
  let editing = $state(false);
  let picking = $state(false);
  let draft = $state("");
  let start = { y: 0, v: 0, acc: 0 };

  const clamp = (v: number) => Math.min(max, Math.max(min, Math.round(v)));
  function set(v: number) { const c = clamp(v); if (c !== value) onchange(c); }

  function down(e: PointerEvent) {
    if (disabled || e.button !== 0) return;
    el.setPointerCapture(e.pointerId);
    dragging = true;
    start = { y: e.clientY, v: value, acc: 0 };
    e.preventDefault();
  }
  function move(e: PointerEvent) {
    if (!dragging) return;
    const px = start.y - e.clientY;
    const per = e.altKey || e.shiftKey ? 600 : 160;      // pixels for the whole range
    const steps = Math.max(range, 1);
    const v = start.v + (px / per) * steps * (steps > 32 ? 1 : 1.6);
    set(steps <= 24 ? start.v + Math.round(px / (e.altKey ? 24 : 12)) : v);
  }
  function up(e: PointerEvent) { if (dragging) { dragging = false; el.releasePointerCapture?.(e.pointerId); } }
  function wheel(e: WheelEvent) {
    if (disabled) return;
    e.preventDefault();
    const d = e.deltaY < 0 ? 1 : -1;
    set(value + d * (e.shiftKey && range > 24 ? 10 : 1));
  }
  function key(e: KeyboardEvent) {
    if (disabled) return;
    const big = e.shiftKey ? 10 : 1;
    const k = e.key;
    if (k === "ArrowUp" || k === "ArrowRight") set(value + big);
    else if (k === "ArrowDown" || k === "ArrowLeft") set(value - big);
    else if (k === "PageUp") set(value + Math.ceil(range / 8));
    else if (k === "PageDown") set(value - Math.ceil(range / 8));
    else if (k === "Home") set(min);
    else if (k === "End") set(max);
    else if (k === "Delete" || k === "Backspace") set(def);
    else if (k === "Enter") openEdit();
    else return;
    e.preventDefault();
  }
  function openEdit() {
    if (disabled) return;
    if (names && names.length) { picking = !picking; return; }
    draft = String(value); editing = true;
  }
  function commit() {
    editing = false;
    const n = Number(draft.replace(",", "."));
    if (Number.isFinite(n)) set(n);
  }
  let input: HTMLInputElement | undefined = $state();
  $effect(() => { if (editing && input) { input.focus(); input.select(); } });
  function outside(e: PointerEvent) { if (picking && !(e.target as HTMLElement).closest?.(".pick")) picking = false; }
  $effect(() => { if (picking) { window.addEventListener("pointerdown", outside, true); return () => window.removeEventListener("pointerdown", outside, true); } });
  onDestroy(() => window.removeEventListener("pointerdown", outside, true));
</script>

<div class="knob" role="group" aria-label={label} class:disabled class:compact class:row={layout === "row"} style="--c:{color}; --s:{size}px" title={describe}
     onmouseenter={() => onhover?.(true)} onmouseleave={() => onhover?.(false)}>
  <div class="dial" bind:this={el} role="slider" tabindex={disabled ? -1 : 0}
       aria-label={label} aria-valuemin={min} aria-valuemax={max} aria-valuenow={value} aria-valuetext={`${text} ${unit}`}
       class:dragging
       onpointerdown={down} onpointermove={move} onpointerup={up} onpointercancel={up}
       onwheel={wheel} onkeydown={key} ondblclick={() => set(def)}>
    <svg viewBox="0 0 56 56" width={size} height={size} aria-hidden="true">
      <path d={arc(0, 1)} class="track" />
      <path d={arc(zero, frac)} class="fill" />
      <circle cx={C} cy={C} r="15.5" class="cap" />
      <line x1={C} y1={C} x2={C} y2={C - 12} class="needle" transform={`rotate(${ang(frac)} ${C} ${C})`} />
    </svg>
  </div>
  {#if !hideLabel}<div class="label">{label}</div>{/if}
  {#if editing}
    <input class="val edit num" bind:this={input} bind:value={draft} onblur={commit}
           onkeydown={(e) => { if (e.key === "Enter") commit(); if (e.key === "Escape") editing = false; }} aria-label={label} />
  {:else}
    <button class="val num" onclick={openEdit} tabindex="-1" disabled={disabled} aria-label={`${label}: ${text}${unit ? " " + unit : ""}`}>
      <span class="v">{text}</span>{#if unit}<span class="u">{unit}</span>{/if}
    </button>
  {/if}
  {#if picking && names}
    <div class="pick scroll" role="listbox" aria-label={label}>
      {#each names as n, i}
        {#if names.indexOf(n) === i || i === value - min}
          <button role="option" aria-selected={i === value - min} class:on={i === value - min}
                  onclick={() => { set(min + i); picking = false; }}>{n}</button>
        {/if}
      {/each}
    </div>
  {/if}
</div>

<style>
  /* (rc3) a fixed box: the dial, then a 14 px label line and an 18 px value line, whatever the value says */
  .knob { position: relative; display: flex; flex-direction: column; align-items: center; gap: 2px; width: 76px; flex: none; user-select: none; }
  .knob.compact { width: 64px; }
  .knob.row { flex-direction: row; width: auto; gap: 6px; }
  .dial { width: var(--s); height: var(--s); border-radius: 50%; cursor: ns-resize; touch-action: none; }
  .dial:focus-visible { box-shadow: var(--focus); }
  .dial.dragging { cursor: grabbing; }
  svg { display: block; overflow: visible; }
  .track { fill: none; stroke: var(--surf-4); stroke-width: 4; stroke-linecap: round; }
  .fill { fill: none; stroke: var(--c); stroke-width: 4; stroke-linecap: round; transition: stroke var(--t-med); }
  .cap { fill: var(--surf-3); stroke: var(--line-2); stroke-width: 1; }
  .dial:hover .cap { fill: var(--surf-4); }
  .needle { stroke: var(--fg); stroke-width: 2.4; stroke-linecap: round; }
  .label { height: 14px; line-height: 14px; margin-top: 2px; font-size: 10.5px; font-weight: 600; letter-spacing: 0.03em; color: var(--dim); white-space: nowrap; max-width: 100%; overflow: hidden; text-overflow: ellipsis; }
  .val { height: 18px; min-width: 52px; background: none; border: 0; padding: 0 4px; border-radius: 4px; font-family: var(--font-dot); font-size: 14px; color: var(--fg); line-height: 18px; cursor: text; white-space: nowrap; max-width: 100%; overflow: hidden; text-overflow: ellipsis; font-variant-numeric: tabular-nums; text-align: center; }
  .compact .label { font-size: 10px; }
  .compact .val { font-size: 12.5px; min-width: 44px; }
  .row .val { min-width: 58px; text-align: left; }
  .val:hover { background: var(--surf-3); }
  .u { font-size: 10px; color: var(--dim); margin-left: 2px; font-family: var(--font-ui); }
  .edit { width: 60px; text-align: center; background: var(--surf); border: 1px solid var(--c); font-family: var(--font-ui); font-size: 13px; }
  .disabled { opacity: 0.38; }
  .disabled .dial { cursor: default; }
  .pick { position: absolute; top: calc(var(--s) + 38px); left: 50%; transform: translateX(-50%); z-index: 30; min-width: 120px; max-height: 260px;
          background: var(--surf-2); border-radius: var(--r-m); box-shadow: var(--shadow-pop); padding: 4px; display: flex; flex-direction: column; }
  .pick button { text-align: left; background: none; border: 0; padding: 5px 9px; border-radius: 5px; font-family: var(--font-dot); font-size: 13px; color: var(--fg-2); white-space: nowrap; }
  .pick button:hover { background: var(--surf-4); color: var(--fg); }
  .pick button.on { color: var(--c); background: color-mix(in srgb, var(--c) 14%, transparent); }
</style>
