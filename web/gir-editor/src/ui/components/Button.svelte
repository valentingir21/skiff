<script lang="ts">
  import type { Snippet } from "svelte";
  interface Props {
    variant?: "primary" | "secondary" | "ghost" | "danger" | "track";
    size?: "s" | "m" | "l";
    icon?: string;              // a Fukiai glyph character
    title?: string;
    disabled?: boolean;
    active?: boolean;
    color?: string;
    type?: "button" | "submit";
    onclick?: (e: MouseEvent) => void;
    children?: Snippet;
    [k: string]: unknown;
  }
  let { variant = "secondary", size = "m", icon, title, disabled = false, active = false, color, type = "button", onclick, children, ...rest }: Props = $props();
</script>

<button {type} class="btn {variant} {size}" class:active {title} aria-label={title && !children ? title : undefined} {disabled} {onclick}
        style={color ? `--c:${color}` : undefined} {...rest}>
  {#if icon}<i class="ic" aria-hidden="true">{icon}</i>{/if}
  {#if children}<span>{@render children()}</span>{/if}
</button>

<style>
  .btn { --c: var(--accent); display: inline-flex; align-items: center; justify-content: center; gap: 7px; border: 1px solid transparent; border-radius: var(--r-m);
         font-weight: 600; white-space: nowrap; transition: background var(--t-fast), border-color var(--t-fast), color var(--t-fast), transform var(--t-fast); }
  .btn:active:not(:disabled) { transform: translateY(1px); }
  .btn:disabled { opacity: 0.4; cursor: default; }
  .s { height: 28px; padding: 0 10px; font-size: 12px; }
  .m { height: 34px; padding: 0 14px; font-size: 13px; }
  .l { height: 46px; padding: 0 22px; font-size: 15px; border-radius: var(--r-l); }
  .ic { font-size: 1.15em; }
  .primary { background: var(--c); color: var(--accent-ink); }
  .primary:hover:not(:disabled) { filter: brightness(1.08); }
  .secondary { background: var(--surf-3); color: var(--fg); border-color: var(--line-2); }
  .secondary:hover:not(:disabled) { background: var(--surf-4); }
  .secondary.active { border-color: var(--c); color: var(--c); background: color-mix(in srgb, var(--c) 10%, var(--surf-3)); }
  .ghost { background: transparent; color: var(--fg-2); }
  .ghost:hover:not(:disabled) { background: var(--surf-3); color: var(--fg); }
  .ghost.active { color: var(--c); background: color-mix(in srgb, var(--c) 12%, transparent); }
  .danger { background: color-mix(in srgb, var(--err) 16%, var(--surf-3)); color: #ffb3ba; border-color: color-mix(in srgb, var(--err) 40%, transparent); }
  .danger:hover:not(:disabled) { background: color-mix(in srgb, var(--err) 26%, var(--surf-3)); }
  .track { background: var(--surf-3); color: var(--fg); border-color: var(--line-2); }
  .track.active { background: var(--c); color: #000; border-color: var(--c); }
  .btn:not(:has(span)) { padding: 0; aspect-ratio: 1; }
</style>
