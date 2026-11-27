<script lang="ts" generics="T">
  // (rc3) cards on the app's grid: columns as wide as a page of 4 knobs (340 px at least), every row full; the last
  // card of a short row stretched (it shows its help beside its knobs). As the Sound view's families.
  import type { Snippet } from "svelte";
  import { pack } from "../lib/layout.svelte";
  interface Props { items: T[]; span?: (x: T) => number; card: Snippet<[T, boolean]> }
  let { items, span = () => 1, card }: Props = $props();
  let w = $state(900);
  const GAP = 10, MIN = 340;
  const cols = $derived(Math.max(1, Math.floor((w + GAP) / (MIN + GAP))));
  const colw = $derived((w - GAP * (cols - 1)) / cols);
</script>
<div class="cards" bind:clientWidth={w} style="--cols:{cols}; --colw:{colw}px">
  {#each pack(items, span, cols) as p}
    <div class="cell" style="grid-column: span {p.span}">{@render card(p.item, p.stretched)}</div>
  {/each}
</div>
<style>
  .cards { display: grid; grid-template-columns: repeat(var(--cols), minmax(0, 1fr)); gap: 10px; }
  .cell { min-width: 0; display: flex; flex-direction: column; }
  .cell > :global(*) { flex: 1; }
</style>
