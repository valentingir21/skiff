<script lang="ts">
  // Cmd/Ctrl + K: go anywhere, do anything, load any preset, by typing a few letters
  import { device } from "../lib/model/device.svelte";
  import { ui, go, VIEWS } from "../lib/ui.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import { shownEngines } from "../lib/model/tables";

  interface Item { label: string; hint?: string; run: () => void; color?: string }
  let q = $state("");
  let idx = $state(0);
  let input: HTMLInputElement | undefined = $state();

  const items = $derived.by((): Item[] => {
    const out: Item[] = [];
    for (const v of VIEWS) out.push({ label: t("cmd.goto", { v: t("view." + v) }), run: () => go(v) });
    if (device.ready) {
      device.tracks.forEach((_, k) => out.push({ label: t("cmd.track", { n: k + 1 }), hint: String(k + 1), run: () => device.selectTrack(k), color: `var(--t${k + 1})` }));
      out.push({ label: t("top.play"), hint: "Space", run: () => device.play() }, { label: t("top.stop"), run: () => device.stop() },
               { label: t("top.rec"), run: () => device.rec() }, { label: t("top.undo"), hint: "⌘Z", run: () => device.undo() },
               { label: t("top.redo"), hint: "⇧⌘Z", run: () => device.redo() }, { label: t("top.help"), hint: "?", run: () => (ui.help = true) });
      const k = device.sel;
      for (const e of shownEngines()) {
        const en = device.engineName(e);
        out.push({ label: `${t("sound.changeEngine")}: ${en}`, hint: `T${k + 1}`, run: () => device.setEngine(k, e), color: `var(--t${k + 1})` });
        (device.names[e] || []).forEach((p, i) => out.push({ label: t("cmd.preset", { p, n: k + 1 }), hint: en, run: () => device.loadPreset(k, e, i), color: `var(--t${k + 1})` }));
      }
    }
    return out;
  });
  const shown = $derived.by(() => {
    const words = q.toLowerCase().split(/\s+/).filter(Boolean);
    const f = words.length ? items.filter((it) => words.every((w) => (it.label + " " + (it.hint ?? "")).toLowerCase().includes(w))) : items.slice(0, 40);
    return f.slice(0, 60);
  });
  $effect(() => { if (ui.palette) { q = ""; idx = 0; queueMicrotask(() => input?.focus()); } });
  $effect(() => { q; idx = 0; });
  function run(it: Item | undefined) { if (!it) return; ui.palette = false; it.run(); }
  function key(e: KeyboardEvent) {
    if (e.key === "ArrowDown") { idx = Math.min(shown.length - 1, idx + 1); e.preventDefault(); }
    else if (e.key === "ArrowUp") { idx = Math.max(0, idx - 1); e.preventDefault(); }
    else if (e.key === "Enter") { run(shown[idx]); e.preventDefault(); }
    else if (e.key === "Escape") ui.palette = false;
    queueMicrotask(() => document.querySelector(".pal li.on")?.scrollIntoView({ block: "nearest" }));
  }
</script>

{#if ui.palette}
  <div class="scrim" role="presentation" onclick={() => (ui.palette = false)}></div>
  <div class="pal" role="dialog" aria-modal="true" aria-label={t("top.commands")}>
    <input bind:this={input} bind:value={q} placeholder={t("cmd.placeholder")} onkeydown={key} aria-label={t("cmd.placeholder")} spellcheck="false" />
    <ul class="scroll" role="listbox">
      {#each shown as it, i}
        <li role="option" aria-selected={i === idx} class:on={i === idx} onmousemove={() => (idx = i)} onclick={() => run(it)} onkeydown={() => {}}>
          {#if it.color}<span class="sw" style="background:{it.color}"></span>{/if}
          <span class="l">{it.label}</span>{#if it.hint}<span class="h">{it.hint}</span>{/if}
        </li>
      {:else}
        <li class="none">{t("cmd.none")}</li>
      {/each}
    </ul>
  </div>
{/if}

<style>
  .scrim { position: fixed; inset: 0; background: rgb(0 0 0 / 45%); z-index: 95; }
  .pal { position: fixed; top: 12%; left: 50%; transform: translateX(-50%); width: min(620px, 94vw); z-index: 96; background: var(--surf-2); border-radius: var(--r-xl); box-shadow: var(--shadow-pop); overflow: hidden; animation: p var(--t-med) var(--ease); }
  @keyframes p { from { opacity: 0; transform: translate(-50%, -6px); } }
  input { width: 100%; height: 54px; border: 0; border-bottom: 1px solid var(--line-2); background: transparent; padding: 0 20px; font-size: 16px; }
  ul { list-style: none; margin: 0; padding: 6px; max-height: 52vh; }
  li { display: flex; align-items: center; gap: 10px; padding: 9px 12px; border-radius: 8px; cursor: pointer; }
  li.on { background: var(--surf-4); }
  .sw { width: 8px; height: 8px; border-radius: 2px; flex: none; }
  .l { flex: 1; }
  .h { color: var(--dim); font-size: 12px; }
  .none { color: var(--dim); cursor: default; }
</style>
