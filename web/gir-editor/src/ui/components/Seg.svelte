<script lang="ts">
  // a segmented control: one of a few options, all visible
  interface Props { options: { value: string | number; label: string; title?: string }[]; value: string | number; onchange: (v: any) => void; label?: string; size?: "s" | "m"; color?: string }
  let { options, value, onchange, label = "", size = "m", color = "var(--accent)" }: Props = $props();
</script>
<div class="seg {size}" role="radiogroup" aria-label={label} style="--c:{color}">
  {#each options as o}
    <button role="radio" aria-checked={o.value === value} class:on={o.value === value} title={o.title} onclick={() => onchange(o.value)}>{o.label}</button>
  {/each}
</div>
<style>
  .seg { display: inline-flex; background: var(--surf); border: 1px solid var(--line-2); border-radius: var(--r-m); padding: 2px; gap: 2px; }
  button { border: 0; background: none; color: var(--dim); font-weight: 600; border-radius: 6px; padding: 0 11px; transition: background var(--t-fast), color var(--t-fast); }
  .m button { height: 28px; font-size: 12.5px; }
  .s button { height: 24px; font-size: 12px; padding: 0 9px; }
  button:hover { color: var(--fg); }
  button.on { background: var(--surf-4); color: var(--fg); box-shadow: inset 0 -2px 0 var(--c); }
</style>
