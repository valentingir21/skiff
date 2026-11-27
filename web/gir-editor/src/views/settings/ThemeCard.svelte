<script lang="ts">
  // (rc2) The FM-1's display: THEME, FONT, MONITOR (as the unit keeps them), the editor following the THEME's colours,
  // and THEME CUSTOM's eleven colours with a preview (the original's COLORS editor): shown on the unit and here while
  // they change, kept by Save, dropped by Revert, the set taken away by Remove (asked first). Colours hard to read get
  // a warning, nothing more: the unit keeps what it is given.
  import { onDestroy, onMount } from "svelte";
  import { device } from "../../lib/model/device.svelte";
  import { t } from "../../lib/i18n/index.svelte";
  import { ask, toastKey } from "../../lib/ui.svelte";
  import { from565hex } from "../../lib/legacy/proto.js";
  import { theme, readPrefs, setPref, setFollow, showColors, colorsOp, ROLES, BUILTIN, hex565, colorWarnings, current, isLight } from "../../lib/theme.svelte";
  import Button from "../../ui/components/Button.svelte";

  let poll: ReturnType<typeof setInterval> | undefined;
  onMount(() => { readPrefs(true).catch(() => {}); poll = setInterval(() => { if (device.ready && !editing) readPrefs().catch(() => {}); }, 2000); });
  onDestroy(() => { clearInterval(poll); if (theme.edit) { theme.edit = null; revert(); } });

  const p = $derived(theme.prefs);
  const caps = $derived(p?.state.caps ?? 0);
  const inUse = $derived(!!p && p.palettes[p.state.palette] === "CUSTOM");
  let c = $state<number[]>([]);
  let look = $state(0);
  let live = $state(true);
  let editing = $state(false);
  let key = "";
  $effect(() => {                              // the unit's set (none: the THEME in use's colours to start from)
    const C = p?.colors;
    if (!p || !C) return;
    const from = inUse ? C.look : Math.min(p.state.palette, BUILTIN.length - 1);
    const k = JSON.stringify([C.stored, C.look, C.c, from]);
    if (k === key || editing) return;
    key = k;
    c = (C.stored ? C.c : BUILTIN[from].c).slice();
    look = C.stored ? C.look : from;
  });
  const warns = $derived(c.length ? colorWarnings(c) : []);
  function set(i: number, hex: string) { c[i] = from565hex(hex); editing = true; showColors(look, c, live); }
  function startFrom(i: number) { c = BUILTIN[i].c.slice(); look = i; editing = true; showColors(look, c, live); }
  async function save() {
    try { const rc = await colorsOp(1, look, c); editing = false; key = ""; toastKey(rc ? "warn" : "ok", rc === 3 ? "theme.unsaved" : rc ? "theme.refused" : "theme.saved"); }
    catch (e) { toastKey("err", "toast.error", { detail: String((e as Error).message) }); }
  }
  async function revert() { try { await colorsOp(3, look, c); } catch { /* the unit's own stays */ } editing = false; key = ""; }
  async function remove() {
    if (!(await ask({ title: t("theme.remove"), body: t("theme.removeBody"), ok: t("theme.remove"), danger: true }))) return;
    try { await colorsOp(2, look, c); editing = false; key = ""; toastKey("ok", "theme.removed"); } catch (e) { toastKey("err", "toast.error", { detail: String((e as Error).message) }); }
  }
  async function pref(id: number, v: number) {
    try { const rc = await setPref(id, v); if (rc === 3) toastKey("warn", "theme.unsaved"); else if (rc === 4) toastKey("info", "theme.pending"); else if (rc) toastKey("err", "theme.refused"); }
    catch (e) { toastKey("err", "toast.error", { detail: String((e as Error).message) }); }
  }
  const hx = (v: number) => hex565(v);
  const shown = $derived(theme.edit ?? current() ?? BUILTIN[0].c);
  const light = $derived(isLight(shown));
</script>

<section class="card wide" data-testid="theme">
  <h2>{t("theme.title")}</h2>
  {#if !p}
    <p class="muted">{t("theme.none")}</p>
  {:else}
    <div class="top">
      <div class="prefs">
        {#if caps & 1}
          <label class="item"><span class="nm">{t("theme.theme")}</span>
            <select value={p.state.palette} onchange={(e) => pref(0, +(e.currentTarget as HTMLSelectElement).value)} aria-label={t("theme.theme")}>
              {#each p.palettes as n, i}<option value={i}>{n}</option>{/each}
            </select></label>
        {/if}
        {#if caps & 2}
          <label class="item"><span class="nm">{t("theme.font")}</span>
            <select value={p.state.font} onchange={(e) => pref(1, +(e.currentTarget as HTMLSelectElement).value)} aria-label={t("theme.font")}>
              <option value={0}>{t("theme.regular")}</option><option value={1}>{t("theme.bold")}</option>
            </select></label>
        {/if}
        {#if caps & 4}
          <label class="item"><span class="nm">{t("theme.monitor")}</span>
            <select value={p.state.monitor} onchange={(e) => pref(2, +(e.currentTarget as HTMLSelectElement).value)} aria-label={t("theme.monitor")}>
              <option value={0}>{t("theme.off")}</option><option value={1}>{t("theme.events")}</option><option value={2}>{t("theme.notes")}</option>
            </select></label>
        {/if}
        <label class="item"><span class="nm">{t("theme.follow")}</span><input type="checkbox" checked={theme.follow} onchange={(e) => setFollow((e.currentTarget as HTMLInputElement).checked)} /></label>
        <p class="hint">{light ? t("theme.lightNote") : t("theme.followHelp")}</p>
      </div>
      <!-- the preview: the FM-1's screen in these colours (a page, its knobs, the six tracks) -->
      <div class="pv" aria-label={t("theme.preview")} role="img" style="--b:{hx(shown[0])}; --s:{hx(shown[1])}; --x:{hx(shown[2])}; --h:{hx(shown[3])}; --a:{hx(shown[4])}">
        <div class="pvh"><span>T1 SUPERSAW</span><span>120</span></div>
        <div class="pvt">ST SS STAB</div>
        <div class="pvk">
          {#each ["CUTOFF", "RESO", "ENV", "DRIVE"] as n, i}
            <div class="kn"><i style="--p:{[70, 35, 55, 20][i]}"></i><span>{n}</span></div>
          {/each}
        </div>
        <div class="pvl">{#each [5, 6, 7, 8, 9, 10] as r, i}<b style="background:{hx(shown[r])}">{i + 1}</b>{/each}</div>
      </div>
    </div>

    {#if caps & 16 && p.colors}
      <div class="custom">
        <h3>{t("theme.custom")}{#if inUse} · {t("theme.inUse")}{/if}</h3>
        <p class="hint">{t("theme.customHelp")}</p>
        <div class="wells">
          {#each ROLES as role, i}
            <label class="well"><input type="color" value={hx(c[i] ?? 0)} aria-label="{role} {t('theme.colour')}" oninput={(e) => set(i, (e.currentTarget as HTMLInputElement).value)} /><span class="dot">{role}</span></label>
          {/each}
        </div>
        <div class="row">
          <label>{t("theme.look")}
            <select bind:value={look} onchange={() => { editing = true; showColors(look, c, live); }}>{#each BUILTIN as b, i}<option value={i}>{b.name}</option>{/each}</select></label>
          <label>{t("theme.from")}
            <select value="" onchange={(e) => { const v = (e.currentTarget as HTMLSelectElement).value; if (v !== "") startFrom(+v); (e.currentTarget as HTMLSelectElement).value = ""; }}>
              <option value="">—</option>{#each BUILTIN as b, i}<option value={i}>{b.name}</option>{/each}</select></label>
          <label><input type="checkbox" bind:checked={live} onchange={() => { if (live) showColors(look, c, true); }} /> {t("theme.live")}</label>
        </div>
        {#if warns.length}<p class="warn" role="status">{t("theme.hard")} {warns.map((x: any) => `${x.a} / ${x.b} ${x.r.toFixed(1)}:1`).join(", ")}</p>{/if}
        <div class="row">
          <Button size="s" variant="primary" onclick={save}>{t("theme.save")}</Button>
          <Button size="s" onclick={revert} disabled={!editing}>{t("theme.revert")}</Button>
          <Button size="s" variant="ghost" onclick={remove} disabled={!p.colors.stored}>{t("theme.remove")}</Button>
        </div>
      </div>
    {/if}
  {/if}
</section>

<style>
  .card { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 16px; display: flex; flex-direction: column; gap: 12px; grid-column: span 2; }
  h2 { margin: 0; font-size: 13px; color: var(--fg-2); }
  h3 { margin: 0; font-size: 13px; font-weight: 600; color: var(--fg); }
  .top { display: grid; grid-template-columns: minmax(260px, 1fr) 260px; gap: 24px; align-items: start; }
  .prefs { display: flex; flex-direction: column; }
  .item { display: flex; align-items: center; justify-content: space-between; gap: 10px; padding: 6px 0; border-bottom: 1px solid var(--line); }
  .nm { font-size: 13.5px; }
  select { height: 28px; border-radius: 6px; border: 1px solid var(--line-2); background: var(--bg); padding: 0 8px; color: var(--fg); }
  .hint { margin: 6px 0 0; color: var(--dim); font-size: 12.5px; line-height: 1.45; }
  .muted { color: var(--dim); margin: 0; }
  .pv { width: 240px; height: 240px; background: var(--b); color: var(--x); border-radius: 10px; padding: 10px; display: flex; flex-direction: column; gap: 10px; font-family: var(--font-dot); box-shadow: 0 0 0 6px #18181b, 0 0 0 7px #2a2a2f; }
  .pvh { display: flex; justify-content: space-between; background: var(--h); color: var(--b); padding: 3px 8px; border-radius: 4px; font-size: 13px; }
  .pvt { font-size: 22px; text-align: center; }
  .pvk { display: grid; grid-template-columns: repeat(4, 1fr); gap: 4px; background: var(--s); border-radius: 6px; padding: 8px 4px; }
  .kn { display: flex; flex-direction: column; align-items: center; gap: 4px; font-size: 9px; }
  .kn i { width: 30px; height: 30px; border-radius: 50%; background: conic-gradient(from 225deg, var(--a) calc(var(--p) * 2.7deg), color-mix(in srgb, var(--x) 18%, transparent) 0 270deg, transparent 0); mask: radial-gradient(circle, transparent 9px, #000 10px); }
  .pvl { display: grid; grid-template-columns: repeat(6, 1fr); gap: 4px; margin-top: auto; }
  .pvl b { height: 26px; border-radius: 4px; display: grid; place-items: center; font-weight: 400; font-size: 13px; color: var(--b); }
  .custom { display: flex; flex-direction: column; gap: 10px; border-top: 1px solid var(--line); padding-top: 12px; }
  .wells { display: grid; grid-template-columns: repeat(auto-fill, minmax(84px, 1fr)); gap: 8px; }
  .well { display: flex; flex-direction: column; align-items: center; gap: 4px; font-size: 12px; color: var(--fg-2); }
  .well input { width: 100%; height: 34px; border: 1px solid var(--line-2); border-radius: 7px; background: none; padding: 2px; cursor: pointer; }
  .row { display: flex; gap: 14px; align-items: center; flex-wrap: wrap; font-size: 12.5px; color: var(--fg-2); }
  .row select { margin-left: 6px; }
  .warn { margin: 0; color: var(--warn); font-size: 12.5px; }
  @media (max-width: 1000px) { .card { grid-column: auto; } .top { grid-template-columns: 1fr; } }
</style>
