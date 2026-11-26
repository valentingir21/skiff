<script lang="ts">
  // The focused step, every field the device keeps: its notes, NOTE / TIE / REST, ACC SLD, velocity, chance, ratchet,
  // the trig condition, FLAM, NUDGE
  import { device } from "../../lib/model/device.svelte";
  import { TC_NAMES, parseNotes, noteName } from "../../lib/legacy/proto.js";
  import type { Step } from "../../lib/protocol/types";
  import Knob from "../../ui/components/Knob.svelte";
  import Seg from "../../ui/components/Seg.svelte";
  import Toggle from "../../ui/components/Toggle.svelte";
  import { t } from "../../lib/i18n/index.svelte";
  interface Props { k: number; i: number | null; color: string }
  let { k, i, color }: Props = $props();
  const s = $derived(i == null ? null : device.steps[k]?.[i] ?? null);
  const notesText = $derived(s && s.time === 0 ? s.notes.slice(0, s.n).map((n: number) => noteName(n)).join(" ") : "");
  let draft = $state("");
  $effect(() => { draft = notesText; });
  function put(patch: Partial<Step>) { if (i == null || !s) return; device.editSteps(k, { [i]: { ...s, notes: [...s.notes], ...patch } }, "step"); }
  function commitNotes() {
    const p = parseNotes(draft);
    if (!p) { draft = notesText; return; }
    if (!p.length) put({ n: 0, notes: [0, 0, 0, 0], time: s?.hit ? 0 : 2 });
    else put({ n: p.length, notes: [...p, 0, 0, 0, 0].slice(0, 4), time: 0, vel: s?.vel || 100 });
  }
</script>
<aside class="ins" style="--c:{color}">
  {#if s && i != null}
    <h3 class="dot">{t("seq.step", { n: i + 1 })}</h3>
    <label class="f"><span>Notes</span>
      <input class="dot" bind:value={draft} onblur={commitNotes} onkeydown={(e) => e.key === "Enter" && commitNotes()} placeholder="C3 E3 G3" spellcheck="false" />
    </label>
    <Seg size="s" label="time" value={s.time} {color} onchange={(v) => put({ time: v, ...(v !== 0 ? { n: 0, notes: [0, 0, 0, 0] } : {}) })}
         options={[{ value: 0, label: "NOTE" }, { value: 1, label: "TIE" }, { value: 2, label: "REST" }]} />
    <div class="tg">
      <Toggle on={!!(s.flags & 1)} label="ACC" {color} title={t("seq.acc")} onchange={() => put({ flags: s.flags ^ 1 })} />
      <Toggle on={!!(s.flags & 2)} label="SLD" {color} title={t("seq.sld")} onchange={() => put({ flags: s.flags ^ 2 })} />
      <Toggle on={!!s.flam} label="FLAM" {color} title={t("seq.flam")} onchange={(on) => put({ flam: on ? 1 : 0, ratchet: on ? 1 : s.ratchet })} />
    </div>
    <div class="kn">
      <Knob value={s.vel} min={1} max={127} def={100} label="VEL" text={String(s.vel)} {color} size={40} onchange={(v) => put({ vel: v })} />
      <Knob value={s.chance} min={0} max={100} def={100} label="CHANCE" text={String(s.chance)} unit="%" {color} size={40} onchange={(v) => put({ chance: v })} />
      <Knob value={s.ratchet} min={1} max={4} def={1} label="RATCHET" text={"x" + s.ratchet} {color} size={40} onchange={(v) => put({ ratchet: v, flam: 0 })} />
      <Knob value={s.cond} min={0} max={TC_NAMES.length - 1} def={0} label="COND" text={TC_NAMES[s.cond]} names={TC_NAMES} {color} size={40} onchange={(v) => put({ cond: v })} />
      <Knob value={s.nudge} min={-32} max={31} def={0} label="NUDGE" text={(s.nudge > 0 ? "+" : "") + s.nudge} {color} size={40} onchange={(v) => put({ nudge: v })} />
    </div>
  {:else}
    <p class="none">{t("seq.noStep")}</p>
  {/if}
</aside>
<style>
  .ins { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 12px; display: flex; flex-direction: column; gap: 10px; }
  h3 { margin: 0; font-size: 16px; font-weight: 400; color: var(--c); }
  .f { display: flex; flex-direction: column; gap: 4px; font-size: 11px; color: var(--dim); font-weight: 600; }
  .f input { height: 32px; border-radius: 7px; border: 1px solid var(--line-2); background: var(--bg); padding: 0 10px; font-size: 14px; }
  .f input:focus { border-color: var(--c); }
  .tg { display: flex; gap: 6px; }
  .kn { display: grid; grid-template-columns: repeat(3, 1fr); justify-items: center; row-gap: 6px; }
  .none { color: var(--dim); margin: 0; font-size: 12.5px; }
</style>
