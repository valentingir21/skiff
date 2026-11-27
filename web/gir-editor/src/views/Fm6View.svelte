<script lang="ts">
  // FM6: the selected track's own 6-operator patch (FM6_GET / FM6_PUT target 0), heard at once as it is edited; its
  // algorithm drawn (which operator modulates which, the carriers on the output, the feedback); the 8 factory patches;
  // .syx import (single voices and 32-voice banks, Dexed's format) and export
  import { device } from "../lib/model/device.svelte";
  import { FM6, req } from "../lib/legacy/proto.js";
  import { TRACK_COLORS, toast, download, pickFile, go, ask } from "../lib/ui.svelte";
  import { T } from "../lib/model/tables";
  import { t } from "../lib/i18n/index.svelte";
  import Knob from "../ui/components/Knob.svelte";
  import Button from "../ui/components/Button.svelte";

  const k = $derived(device.sel);
  const tr = $derived(device.tracks[k]);
  const isFm6 = $derived(!!tr && device.engineName(tr.engine) === "FM6");
  const color = $derived(TRACK_COLORS[k]);
  let v = $state<number[] | null>(null);
  let imported = $state<{ name: string; v: Uint8Array }[]>([]);
  let loadedFor = -1;

  async function read() {
    const g = await device.ask<{ rc: number; packed: number[] | null }>(req.fm6Get(0, k));
    if (!g.rc && g.packed) v = [...FM6.unpack(g.packed)];
  }
  $effect(() => { if (isFm6 && loadedFor !== k) { loadedFor = k; read().catch(() => {}); } });
  let timer: ReturnType<typeof setTimeout> | null = null;
  function sendSoon() { if (timer) clearTimeout(timer); timer = setTimeout(send, 60); }
  async function send() {
    if (!v) return;
    try { await device.ask(req.fm6Put(0, k, FM6.pack(Uint8Array.from(v))), { key: "fm6:" + k }); device.touch(k); } catch (e) { toast("err", String((e as Error).message)); }
  }
  function set(i: number, x: number) { if (!v) return; v[i] = Math.max(0, Math.min(FM6.max(i), x)); sendSoon(); }
  const op = (n: number, f: string) => (v ? v[FM6.at(n, f)] : 0);
  const alg = $derived(v ? v[FM6.VI.ALG] : 0);
  const name = $derived(v ? FM6.name(Uint8Array.from(v)) : "");

  // the algorithm's graph: the operators processed 6 .. 1, each reading a bus and writing one (msfa's flags)
  const graph = $derived.by(() => {
    const a = FM6.ALG[alg & 31] as number[];
    const bus: Record<number, number[]> = { 1: [], 2: [] };
    const mods: Record<number, number[]> = {};
    for (let x = 0; x < 6; x++) {
      const n = 6 - x, f = a[x];
      const inb = f & 0x10 ? 1 : f & 0x20 ? 2 : 0;
      mods[n] = inb ? [...bus[inb]] : [];
      const outb = f & 1 ? 1 : f & 2 ? 2 : 0;
      if (outb) bus[outb] = f & 4 ? [...bus[outb], n] : [n];
    }
    const carriers: number[] = FM6.carriers(alg);
    const depth: Record<number, number> = {};
    const walk = (n: number, d: number) => { depth[n] = Math.max(depth[n] ?? 0, d); for (const m of mods[n]) walk(m, d + 1); };
    for (const c of carriers) walk(c, 0);
    for (let n = 1; n <= 6; n++) depth[n] ??= 0;
    const rows: number[][] = [];
    for (let n = 1; n <= 6; n++) (rows[depth[n]] ??= []).push(n);
    const pos: Record<number, [number, number]> = {};
    const H = rows.length;
    rows.forEach((r, d) => r.forEach((n, i) => (pos[n] = [((i + 1) / (r.length + 1)) * 300, 30 + (H - 1 - d) * 54])));
    return { mods, carriers, pos, fb: FM6.feedbackOp(alg), h: 30 + H * 54 };
  });

  async function importSyx() {
    const [f] = await pickFile(".syx,.SYX,application/octet-stream");
    if (!f) return;
    const r = FM6.parseSysex(new Uint8Array(await f.arrayBuffer()));
    imported = r.voices;
    if (!r.voices.length) toast("warn", `${f.name}: no voice`);
  }
  function useVoice(x: Uint8Array) { v = [...x]; send(); }
  function exportSyx() { if (v) download(`${name.trim() || "FM6"}.syx`, Uint8Array.from(FM6.singleSysex(Uint8Array.from(v))), "application/octet-stream"); }
  // (rc3) the empty state: the FM6 tracks to pick, or this track put on FM6 (asked first: its sound changes)
  const fm6Tracks = $derived(device.tracks.map((x, i) => ({ x, i })).filter(({ x }) => device.engineName(x.engine) === "FM6"));
  const FM6_E = T.ENG.findIndex((x) => x.name === "FM6");
  const lightOff = $derived(!!device.info && !!(device.info.light.tracks & (1 << k)) && !!(device.info.light.engines & (1 << FM6_E)));
  async function toFm6() {
    if (!(await ask({ title: t("fm6.switchTitle", { n: k + 1 }), body: t("fm6.switchBody", { n: k + 1, name: tr?.name ?? "" }), ok: t("fm6.switchOk") }))) return;
    await device.setEngine(k, FM6_E);
  }
  const ENVN = ["R1", "R2", "R3", "R4", "L1", "L2", "L3", "L4"];
</script>

<div class="fm6" style="--c:{color}">
  {#if !isFm6}
    <div class="empty">
      <h2>{t("fm6.emptyTitle", { n: k + 1, eng: tr ? device.engineName(tr.engine) : "" })}</h2>
      <p>{t("fm6.emptyLead")}</p>
      {#if fm6Tracks.length}
        <div class="picks">
          {#each fm6Tracks as { x, i }}
            <button class="pick" style="--tc:{TRACK_COLORS[i]}" onclick={() => device.selectTrack(i)}>
              <span class="n dot">{i + 1}</span><span class="nm dot">{x.name}</span><span class="go">{t("fm6.editThis")}</span>
            </button>
          {/each}
        </div>
      {:else}
        <p class="none">{t("fm6.noneOnFm6")}</p>
      {/if}
      <div class="acts2">
        <Button variant="primary" onclick={toFm6} disabled={lightOff}>{t("fm6.switch", { n: k + 1 })}</Button>
        <Button variant="ghost" onclick={() => go("sound")}>{t("view.sound")}</Button>
      </div>
    </div>
  {:else if v}
    <header>
      <h2 class="dot">T{k + 1} · {name || "FM6"}</h2>
      <div class="acts">
        <select onchange={(e) => { const i = +(e.currentTarget as HTMLSelectElement).value; if (i >= 0) useVoice(FM6.factory(i)); (e.currentTarget as HTMLSelectElement).value = "-1"; }} aria-label="Factory patch">
          <option value="-1">Factory patch…</option>
          {#each ["TINE EP", "BELL", "FM BASS", "BRASS", "PAD", "MARIMBA", "ORGAN", "PLUCK"] as nm, i}<option value={i}>F{i + 1} {nm}</option>{/each}
        </select>
        <Button size="s" onclick={importSyx}>{t("fm6.import")}</Button>
        <Button size="s" onclick={exportSyx}>{t("fm6.export")}</Button>
        <Button size="s" variant="ghost" onclick={read}>{t("fm6.read")}</Button>
      </div>
    </header>
    {#if imported.length}
      <div class="imp">{#each imported as x}<button class="dot" onclick={() => useVoice(x.v)}>{x.name}</button>{/each}</div>
    {/if}

    <div class="top">
      <section class="alg card">
        <h3>{t("fm6.algo")} {alg + 1}</h3>
        <svg viewBox="0 0 300 {graph.h}" role="img" aria-label="Algorithm {alg + 1}">
          {#each Object.entries(graph.mods) as [n, ms]}{#each ms as m}
            <line x1={graph.pos[m][0]} y1={graph.pos[m][1] + 14} x2={graph.pos[+n][0]} y2={graph.pos[+n][1] - 14} class="ln" />
          {/each}{/each}
          {#each graph.carriers as c}<line x1={graph.pos[c][0]} y1={graph.pos[c][1] + 14} x2={graph.pos[c][0]} y2={graph.h - 4} class="out" />{/each}
          <line x1="10" x2="290" y1={graph.h - 4} y2={graph.h - 4} class="out" />
          {#each [1, 2, 3, 4, 5, 6] as n}
            <g transform="translate({graph.pos[n][0] - 16} {graph.pos[n][1] - 14})">
              {#if graph.fb === n}<path d="M32 6 h8 v-14 h-24 v8" class="fb" />{/if}
              <rect width="32" height="28" rx="6" class:car={graph.carriers.includes(n)} />
              <text x="16" y="19" text-anchor="middle">{n}</text>
            </g>
          {/each}
        </svg>
        <div class="row">
          <Knob value={alg} min={0} max={31} label="ALGO" text={String(alg + 1)} {color} onchange={(x) => set(FM6.VI.ALG, x)} />
          <Knob value={v[FM6.VI.FB]} min={0} max={7} label="FEEDBACK" text={String(v[FM6.VI.FB])} {color} onchange={(x) => set(FM6.VI.FB, x)} />
          <Knob value={v[FM6.VI.TRNSP]} min={0} max={48} def={24} label="TRANSP" text={String(v[FM6.VI.TRNSP] - 24)} {color} onchange={(x) => set(FM6.VI.TRNSP, x)} />
          <Knob value={v[FM6.VI.LFS]} min={0} max={99} label="LFO SPD" text={String(v[FM6.VI.LFS])} {color} onchange={(x) => set(FM6.VI.LFS, x)} />
          <Knob value={v[FM6.VI.LPMD]} min={0} max={99} label="PM DEPTH" text={String(v[FM6.VI.LPMD])} {color} onchange={(x) => set(FM6.VI.LPMD, x)} />
        </div>
      </section>
    </div>

    <div class="ops">
      {#each [1, 2, 3, 4, 5, 6] as n}
        <section class="card op" class:car={graph.carriers.includes(n)}>
          <h3 class="dot">OP {n} <span>{FM6.freqText(Uint8Array.from(v), n)}</span></h3>
          <div class="g">
            <Knob value={op(n, "OL")} min={0} max={99} label="LEVEL" text={String(op(n, "OL"))} {color} size={34} onchange={(x) => set(FM6.at(n, "OL"), x)} />
            <Knob value={op(n, "FC")} min={0} max={31} label="COARSE" text={String(op(n, "FC"))} {color} size={34} onchange={(x) => set(FM6.at(n, "FC"), x)} />
            <Knob value={op(n, "FF")} min={0} max={99} label="FINE" text={String(op(n, "FF"))} {color} size={34} onchange={(x) => set(FM6.at(n, "FF"), x)} />
            <Knob value={op(n, "DET")} min={0} max={14} def={7} label="DETUNE" text={String(op(n, "DET") - 7)} {color} size={34} onchange={(x) => set(FM6.at(n, "DET"), x)} />
            <Knob value={op(n, "KVS")} min={0} max={7} label="VEL" text={String(op(n, "KVS"))} {color} size={34} onchange={(x) => set(FM6.at(n, "KVS"), x)} />
            <Knob value={op(n, "RS")} min={0} max={7} label="RATE SC" text={String(op(n, "RS"))} {color} size={34} onchange={(x) => set(FM6.at(n, "RS"), x)} />
          </div>
          <div class="env">
            {#each ENVN as f}
              <label><span>{f}</span><input type="range" min="0" max="99" value={op(n, f)} oninput={(e) => set(FM6.at(n, f), +(e.currentTarget as HTMLInputElement).value)} aria-label={`OP${n} ${f}`} /></label>
            {/each}
          </div>
        </section>
      {/each}
    </div>
  {:else}
    <p class="muted">…</p>
  {/if}
</div>

<style>
  .fm6 { display: flex; flex-direction: column; gap: 12px; }
  .empty { max-width: 560px; margin: 40px auto; padding: 28px; background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-xl); color: var(--fg-2); display: flex; flex-direction: column; gap: 14px; }
  .empty h2 { margin: 0; font-size: var(--fs-xl); color: var(--fg); }
  .empty p { margin: 0; line-height: 1.5; }
  .empty .none { color: var(--dim); }
  .picks { display: flex; flex-direction: column; gap: 6px; }
  .pick { display: grid; grid-template-columns: 28px 1fr auto; align-items: center; gap: 10px; padding: 8px 10px; border-radius: var(--r-m); border: 1px solid var(--line-2); background: var(--surf-2); text-align: left; }
  .pick:hover { border-color: var(--tc); }
  .pick .n { width: 28px; height: 28px; display: grid; place-items: center; border-radius: 7px; background: var(--tc); color: #0a0a0a; }
  .pick .go { color: var(--dim); font-size: 12px; font-weight: 600; }
  .acts2 { display: flex; gap: 8px; flex-wrap: wrap; }
  header { display: flex; align-items: center; gap: 12px; }
  h2 { margin: 0; font-size: 22px; font-weight: 400; color: var(--c); flex: 1; }
  .acts { display: flex; gap: 6px; }
  select { height: 28px; border-radius: 7px; border: 1px solid var(--line-2); background: var(--surf-3); }
  .imp { display: flex; flex-wrap: wrap; gap: 4px; }
  .imp button { border: 1px solid var(--line-2); background: var(--surf-2); border-radius: 6px; padding: 3px 8px; font-size: 13px; }
  .imp button:hover { border-color: var(--c); }
  .card { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 12px; }
  h3 { margin: 0 0 8px; font-size: 14px; font-weight: 400; display: flex; justify-content: space-between; }
  h3 span { font-size: 12px; color: var(--dim); font-family: var(--font-ui); }
  .alg { display: grid; grid-template-columns: 320px 1fr; gap: 16px; align-items: center; }
  .alg h3 { grid-column: span 2; }
  svg { width: 100%; max-width: 320px; }
  .ln { stroke: var(--fg-2); stroke-width: 1.5; }
  .out { stroke: var(--c); stroke-width: 2; }
  rect { fill: var(--surf-3); stroke: var(--line-2); }
  rect.car { fill: color-mix(in srgb, var(--c) 30%, var(--surf-3)); stroke: var(--c); }
  text { fill: var(--fg); font-family: var(--font-dot); font-size: 15px; }
  .fb { fill: none; stroke: var(--warn); stroke-width: 1.5; }
  .row { display: flex; flex-wrap: wrap; gap: 10px 0; }
  .ops { display: grid; grid-template-columns: repeat(auto-fill, minmax(250px, 1fr)); gap: 10px; }
  .op.car { border-color: color-mix(in srgb, var(--c) 40%, var(--line)); }
  .g { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); justify-items: center; row-gap: 10px; }
  .env { display: grid; grid-template-columns: 1fr 1fr; gap: 2px 12px; margin-top: 12px; }
  .env label { display: grid; grid-template-columns: 22px 1fr; align-items: center; gap: 6px; font-size: 11px; color: var(--dim); }
  .env input { accent-color: var(--c); width: 100%; }
  .muted { color: var(--dim); }
  @media (max-width: 1200px) { .alg { grid-template-columns: 1fr; } }
</style>
