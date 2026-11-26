<script lang="ts">
  import { ui } from "../../lib/ui.svelte";
</script>
<div class="toasts" role="status" aria-live="polite">
  {#each ui.toasts as tst (tst.id)}
    <div class="toast {tst.level}">
      <span class="dot" aria-hidden="true"></span>
      <span class="txt">{tst.text}</span>
      {#if tst.action}<button onclick={() => { tst.action!.run(); ui.toasts = ui.toasts.filter((x) => x.id !== tst.id); }}>{tst.action.label}</button>{/if}
      <button class="x" aria-label="Close" onclick={() => (ui.toasts = ui.toasts.filter((x) => x.id !== tst.id))}>×</button>
    </div>
  {/each}
</div>
<style>
  .toasts { position: fixed; bottom: 18px; left: 50%; transform: translateX(-50%); display: flex; flex-direction: column; gap: 8px; z-index: 100; pointer-events: none; }
  .toast { pointer-events: auto; display: flex; align-items: center; gap: 10px; min-width: 300px; max-width: 560px; padding: 10px 10px 10px 14px; border-radius: var(--r-l);
           background: var(--surf-3); box-shadow: var(--shadow-pop); font-size: 13px; animation: in var(--t-med) var(--ease); }
  @keyframes in { from { opacity: 0; transform: translateY(8px); } }
  .dot { width: 8px; height: 8px; border-radius: 50%; flex: none; background: var(--info); }
  .ok .dot { background: var(--ok); } .warn .dot { background: var(--warn); } .err .dot { background: var(--err); }
  .txt { flex: 1; color: var(--fg); }
  button { background: var(--surf-4); border: 0; border-radius: 6px; padding: 4px 10px; font-weight: 600; font-size: 12px; }
  .x { background: none; color: var(--dim); font-size: 18px; padding: 0 6px; }
</style>
