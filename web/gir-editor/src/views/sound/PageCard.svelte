<script lang="ts">
  // One page of the FM-1 (its title, its four knobs in the device's order), with its graph where one helps
  import { device } from "../../lib/model/device.svelte";
  import { ID, HELP, helpText, isHelp, pathOf, type Page } from "../../lib/model/tables";
  import { t } from "../../lib/i18n/index.svelte";
  import Param from "../../ui/Param.svelte";
  import AdsrGraph from "../../ui/graphs/AdsrGraph.svelte";
  import WaveGraph from "../../ui/graphs/WaveGraph.svelte";
  import RumbleGraph from "../../ui/graphs/RumbleGraph.svelte";

  interface Props { page: Page; k: number; color: string; wide?: boolean }
  let { page, k, color, wide = false }: Props = $props();
  const tr = $derived(device.tracks[k]);
  const e = $derived(tr?.engine ?? 0);
  const sub = $derived(page.title === "EDIT 1" ? device.titles[e]?.[0] : page.title === "EDIT 2" ? device.titles[e]?.[1] : "");
  const p = (id: number) => tr?.p[id] ?? 0;
  const GIR_PAGES = new Set(["RUMBLE", "RUMBLE 2", "KICK", "OSC 1", "OSC 2", "OSC 3", "MINI MOD", "EQ"]);
  const env = $derived(page.title === "ENV" || page.title === "ENV 2");
  const lfo = $derived(page.title === "LFO");
  const rumble = $derived(page.title === "RUMBLE" && page.fam === "EDIT");
  // (rc3) a card stretched alone on its row: its knobs stay in the columns of the cards above, its help beside them
  const help = $derived.by(() => {
    const fam = page.fam === "TRK" ? "GLO" : page.fam;
    const h = HELP.pages[`${fam}/${page.title}`];
    if (!h) return [];
    const eng = HELP.engines[device.engineName(e)];
    return page.ids.map((id, i) => {
      if (id < 0) return "";
      const line = (h.knobs[i] ?? "").trim();
      // a bare name (K.CLICK) says nothing: the engine's help for that knob instead, else nothing
      if (!/\s/.test(line)) {
        const p = eng?.params?.[line];
        if (p && isHelp(p)) return `${line} : ${helpText(p)}`;
        const k = "help.knob." + line;                   // the manual's tables the help generator does not read (KICK)
        return t(k) !== k ? `${line} : ${t(k)}` : "";
      }
      return isHelp(line) ? helpText(line) : "";
    }).filter(Boolean);
  });
  const waveName = $derived.by(() => { const d = device.desc(ID.P_LWAVE, e); return d?.names?.[p(ID.P_LWAVE) - d.min] ?? "SIN"; });
</script>

<article class="card" class:wide style="--c:{color}">
  <div class="inner">
  <header>
    <h3 class="dot">{page.title}</h3>
    {#if sub}<span class="sub dot">{sub}</span>{/if}
    {#if GIR_PAGES.has(page.title)}<span class="gir">Gir</span>{/if}
  </header>
  {#if env}
    <AdsrGraph a={p(page.ids[0])} d={p(page.ids[1])} s={p(page.ids[2])} r={p(page.ids[3])} {color}
               onchange={(w, v) => device.setParam(k, page.ids["adsr".indexOf(w)], v)} />
  {:else if lfo}
    <WaveGraph wave={waveName} cycles={1 + (p(ID.P_LRATE) / 127) * 5} phase={p(ID.P_LPHASE)} {color} />
  {:else if rumble}
    <RumbleGraph mix={p(ID.P_RB_MIX)} slice={p(ID.P_RB_SLICE)} decay={p(ID.P_RB_DECAY)} atk={p(ID.P_RB_ATK)} duck={p(ID.P_RB_DUCK)} tone={p(ID.P_RB_TONE)} {color} />
  {/if}
  <div class="knobs">
    {#each page.ids as id, slot}
      {#if id >= 0}
        <Param track={k} {id} scope={page.slots[slot] === "global" ? 1 : 0} {page} {slot} where={pathOf(page, slot, e)} {color}
               label={page.send && slot === 0 ? "SEND" : undefined} />
      {:else}
        <div class="gap"></div>
      {/if}
    {/each}
  </div>
  </div>
  {#if help.length}
    <ul class="help">{#each help as h}<li>{h}</li>{/each}</ul>
  {/if}
</article>

<style>
  .card { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 12px 12px 12px; display: flex; gap: 24px; min-width: 0; container-type: inline-size; }
  /* the knobs never spread wider than a page of 4 (no stretched knobs); a card wider than that shows its help beside */
  .inner { flex: 1; max-width: 400px; min-width: 0; display: flex; flex-direction: column; gap: 10px; }
  /* (rc3) stretched: the knobs' part as wide as a one-column card's (--colw from the Sound view), the help beside */
  .wide .inner { flex: none; width: min(400px, calc(var(--colw, 340px) - 26px)); }
  ul.help { flex: 1; min-width: 0; margin: 30px 0 0; padding: 0 0 0 18px; border-left: 1px solid var(--line); list-style: none; display: none; flex-direction: column; gap: 6px; font-size: 12px; line-height: 1.45; color: var(--dim); }
  @container (min-width: 620px) { ul.help { display: flex; } }
  .card:hover { border-color: var(--line-2); }
  header { display: flex; align-items: baseline; gap: 10px; }
  h3 { margin: 0; font-size: 15px; font-weight: 400; color: var(--fg); }
  .sub { font-size: 13px; color: var(--c); }
  .gir { margin-left: auto; font-size: 10px; font-weight: 700; color: var(--c); border: 1px solid color-mix(in srgb, var(--c) 50%, transparent); border-radius: 4px; padding: 0 5px; }
  .knobs { margin-top: auto; display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); justify-items: center; gap: 10px 4px; }
  .gap { width: 1px; }
</style>
