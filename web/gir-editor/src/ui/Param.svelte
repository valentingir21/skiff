<script lang="ts">
  // A parameter of a track (scope 0) or a global (scope 1), as a knob: its descriptor (the device's), its value as the
  // device formats it, and its help (the manual, the CC, where it is on the FM-1) in the help bar while hovered.
  import { device } from "../lib/model/device.svelte";
  import { fmtValue, aliasOf } from "../lib/legacy/proto.js";
  import { ccOf, HELP, helpText, isHelp, type Page } from "../lib/model/tables";
  import { ui } from "../lib/ui.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import Knob from "./components/Knob.svelte";

  interface Props { track?: number; id: number; scope?: 0 | 1; page?: Page; slot?: number; where?: string; color?: string; size?: number; label?: string }
  let { track = device.sel, id, scope = 0, page, slot = 0, where = "", color = "var(--accent)", size = 52, label }: Props = $props();

  const tr = $derived(device.tracks[track]);
  const d = $derived(scope === 1 ? device.gdesc[id] : tr ? device.desc(id, tr.engine) : undefined);
  const value = $derived(scope === 1 ? device.g[id] ?? 0 : tr?.p[id] ?? 0);
  const shown = $derived.by(() => {
    if (!d) return ["", ""];
    const v = d.fmt === 8 && d.names ? d.min + aliasOf(d.names, value - d.min) : value;
    return fmtValue(d, v) as [string, string];
  });
  const help = $derived.by(() => {
    if (!d || !tr) return "";
    if (page) {
      const fam = page.fam === "TRK" ? "GLO" : page.fam;
      const h = HELP.pages[`${fam}/${page.title}`]?.knobs?.[slot];
      if (h && isHelp(h)) return helpText(h);
    }
    const eng = device.engineName(tr.engine);
    return helpText(HELP.engines[eng]?.params?.[d.label]);
  });
  const fmtR = (v: number) => (d ? fmtValue(d, v).join(" ").trim() : String(v));
  function hover(on: boolean) {
    if (!on) { if (ui.hint?.label === d?.label) ui.hint = null; return; }
    if (!d) return;
    ui.hint = { label: d.label, text: help, range: `${fmtR(d.min)} … ${fmtR(d.max)}`, def: fmtR(d.def),
                cc: scope === 0 && tr ? ccOf(id, tr.engine) : null, where, color };
  }
</script>

{#if d}
  <Knob value={value} min={d.min} max={d.max} def={d.def} label={label ?? d.label} text={shown[0]} unit={shown[1]}
        names={d.fmt === 8 ? d.names : d.fmt === 11 ? ["OFF", "ON"] : null} {color} {size}
        describe={[help, where && `${t("param.where")} : ${where}`].filter(Boolean).join("\n")}
        onhover={hover}
        onchange={(v) => (scope === 1 ? device.setGlobal(id, v) : device.setParam(track, id, v, { label: d!.label }))} />
{:else}
  <div class="empty" style="width:{size + 34}px"></div>
{/if}

<style>.empty { height: 1px; }</style>
