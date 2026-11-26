<script lang="ts">
  interface Props { on: boolean; label: string; color?: string; onchange: (on: boolean) => void; title?: string; size?: "s" | "m" }
  let { on, label, color = "var(--accent)", onchange, title, size = "m" }: Props = $props();
</script>
<button class="tg {size}" class:on aria-pressed={on} {title} style="--c:{color}" onclick={() => onchange(!on)}>{label}</button>
<style>
  .tg { border: 1px solid var(--line-2); background: var(--surf-3); color: var(--dim); font-weight: 700; border-radius: 6px; transition: all var(--t-fast); }
  .m { height: 26px; min-width: 30px; padding: 0 8px; font-size: 11.5px; }
  .s { height: 22px; min-width: 24px; padding: 0 6px; font-size: 10.5px; }
  .tg:hover { color: var(--fg); }
  .tg.on { background: var(--c); border-color: var(--c); color: #0b0b0b; }
</style>
