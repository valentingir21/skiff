<script lang="ts">
  // The presets: instant search, this engine or all of them, the bank's categories (LD BS PA PL ST DR FX PC KT and the
  // classic ones), favourites (kept here and, when the FM-1 has them, on the FM-1 too), ↑ ↓ to audition one by one
  import { device } from "../../lib/model/device.svelte";
  import { CATEGORIES, T, shownEngines } from "../../lib/model/tables";
  import { req } from "../../lib/legacy/proto.js";
  import { prefs } from "../../lib/store";
  import { t, i18n } from "../../lib/i18n/index.svelte";
  interface Props { k: number; color: string }
  let { k, color }: Props = $props();

  let q = $state("");
  let scope = $state<"engine" | "all">("engine");
  let cat = $state<string>("*");
  let favOnly = $state(false);
  let favs = $state<Record<string, boolean>>(prefs.get("favs", {}));
  let list: HTMLUListElement | undefined = $state();

  const tr = $derived(device.tracks[k]);
  const light = (e: number) => !!device.info && !!(device.info.light.tracks & (1 << k)) && !!(device.info.light.engines & (1 << e));
  interface Row { e: number; p: number; name: string; cat: string; eng: string; key: string }
  const all = $derived.by((): Row[] => {
    const out: Row[] = [];
    for (const e of shownEngines()) {
      if (light(e)) continue;
      (device.names[e] || []).forEach((name, p) => out.push({ e, p, name, cat: T.ENG[e]?.presets[p]?.cat ?? "", eng: device.engineName(e), key: `${e}:${p}` }));
    }
    return out;
  });
  const cats = $derived([...new Set(all.filter((r) => scope === "all" || r.e === tr?.engine).map((r) => r.cat))].sort((a, b) => (a === "" ? 1 : b === "" ? -1 : a.localeCompare(b))));
  const rows = $derived.by(() => {
    const w = q.toLowerCase().split(/\s+/).filter(Boolean);
    return all.filter((r) => (scope === "all" || w.length > 0 || r.e === tr?.engine) && (cat === "*" || r.cat === cat) && (!favOnly || favs[r.key]) &&
      w.every((x) => (r.name + " " + r.eng).toLowerCase().includes(x)));
  });
  const curKey = $derived(tr ? `${tr.engine}:${tr.preset}` : "");
  $effect(() => { if (!cats.includes(cat) && cat !== "*") cat = "*"; });

  function load(r: Row) { device.loadPreset(k, r.e, r.p); }
  function fav(r: Row) {
    favs[r.key] = !favs[r.key];
    if (!favs[r.key]) delete favs[r.key];
    prefs.set("favs", favs);
    if (device.info && device.info.uiCaps & 8) device.rq(req.favSet(r.e, r.p, !!favs[r.key])).catch(() => {});
  }
  function key(e: KeyboardEvent) {
    if (e.key !== "ArrowDown" && e.key !== "ArrowUp") return;
    e.preventDefault();
    const i = rows.findIndex((r) => r.key === curKey);
    const n = rows[Math.max(0, Math.min(rows.length - 1, i + (e.key === "ArrowDown" ? 1 : -1)))];
    if (n) { load(n); queueMicrotask(() => list?.querySelector(`[data-k="${n.key}"]`)?.scrollIntoView({ block: "nearest" })); }
  }
  const catLabel = (c: string) => (CATEGORIES[c] ? CATEGORIES[c][i18n.lang] : c);
</script>

<div class="pb" style="--c:{color}" role="region" aria-label={t("sound.presets")}>
  <div class="search">
    <svg viewBox="0 0 16 16" aria-hidden="true"><circle cx="7" cy="7" r="4.5" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M10.5 10.5L14 14" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/></svg>
    <input bind:value={q} placeholder={t("sound.search")} onkeydown={key} aria-label={t("sound.search")} spellcheck="false" />
  </div>
  <div class="row">
    <div class="seg" role="radiogroup">
      <button role="radio" aria-checked={scope === "engine"} class:on={scope === "engine"} onclick={() => (scope = "engine")}>{t("sound.thisEngine")}</button>
      <button role="radio" aria-checked={scope === "all"} class:on={scope === "all"} onclick={() => (scope = "all")}>{t("sound.allEngines")}</button>
    </div>
    <button class="star" class:on={favOnly} aria-pressed={favOnly} title={t("sound.favorites")} onclick={() => (favOnly = !favOnly)}>★</button>
  </div>
  <div class="cats">
    <button class:on={cat === "*"} onclick={() => (cat = "*")}>{t("sound.all")}</button>
    {#each cats as c}<button class:on={cat === c} onclick={() => (cat = c)} title={catLabel(c)}>{c || catLabel(c)}</button>{/each}
  </div>
  <ul class="list scroll" bind:this={list} tabindex="0" onkeydown={key} role="listbox" aria-label={t("sound.presets")}>
    {#each rows as r (r.key)}
      <li data-k={r.key} role="option" aria-selected={r.key === curKey} class:on={r.key === curKey}>
        <button class="nm" onclick={() => load(r)}>
          <span class="n dot">{r.name}</span>
          {#if scope === "all" || q}<span class="e">{r.eng}</span>{/if}
        </button>
        {#if r.cat}<span class="c">{r.cat}</span>{/if}
        <button class="fv" class:on={favs[r.key]} aria-pressed={!!favs[r.key]} aria-label={t("sound.favorites")} onclick={() => fav(r)}>★</button>
      </li>
    {:else}
      <li class="none">{t("sound.noResults", { q })}</li>
    {/each}
  </ul>
  <div class="foot">{rows.length} · {t("sound.audition")}</div>
</div>

<style>
  .pb { display: flex; flex-direction: column; gap: 8px; min-height: 0; height: 100%; }
  .search { position: relative; }
  .search svg { position: absolute; left: 11px; top: 10px; width: 15px; height: 15px; color: var(--dim); }
  .search input { width: 100%; height: 36px; border-radius: var(--r-m); border: 1px solid var(--line-2); background: var(--bg); padding: 0 12px 0 34px; }
  .search input:focus { border-color: var(--c); }
  .row { display: flex; gap: 6px; }
  .seg { flex: 1; display: flex; background: var(--bg); border: 1px solid var(--line-2); border-radius: var(--r-m); padding: 2px; }
  .seg button { flex: 1; border: 0; background: none; color: var(--dim); font-size: 12px; font-weight: 600; border-radius: 6px; height: 26px; }
  .seg button.on { background: var(--surf-4); color: var(--fg); }
  .star { width: 34px; border-radius: var(--r-m); border: 1px solid var(--line-2); background: var(--bg); color: var(--dim); }
  .star.on { color: var(--warn); border-color: var(--warn); }
  .cats { display: flex; flex-wrap: wrap; gap: 4px; }
  .cats button { border: 1px solid var(--line-2); background: none; color: var(--dim); border-radius: 999px; padding: 2px 9px; font-size: 11.5px; font-weight: 700; }
  .cats button:hover { color: var(--fg); }
  .cats button.on { background: var(--c); border-color: var(--c); color: #0b0b0b; }
  .list { list-style: none; margin: 0; padding: 0; flex: 1; min-height: 120px; border-radius: var(--r-m); }
  .list:focus-visible { box-shadow: var(--focus); }
  li { display: flex; align-items: center; gap: 4px; border-radius: 7px; padding-right: 4px; }
  li:hover { background: var(--surf-2); }
  li.on { background: color-mix(in srgb, var(--c) 16%, var(--surf-2)); }
  .nm { flex: 1; min-width: 0; display: flex; align-items: baseline; gap: 8px; background: none; border: 0; padding: 7px 8px; text-align: left; }
  .n { font-size: 14px; color: var(--fg-2); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  li.on .n { color: var(--c); }
  .e { font-size: 10.5px; color: var(--dim); font-weight: 700; }
  .c { font-size: 10px; font-weight: 800; color: var(--dim); border: 1px solid var(--line-2); border-radius: 4px; padding: 0 4px; }
  .fv { background: none; border: 0; color: var(--surf-4); font-size: 14px; width: 24px; }
  li:hover .fv { color: var(--mute); }
  .fv.on, li:hover .fv.on { color: var(--warn); }
  .none { padding: 12px; color: var(--dim); }
  .foot { font-size: 11px; color: var(--mute); }
</style>
