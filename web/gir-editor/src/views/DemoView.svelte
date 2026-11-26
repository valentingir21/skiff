<script lang="ts">
  // The design system's own page (?view=demo): every component in its states, for the design review
  import Knob from "../ui/components/Knob.svelte";
  import Fader from "../ui/components/Fader.svelte";
  import Seg from "../ui/components/Seg.svelte";
  import Toggle from "../ui/components/Toggle.svelte";
  import Button from "../ui/components/Button.svelte";
  import AdsrGraph from "../ui/graphs/AdsrGraph.svelte";
  import WaveGraph from "../ui/graphs/WaveGraph.svelte";
  import RumbleGraph from "../ui/graphs/RumbleGraph.svelte";
  import { toast, ask, go } from "../lib/ui.svelte";
  let v = $state([64, 0, 127, 20]);
  let f = $state(100);
  let seg = $state("a");
  let on = $state(true);
  let env = $state({ a: 10, d: 70, s: 90, r: 60 });
  let rb = $state({ mix: 100, slice: 2, decay: 80, atk: 0, duck: 64, tone: 50 });
  const cols = ["var(--t1)", "var(--t2)", "var(--t3)", "var(--t4)", "var(--t5)", "var(--t6)"];
</script>
<div class="demo scroll">
  <header><h1 class="dot">GIR EDITOR · DESIGN SYSTEM</h1><Button variant="ghost" onclick={() => go("sound")}>Back</Button></header>
  <section><h2>Colour</h2>
    <div class="sw">{#each ["--bg", "--bg-2", "--surf", "--surf-2", "--surf-3", "--surf-4", "--line", "--line-2", "--fg", "--fg-2", "--dim", "--mute", "--t1", "--t2", "--t3", "--t4", "--t5", "--t6", "--ok", "--warn", "--err", "--info"] as c}
      <div class="chip"><span style="background:var({c})"></span><code>{c}</code></div>{/each}</div></section>
  <section><h2>Type</h2>
    <p class="dot big">SUPERSAW 120 BPM</p><p class="ui">Inter for the interface: every sound, pattern and song of your FM-1.</p><p class="num">0123456789 tabular</p></section>
  <section><h2>Buttons</h2><div class="row">
    <Button variant="primary">Primary</Button><Button>Secondary</Button><Button active>Active</Button><Button variant="ghost">Ghost</Button>
    <Button variant="danger">Danger</Button><Button disabled>Disabled</Button><Button size="s">Small</Button><Button size="l" variant="primary">Large</Button></div></section>
  <section><h2>Knobs</h2><div class="row">
    {#each v as x, i}<Knob value={x} min={i === 1 ? -64 : 0} max={i === 1 ? 63 : 127} def={64} label={["CUTOFF", "PAN", "LEVEL", "WAVE"][i]} text={i === 3 ? ["SIN", "TRI", "SAW", "SQR"][x % 4] : String(x)} unit={i === 3 ? "" : "%"} names={i === 3 ? ["SIN", "TRI", "SAW", "SQR"] : null} color={cols[i]} onchange={(n) => (v[i] = n)} />{/each}
    <Knob value={40} min={0} max={127} label="DISABLED" text="40" disabled onchange={() => {}} />
    <Knob value={90} min={0} max={127} label="SMALL" text="90" size={32} color={cols[5]} onchange={() => {}} /></div></section>
  <section><h2>Fader, segmented, toggles</h2><div class="row">
    <Fader value={f} label="LEVEL" text={f + ""} onchange={(n) => (f = n)} />
    <Seg value={seg} onchange={(x) => (seg = x)} options={[{ value: "a", label: "Piano roll" }, { value: "b", label: "Drum grid" }, { value: "c", label: "Third" }]} />
    <Toggle on={on} label="ACC" onchange={(x) => (on = x)} /><Toggle on={!on} label="SLD" color="var(--t2)" onchange={(x) => (on = !x)} /></div></section>
  <section><h2>Graphs</h2><div class="g3">
    <div class="card"><AdsrGraph {...env} onchange={(w, x) => (env[w] = x)} /></div>
    <div class="card"><WaveGraph wave="TRI" cycles={3} color="var(--t2)" /></div>
    <div class="card"><RumbleGraph {...rb} color="var(--t5)" /></div></div></section>
  <section><h2>Feedback</h2><div class="row">
    <Button onclick={() => toast("ok", "Saved to slot A.")}>Toast ok</Button><Button onclick={() => toast("warn", "Stop playback first.")}>Toast warn</Button>
    <Button onclick={() => toast("err", "The FM-1 didn't answer. Check the cable.")}>Toast error</Button>
    <Button onclick={() => ask({ title: "Erase U07?", body: "This can't be undone.", ok: "Erase", danger: true })}>Confirm</Button>
    <Button onclick={() => ask({ title: "Save as", body: "", ok: "Save", input: { label: "Project name", value: "ACID NIGHT" } })}>Prompt</Button></div></section>
</div>
<style>
  .demo { height: 100%; padding: 28px 40px; display: flex; flex-direction: column; gap: 28px; }
  header { display: flex; justify-content: space-between; align-items: center; }
  h1 { margin: 0; font-size: 22px; font-weight: 400; }
  h2 { margin: 0 0 12px; font-size: 13px; color: var(--fg-2); }
  .row { display: flex; flex-wrap: wrap; gap: 12px; align-items: center; }
  .sw { display: grid; grid-template-columns: repeat(auto-fill, minmax(120px, 1fr)); gap: 8px; }
  .chip { display: flex; align-items: center; gap: 8px; font-size: 11.5px; }
  .chip span { width: 26px; height: 26px; border-radius: 6px; border: 1px solid var(--line-2); }
  .big { font-size: 34px; margin: 0; }
  .ui { font-size: 15px; margin: 6px 0; }
  .g3 { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; }
  .card { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 12px; }
</style>
