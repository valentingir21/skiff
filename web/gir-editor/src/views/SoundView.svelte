<script lang="ts">
  // The selected track's sound: its engine (the 15, by the EDIT layer's keys) and preset, then every page of the FM-1
  // the track has, grouped by the device's families, knob names as on its screen; the presets beside.
  import { device } from "../lib/model/device.svelte";
  import { EDIT_KEYS, GIR_ENGINES, HELP, helpText, pagesFor, shownEngines, T, type Page } from "../lib/model/tables";
  import { ui, go, TRACK_COLORS } from "../lib/ui.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import { ic, FAM_ICON } from "../ui/icons";
  import PageCard from "./sound/PageCard.svelte";
  import ModCard from "./sound/ModCard.svelte";
  import LanesCard from "./sound/LanesCard.svelte";
  import KitCard from "./sound/KitCard.svelte";
  import EnginePicker from "./sound/EnginePicker.svelte";
  import PresetBrowser from "./sound/PresetBrowser.svelte";

  const k = $derived(device.sel);
  const tr = $derived(device.tracks[k]);
  const e = $derived(tr?.engine ?? 0);
  const name = $derived(device.engineName(e));
  const color = $derived(TRACK_COLORS[k]);
  const help = $derived(HELP.engines[name]);
  const key = $derived(EDIT_KEYS[shownEngines().indexOf(e)] ?? "");
  let picking = $state(false);

  const ORDER = ["EDIT", "ENV", "LFO", "FX", "SCL", "ARP"];
  type Item = { kind: "page"; page: Page } | { kind: "mod"; page: Page } | { kind: "lanes" } | { kind: "kit" } | { kind: "fm6" } | { kind: "samples" };
  const groups = $derived.by(() => {
    const out: { fam: string; items: Item[] }[] = [];
    const pages = pagesFor(e, { usrSamples: true });
    for (const fam of ORDER) {
      const items: Item[] = [];
      let lanes = false, fm6 = false;
      for (const pg of pages.filter((p) => p.fam === fam)) {
        if (pg.title === "MOD") items.push({ kind: "mod", page: pg });
        else if (pg.title === "LANES" || pg.title === "LANE MIX") { if (!lanes) items.push({ kind: "lanes" }); lanes = true; }
        else if (pg.title === "SOUND") items.push({ kind: "kit" });
        else if (pg.title === "OPERATOR" || pg.title === "ALL OPS") { if (!fm6) items.push({ kind: "fm6" }); fm6 = true; }
        else if (pg.title === "SAMPLES") { if (device.engineName(e) === "SAMPLE" && (tr?.p[T.P_E0] ?? 0) >= 5) items.push({ kind: "samples" }); }
        else if (pg.ids.some((x) => x >= 0)) items.push({ kind: "page", page: pg });
      }
      if (items.length) out.push({ fam, items });
    }
    return out;
  });
  const famTitle = (f: string) => t("sound.page." + f);
  function step(d: number) {
    if (!tr) return;
    const n = device.names[e]?.length ?? 0;
    if (n) device.loadPreset(k, e, (tr.preset + d + n) % n);
  }
</script>

<div class="sound" style="--c:{color}">
  <div class="mainc">
    <section class="head">
      <button class="engine" onclick={() => (picking = true)} title={t("sound.changeEngine")}>
        <span class="lbl">{t("sound.engine")} <span class="key">{t("sound.keyOnFm1", { key })}</span></span>
        <span class="en dot">{name}{#if GIR_ENGINES.has(name)}<em>Gir</em>{/if}</span>
        <span class="pr">{helpText(help?.principle)}</span>
        <span class="chev" aria-hidden="true">▾</span>
      </button>
      <div class="preset">
        <span class="lbl">{t("sound.preset")}</span>
        <div class="pn">
          <button class="arr" onclick={() => step(-1)} aria-label="Previous preset">‹</button>
          <span class="pv dot">{tr?.name || "—"}</span>
          <button class="arr" onclick={() => step(1)} aria-label="Next preset">›</button>
        </div>
        <span class="pr">{helpText(help?.uses)}</span>
      </div>
    </section>

    {#each groups as g}
      <section class="fam" aria-label={famTitle(g.fam)}>
        <h2><i class="ic" aria-hidden="true">{ic(FAM_ICON[g.fam] ?? "ui_knob")}</i>{famTitle(g.fam)}</h2>
        <div class="cards">
          {#each g.items as it}
            {#if it.kind === "page"}<PageCard page={it.page} {k} {color} />
            {:else if it.kind === "mod"}<ModCard page={it.page} {k} {color} />
            {:else if it.kind === "lanes"}<LanesCard {k} {color} />
            {:else if it.kind === "kit"}<KitCard {color} />
            {:else if it.kind === "fm6"}
              <article class="link"><h3 class="dot">OPERATOR · ALL OPS</h3><p>{t("sound.specialPage")}</p>
                <button onclick={() => go("fm6")}>{t("sound.openFm6")}</button></article>
            {:else if it.kind === "samples"}
              <article class="link"><h3 class="dot">SAMPLES</h3><p>{t("sound.usrSamples")}</p>
                <button onclick={() => go("samples")}>{t("view.samples")}</button></article>
            {/if}
          {/each}
        </div>
      </section>
    {/each}
  </div>

  <aside class="presets">
    <PresetBrowser {k} {color} />
  </aside>
</div>

{#if picking}<EnginePicker {k} {color} onclose={() => (picking = false)} />{/if}

<style>
  .sound { display: grid; grid-template-columns: 1fr 300px; gap: 24px; align-items: start; }
  .mainc { min-width: 0; display: flex; flex-direction: column; gap: 22px; }
  .head { display: grid; grid-template-columns: 1.15fr 1fr; gap: 12px; }
  .engine, .preset { position: relative; text-align: left; display: flex; flex-direction: column; gap: 4px; padding: 16px 18px; border-radius: var(--r-xl); background: var(--surf); border: 1px solid var(--line); }
  .engine { cursor: pointer; background: linear-gradient(135deg, color-mix(in srgb, var(--c) 16%, var(--surf)), var(--surf) 60%); border-color: color-mix(in srgb, var(--c) 35%, var(--line)); }
  .engine:hover { border-color: var(--c); }
  .lbl { font-size: 11.5px; font-weight: 600; color: var(--dim); display: flex; gap: 8px; }
  .key { color: var(--mute); font-weight: 500; }
  .en { font-size: 34px; line-height: 1.05; color: var(--c); display: flex; align-items: center; gap: 10px; }
  .en em { font-style: normal; font-family: var(--font-ui); font-size: 11px; font-weight: 700; border: 1px solid; border-radius: 5px; padding: 1px 6px; }
  .pr { font-size: 12.5px; color: var(--fg-2); line-height: 1.4; }
  .chev { position: absolute; right: 16px; top: 16px; color: var(--dim); }
  .pn { display: flex; align-items: center; gap: 8px; }
  .pv { font-size: 30px; line-height: 1.1; flex: 1; min-width: 0; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .arr { width: 30px; height: 30px; border-radius: 8px; border: 1px solid var(--line-2); background: var(--surf-2); font-size: 18px; color: var(--fg-2); }
  .arr:hover { color: var(--fg); border-color: var(--c); }
  .fam h2 { display: flex; align-items: center; gap: 8px; margin: 0 0 10px; font-size: 13px; font-weight: 700; color: var(--fg-2); }
  .fam h2 .ic { color: var(--c); font-size: 15px; }
  .cards { display: grid; grid-template-columns: repeat(auto-fill, minmax(286px, 1fr)); gap: 10px; }
  .link { background: var(--surf); border: 1px dashed var(--line-2); border-radius: var(--r-l); padding: 12px; display: flex; flex-direction: column; gap: 8px; }
  .link h3 { margin: 0; font-size: 15px; font-weight: 400; }
  .link p { margin: 0; color: var(--dim); font-size: 12.5px; flex: 1; }
  .link button { align-self: flex-start; background: var(--surf-3); border: 1px solid var(--line-2); border-radius: 7px; padding: 5px 10px; font-weight: 600; }
  .presets { position: sticky; top: 0; height: calc(100vh - 200px); min-height: 420px; }
  @media (max-width: 1320px) { .sound { grid-template-columns: 1fr 260px; } }
  @media (max-width: 1100px) { .sound { grid-template-columns: 1fr; } .presets { position: static; height: 480px; } .head { grid-template-columns: 1fr; } }
</style>
