<script lang="ts">
  // The selected track's sequence: where it is (its entry and pattern in the song), PATTERN (LEN DIV SWING GATE), the
  // device's SEQ TOOLS and, on an ACID track, Gir's acid generator, then the piano roll or the drum grid, the step
  // lanes (ACC, SLD, velocity, chance, ratchet, nudge), the focused step, and the automation lanes
  import { device, TOOL } from "../lib/model/device.svelte";
  import { ID, T } from "../lib/model/tables";
  import { TRACK_COLORS, toast } from "../lib/ui.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import Param from "../ui/Param.svelte";
  import Seg from "../ui/components/Seg.svelte";
  import Button from "../ui/components/Button.svelte";
  import PianoRoll from "./seq/PianoRoll.svelte";
  import DrumGrid from "./seq/DrumGrid.svelte";
  import StepLanes from "./seq/StepLanes.svelte";
  import Inspector from "./seq/Inspector.svelte";
  import Automation from "./seq/Automation.svelte";

  const k = $derived(device.sel);
  const tr = $derived(device.tracks[k]);
  const color = $derived(TRACK_COLORS[k]);
  const eng = $derived(tr ? device.engineName(tr.engine) : "");
  const drum = $derived(eng === "DRUM" || eng === "DRUM-X");
  const acid = $derived(eng === "ACID");
  const len = $derived(tr?.p[ID.P_SLEN] ?? 16);
  let mode = $state<"roll" | "drums" | null>(null);
  const shown = $derived(mode ?? (drum ? "drums" : "roll"));
  let zoom = $state(1);
  let sel = $state(new Set<number>());
  let focus = $state<number | null>(null);
  let busyTool = $state<string | null>(null);
  let roll: PianoRoll | undefined = $state();

  $effect(() => {
    const kk = k;
    mode = null; sel = new Set(); focus = null;
    if (!device.steps[kk] && device.stepsLoading == null) device.loadSteps(kk).then(() => device.readMotion(kk)).catch(() => {});
    else device.readMotion(kk).catch(() => {});
    if (device.info && device.info.skiff & 8) device.readSong().catch(() => {});
  });
  const where = $derived.by(() => {
    const x = device.lists?.tracks[k];
    if (!x) return "";
    if (x.cur < 0) return t("seq.nowhere", { t: k + 1 });
    return t("seq.where", { t: k + 1, e: "ABCDEFGHIJKLMNOP"[x.cur], p: x.pat < 0 ? "–" : String(x.pat + 1).padStart(2, "0") });
  });
  async function tool(name: keyof typeof TOOL, label: string) {
    if (busyTool) return;
    busyTool = name;
    try { await device.seqTool(k, TOOL[name]); toast("ok", t("seq.toolDone", { tool: label, n: k + 1 })); }
    catch (e) {
      const m = String((e as Error).message);
      toast("err", m.startsWith("seqtool:") ? t("seq." + m.slice(8).replace("noeffect", "noEffect").replace("rec", "recArmed"), { tool: label, n: k + 1 }) : m);
    }
    finally { busyTool = null; }
  }
  const TOOLS: (keyof typeof TOOL)[] = ["CLEAR", "REVERSE", "SHIFT_L", "SHIFT_R", "RANDOM", "COOK", "BEAT", "EUCLID", "DOUBLE", "HALF"];
  const ACID: [keyof typeof TOOL, string][] = [["ACID_NEW", "seq.acidNew"], ["ACID_RHYTHM", "seq.acidRhythm"], ["ACID_NOTES", "seq.acidNotes"], ["ACID_ACCSLD", "seq.acidAccSld"]];
  const patPage = T.PAGES.find((p) => p.title === "PATTERN")!;
  function onKey(e: KeyboardEvent) {
    const el = e.target as HTMLElement;
    if (el.tagName === "INPUT" || el.tagName === "SELECT" || el.tagName === "TEXTAREA") return;
    if (shown === "roll" && roll?.key(e)) e.preventDefault();
  }
</script>

<svelte:window onkeydown={onKey} />

<div class="seq" style="--c:{color}">
  <header class="head">
    <div class="where">
      <h2 class="dot">T{k + 1} · {tr?.name}</h2>
      <span>{where}</span>
    </div>
    <div class="pat">
      {#each patPage.ids as id, slot}<Param track={k} {id} page={patPage} {slot} where={`SEQ ×3 › PATTERN › KNOB ${slot + 1}`} {color} size={38} />{/each}
    </div>
    <div class="right">
      <Seg size="s" label="mode" value={shown} {color} onchange={(v) => (mode = v)}
           options={[{ value: "roll", label: t("seq.roll") }, { value: "drums", label: t("seq.drums") }]} />
      <label class="zoom"><span class="sr-only">Zoom</span><input type="range" min="0.6" max="2" step="0.1" bind:value={zoom} aria-label="Zoom" /></label>
      <Button size="s" variant="ghost" onclick={() => device.loadSteps(k).then(() => device.readMotion(k))}>{t("seq.reload")}</Button>
    </div>
  </header>

  <div class="tools" role="toolbar" aria-label={t("seq.tools")}>
    <span class="tl" title={t("seq.tools.hint")}>{t("seq.tools")}</span>
    {#each TOOLS as nm}
      <button class="tool" disabled={!!busyTool || !device.hasPanel} class:busy={busyTool === nm} onclick={() => tool(nm, t("seq.tool." + nm))} title={t("seq.tools.hint")}>{t("seq.tool." + nm)}</button>
    {/each}
    <span class="sep"></span>
    <span class="tl acid" title={acid ? t("seq.tools.hint") : t("seq.acidOnly")}>{t("seq.acid")}</span>
    {#each ACID as [nm, key]}
      <button class="tool acidb" disabled={!acid || !!busyTool || !device.hasPanel} class:busy={busyTool === nm} title={acid ? t("seq.tools.hint") : t("seq.acidOnly")} onclick={() => tool(nm, t(key))}>{t(key)}</button>
    {/each}
  </div>

  {#if device.stepsLoading === k || !device.steps[k]}
    <div class="loading">{t("seq.loading")}</div>
  {:else}
    <p class="help">{shown === "roll" ? t("seq.help") : t("seq.drumHelp")}</p>
    <div class="main">
      <div class="grids">
        {#if shown === "roll"}
          <PianoRoll bind:this={roll} {k} {len} {color} {zoom} {sel} onselect={(s, f) => { sel = s; focus = f ?? null; }} />
        {:else}
          <DrumGrid {k} {len} {color} {zoom} {sel} onselect={(s, f) => { sel = s; focus = f ?? null; }} />
        {/if}
        <StepLanes {k} {len} {color} {zoom} {sel} />
      </div>
      <Inspector {k} i={focus} {color} />
    </div>
    <Automation {k} {len} {color} {zoom} />
  {/if}
</div>

<style>
  .seq { display: flex; flex-direction: column; gap: 12px; }
  .head { display: flex; align-items: center; gap: 24px; flex-wrap: wrap; }
  .where h2 { margin: 0; font-size: 22px; font-weight: 400; color: var(--c); }
  .where span { font-size: 12.5px; color: var(--dim); }
  .pat { display: flex; gap: 0; background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 6px 4px; }
  .right { margin-left: auto; display: flex; align-items: center; gap: 10px; }
  .zoom input { width: 90px; accent-color: var(--c); }
  .tools { display: flex; flex-wrap: wrap; align-items: center; gap: 4px; padding: 8px; background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); }
  .tl { font-size: 11.5px; font-weight: 700; color: var(--dim); padding: 0 6px; }
  .tl.acid { color: var(--t2); }
  .tool { height: 28px; padding: 0 10px; border-radius: 7px; border: 1px solid var(--line-2); background: var(--surf-3); font-size: 12px; font-weight: 600; color: var(--fg-2); }
  .tool:hover:not(:disabled) { color: var(--fg); border-color: var(--c); }
  .tool:disabled { opacity: 0.38; cursor: default; }
  .tool.busy { background: var(--c); color: #0b0b0b; border-color: var(--c); }
  .acidb:not(:disabled) { border-color: color-mix(in srgb, var(--t2) 50%, var(--line-2)); }
  .sep { width: 1px; height: 20px; background: var(--line-2); margin: 0 6px; }
  .loading { padding: 40px; text-align: center; color: var(--dim); }
  .help { margin: 0; color: var(--dim); font-size: 12px; }
  .main { display: grid; grid-template-columns: 1fr 250px; gap: 12px; align-items: start; }
  .grids { min-width: 0; display: flex; flex-direction: column; gap: 8px; }
  @media (max-width: 1150px) { .main { grid-template-columns: 1fr; } }
</style>
