<script lang="ts">
  // The three user sample slots (USR1..USR3, 80 KB each), as the original editor's Samples tab, every function of it:
  // - what each slot holds on the FM-1, its room used, its sounds' keys and LEVEL / REV / DLY (SMP_MIX, set at once);
  // - a new slot from WAV files (one sound per file, its root from the name, "pad_C3.wav"; the slot's name from the
  //   first file), from the computer's audio input (a level meter, a count-in, a preview), each sound trimmed on its
  //   waveform (two ends to drag), heard as it will be stored;
  // - a DRUM KIT: a sound per DRUM lane (KICK .. BELL), each with its level and where it starts, from a file per lane,
  //   or (rc2) one file cut at its hits into the lanes;
  // - the room the draft takes (16 sounds, ~7.4 s, 80 KB), and the upload: asked first, playback stopped, then written.
  import { onDestroy, onMount } from "svelte";
  import { device } from "../lib/model/device.svelte";
  import { req, buildSlot, kitZones, noteName, parseNote } from "../lib/legacy/proto.js";
  import { ask, toast, toastKey, pickFile } from "../lib/ui.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import Button from "../ui/components/Button.svelte";
  import TrimWave from "./samples/TrimWave.svelte";
  import { decode, slotName, preview, stopPreview, Recorder, inputs, sliceToLanes, bytesOf, secondsOf, autoTrim, sample,
    MAX_DATA, MAX_ZONES, LANE_NAMES, MIX0, pctOf, rawOf, RATE, type Sound } from "../lib/samples";

  interface SlotInfo { zones: number; name: string; kib: number; mix?: { keys: number[][]; mix: number[][]; rc: number } }
  interface Draft { name: string; kit: boolean; files: Sound[]; lanes: (Sound | null)[] }
  let info = $state<{ nslots: number; slotKiB: number; slots: SlotInfo[] } | null>(null);
  let drafts = $state<Draft[]>([0, 1, 2].map(() => ({ name: "", kit: false, files: [], lanes: LANE_NAMES.map(() => null) })));
  let busy = $state<{ k: number; pct: number; what: string } | null>(null);
  const mixOn = $derived(!!device.info && !!((device.info as any).skiff & 32));

  async function read() {
    const si = await device.ask(req.smpInfo());
    if (mixOn) for (const [k, u] of si.slots.entries()) {
      if (!u.zones) continue;
      try { u.mix = await device.ask(req.smpMix(k)); } catch { /* shown without */ }
    }
    info = si;
  }
  onMount(() => { read().catch(() => {}); inputs().then((l) => (ins = l)); });
  onDestroy(() => { rec?.cancel(); stopPreview(); clearInterval(tick); });

  const sounds = (d: Draft) => (d.kit ? d.lanes.filter(Boolean) as Sound[] : d.files);
  const used = (d: Draft) => sounds(d).reduce((n, s) => n + bytesOf(s), 0);
  const tooBig = (d: Draft) => used(d) > MAX_DATA;
  function keysOf(files: Sound[]) {
    const z = files.map((f, i) => ({ i, root: f.root })).sort((a, b) => a.root - b.root), out: [number, number][] = [];
    z.forEach((x, j) => { out[x.i] = [j === 0 ? 0 : Math.floor((z[j - 1].root + x.root) / 2) + 1, j === z.length - 1 ? 127 : Math.floor((x.root + z[j + 1].root) / 2)]; });
    return out;
  }

  // ------------------------------------------------------------------ files ---
  async function addFiles(k: number, list: File[]) {
    const d = drafts[k];
    for (const f of list) {
      if (d.files.length >= MAX_ZONES) { toastKey("warn", "smp.max16"); break; }
      try { const s = await decode(f); d.files.push(s); if (!d.name) d.name = slotName(f.name); }
      catch (e) { toastKey("err", "smp.decodeFail", { file: f.name, detail: String((e as Error).message) }); }
    }
  }
  async function pick(k: number) { addFiles(k, await pickFile(".wav,audio/wav,audio/x-wav,audio/*", true)); }
  async function laneFile(k: number, i: number, f: File) {
    const d = drafts[k];
    try {
      const s = await decode(f);
      [s.a, s.b] = autoTrim(s.raw, 0.003);           // (silence before a hit would play late)
      d.lanes[i] = s;
      if (!d.name) d.name = "KIT";
    } catch (e) { toastKey("err", "smp.decodeFail", { file: f.name, detail: String((e as Error).message) }); }
  }
  async function pickLane(k: number, i: number) { const [f] = await pickFile(".wav,audio/wav,audio/x-wav,audio/*"); if (f) laneFile(k, i, f); }
  async function sliceFile(k: number) {
    const [f] = await pickFile(".wav,audio/wav,audio/x-wav,audio/*");
    if (!f) return;
    try {
      const s = await decode(f), lanes = sliceToLanes(s);
      drafts[k].lanes = lanes;
      if (!drafts[k].name) drafts[k].name = slotName(f.name) || "KIT";
      toastKey("ok", "smp.sliced", { n: lanes.filter(Boolean).length, file: f.name });
    } catch (e) { toastKey("err", "smp.decodeFail", { file: f.name, detail: String((e as Error).message) }); }
  }
  function drop(k: number, e: DragEvent, lane?: number) {
    e.preventDefault(); e.stopPropagation();
    over = null;
    if (busy || rec) return;
    const fs = [...(e.dataTransfer?.files ?? [])];
    if (!fs.length) return;
    if (lane != null) laneFile(k, lane, fs[0]); else if (!drafts[k].kit) addFiles(k, fs);
  }
  let over = $state<string | null>(null);

  // -------------------------------------------------------------- recording ---
  let ins = $state<MediaDeviceInfo[]>([]);
  let inputId = $state("");
  let countIn = $state(true);
  let rec: Recorder | null = null;
  let recK = $state<number | null>(null);
  let recState = $state<"count" | "on" | null>(null);
  let count = $state(0);
  let level = $state(0), secs = $state(0), maxSecs = $state(0);
  let tick: ReturnType<typeof setInterval> | undefined;
  async function recStart(k: number) {
    const d = drafts[k], left = MAX_DATA - used(d);
    if (rec || busy) return;
    if (left < 1024 || d.files.length >= MAX_ZONES) { toastKey("warn", "smp.recFull"); return; }
    recK = k;
    if (countIn) {
      recState = "count";
      for (count = 3; count > 0; count--) { await new Promise((r) => setTimeout(r, 1000)); if (recK !== k) return; }
    }
    const r = new Recorder();
    rec = r;
    try {
      await r.start(inputId, secondsOf(left));
      r.onFull = () => recStop();
      recState = "on";
      ins = await inputs();                        // (the labels come with the permission)
      tick = setInterval(() => { level = Math.min(1, r.peak); r.peak *= 0.7; secs = r.seconds; maxSecs = r.maxSeconds; }, 50);
    } catch (e) {
      r.cancel(); rec = null; recK = null; recState = null;
      toastKey("err", "smp.recDenied", { detail: String((e as Error).message) });
    }
  }
  async function recStop() {
    const r = rec, k = recK;
    if (!r || k == null) { recK = null; recState = null; return; }
    rec = null; recK = null; recState = null; clearInterval(tick); level = 0;
    const d = drafts[k];
    const s = await r.stop(`REC ${d.files.length + 1}`);
    if (!s) { toastKey("warn", "smp.recSilent"); return; }
    d.files.push(s);
    if (!d.name) d.name = "REC";
    preview(s);                                    // heard back at once
  }

  // ----------------------------------------------------------------- upload ---
  async function upload(k: number) {
    const d = drafts[k];
    let b: { hdr: Uint8Array; data: Uint8Array; zones: { mix?: number[] }[] };
    try { b = buildSlot(d.name || `USR${k + 1}`, d.kit ? kitZones(d.lanes) : d.files.map((f) => ({ s: sample(f), root: f.root, mix: f.mix }))); }
    catch (e) { toast("err", String((e as Error).message)); return; }
    const was = info?.slots[k];
    const body = t(was?.zones ? "smp.confirmReplace" : "smp.confirmNew", { n: k + 1, name: d.name, old: was?.name ?? "", count: sounds(d).length,
      kb: Math.round((b.data.length + 512) / 1024), s: secondsOf(b.data.length).toFixed(1) });
    if (!(await ask({ title: t("smp.uploadTitle", { n: k + 1 }), body, ok: t("smp.send"), danger: !!was?.zones }))) return;
    stopPreview();
    busy = { k, pct: 0, what: "send" };
    try {
      await device.stop();                         // (the unit stops its transport for a flash write anyway)
      await device.writing("sample", async () => {
        let r = await device.ask<{ rc: number }>(req.smpBegin(k), { timeout: 4000, retries: 1 });
        if (r.rc) throw new Error("begin rc " + r.rc);
        for (let off = 0; off < b.data.length; off += 256) {
          const w = await device.ask<{ rc: number }>(req.smpWrite(k, 512 + off, b.data.subarray(off, off + 256)), { timeout: 1500, retries: 2 });
          if (w.rc) throw new Error("write rc " + w.rc);
          busy = { k, pct: Math.min(100, Math.round((100 * (off + 256)) / b.data.length)), what: "send" };
        }
        r = await device.ask<{ rc: number }>(req.smpEnd(k, b.hdr), { timeout: 4000, retries: 1 });
        if (r.rc) throw new Error("end rc " + r.rc);
        if (mixOn && !d.kit) {
          const m = await device.ask<{ rc: number }>(req.smpMix(k, b.zones.map((z) => z.mix || MIX0)), { retries: 2 });
          if (m.rc === 1) throw new Error("mix rc 1");
        }
      }, () => upload(k));
      device.interrupted = null;
      drafts[k] = { name: "", kit: d.kit, files: [], lanes: LANE_NAMES.map(() => null) };
      toastKey("ok", "smp.done", { n: k + 1 });
    } catch (e) {
      const m = String((e as Error).message);
      if (m !== "interrupted") toastKey("err", "smp.failed", { detail: m });
    } finally { busy = null; await read().catch(() => {}); }
  }
  async function erase(k: number) {
    if (!(await ask({ title: t("smp.eraseTitle", { n: k + 1 }), body: t("smp.eraseBody", { n: k + 1, name: info?.slots[k]?.name ?? "" }), ok: t("dlg.erase"), danger: true }))) return;
    busy = { k, pct: 0, what: "erase" };
    try { await device.stop(); const r = await device.ask<{ rc: number }>(req.smpErase(k), { timeout: 4000, retries: 1 }); if (r.rc) throw new Error("rc " + r.rc); toastKey("ok", "smp.erased", { n: k + 1 }); }
    catch (e) { toastKey("err", "smp.failed", { detail: String((e as Error).message) }); }
    finally { busy = null; await read().catch(() => {}); }
  }
  async function setMix(k: number, i: number, w: number, pct: number) {
    const u = info?.slots[k];
    if (!u?.mix) return;
    const mix = u.mix.mix.map((m) => m.slice());
    mix[i][w] = rawOf(Math.max(0, Math.min(100, Math.round(pct) || 0)));
    try {
      const r = await device.ask(req.smpMix(k, mix), { key: `smix${k}`, retries: 1 });
      if (r.rc === 1) throw new Error("mix rc 1");
      u.mix = r;
      if (r.rc === 2) toastKey("warn", "smp.mixNotKept");
    } catch (e) { toastKey("err", "smp.failed", { detail: String((e as Error).message) }); }
  }
  const MIXK = ["smp.level", "smp.rev", "smp.dly"];
  const off = $derived(!!busy || recK != null);
</script>

<div class="smp">
  <p class="what">{t("smp.what")}</p>
  {#if info}
    <div class="slots">
      {#each info.slots as u, k}
        {@const d = drafts[k]}
        {@const bytes = used(d)}
        {@const big = bytes > MAX_DATA}
        <section class="slot" aria-label="USR{k + 1}" class:over={over === "s" + k} data-testid="slot-{k}"
                 ondragover={(e) => { e.preventDefault(); if (!d.kit) over = "s" + k; }} ondragleave={() => (over = null)} ondrop={(e) => drop(k, e)}>
          <header>
            <h3 class="dot">USR{k + 1}</h3>
            <div class="on">
              <span class="nm dot">{u.zones ? u.name : t("smp.empty")}</span>
              {#if u.zones}<span class="meta">{t("smp.onUnit", { n: u.zones, kb: u.kib, max: info.slotKiB })}</span>{/if}
            </div>
            {#if u.zones}<Button size="s" variant="ghost" disabled={off} onclick={() => erase(k)}>{t("smp.erase")}</Button>{/if}
          </header>
          <div class="bar" title="{u.kib} / {info.slotKiB} KB"><div style="width:{(u.kib / info.slotKiB) * 100}%"></div></div>

          {#if u.zones && u.mix && !d.kit && !d.files.length}
            <table class="mix">
              <thead><tr><th>{t("smp.sound")}</th><th>{t("smp.keys")}</th>{#each MIXK as m}<th>{t(m)}</th>{/each}</tr></thead>
              <tbody>
                {#each u.mix.mix as m, i}
                  <tr><td>{i + 1}</td><td class="dot">{noteName(u.mix.keys[i][0])}–{noteName(u.mix.keys[i][1])}</td>
                    {#each m as v, w}<td><input type="number" min="0" max="100" value={pctOf(v)} disabled={off} aria-label="{t(MIXK[w])} {i + 1}"
                      onchange={(e) => setMix(k, i, w, +(e.currentTarget as HTMLInputElement).value)} /> %</td>{/each}</tr>
                {/each}
              </tbody>
            </table>
          {/if}

          <div class="new">
            <div class="row">
              <div class="seg" role="radiogroup" aria-label={t("smp.kind")}>
                <button role="radio" aria-checked={!d.kit} class:sel={!d.kit} disabled={off} onclick={() => (d.kit = false)}>{t("smp.sounds")}</button>
                <button role="radio" aria-checked={d.kit} class:sel={d.kit} disabled={off} onclick={() => (d.kit = true)}>{t("smp.kit")}</button>
              </div>
              <label class="name">{t("smp.name")}
                <input class="dot" maxlength="8" spellcheck="false" placeholder="NAME" value={d.name} disabled={off}
                       oninput={(e) => { const el = e.currentTarget as HTMLInputElement; d.name = el.value.toUpperCase().replace(/[^\x20-\x7E]/g, "").slice(0, 8); el.value = d.name; }} />
              </label>
            </div>

            {#if !d.kit}
              {#each d.files as f, i (f)}
                {@const keys = keysOf(d.files)}
                <div class="snd">
                  <div class="sr">
                    <span class="fn" title={f.fname}>{f.fname}</span>
                    <label class="root">{t("smp.root")} <input class="dot" value={noteName(f.root)} disabled={off}
                      onchange={(e) => { const n = parseNote((e.currentTarget as HTMLInputElement).value.trim()); if (n != null) f.root = n; else (e.currentTarget as HTMLInputElement).value = noteName(f.root); }} /></label>
                    <span class="meta dot">{noteName(keys[i][0])}–{noteName(keys[i][1])}</span>
                    <span class="meta">{((f.b - f.a) / RATE).toFixed(2)} s</span>
                    <button class="ic" title={t("smp.preview")} aria-label="{t('smp.preview')} {f.fname}" onclick={() => preview(f)}>▶</button>
                    <button class="ic" title={t("smp.autoTrim")} disabled={off} onclick={() => { [f.a, f.b] = autoTrim(f.raw); }}>{t("smp.trim")}</button>
                    <button class="ic" title={t("smp.remove")} aria-label="{t('smp.remove')} {f.fname}" disabled={off} onclick={() => d.files.splice(i, 1)}>✕</button>
                  </div>
                  <TrimWave raw={f.raw} bind:a={f.a} bind:b={f.b} disabled={off} label="{f.fname}: {t('smp.trimLabel')}" />
                  {#if mixOn}
                    <div class="fmix">{#each MIXK as m, w}<label>{t(m)} <input type="number" min="0" max="100" value={pctOf(f.mix[w])} disabled={off}
                      onchange={(e) => (f.mix[w] = rawOf(Math.max(0, Math.min(100, +(e.currentTarget as HTMLInputElement).value || 0))))} /> %</label>{/each}</div>
                  {/if}
                </div>
              {/each}
              {#if !d.files.length}<p class="hint">{t("smp.dropHint")}</p>{/if}
              <div class="acts">
                <Button size="s" onclick={() => pick(k)} disabled={off || d.files.length >= MAX_ZONES}>{t("smp.addFiles")}</Button>
                {#if recK === k}
                  <Button size="s" variant="primary" onclick={recStop} disabled={recState === "count"}>{recState === "count" ? count : "■ " + t("smp.stopRec")}</Button>
                {:else}
                  <Button size="s" onclick={() => recStart(k)} disabled={off}>● {t("smp.record")}</Button>
                {/if}
                {#if d.files.length}<Button size="s" variant="ghost" disabled={off} onclick={() => (d.files = [])}>{t("smp.clearDraft")}</Button>{/if}
              </div>
              {#if recK === k}
                <div class="rec" aria-live="polite">
                  {#if recState === "count"}<span class="cnt">{t("smp.countIn", { n: count })}</span>
                  {:else}
                    <span class="recdot"></span>
                    <div class="vu" role="meter" aria-label={t("smp.level")} aria-valuenow={Math.round(level * 100)} aria-valuemin={0} aria-valuemax={100}>
                      <div style="width:{level * 100}%" class:clip={level > 0.98}></div>
                    </div>
                    <span class="meta">{secs.toFixed(1)} / {maxSecs.toFixed(1)} s</span>
                  {/if}
                </div>
              {/if}
            {:else}
              <p class="hint">{t("smp.kitHelp", { n: k + 1 })}</p>
              <div class="lanes">
                {#each d.lanes as f, i}
                  <div class="lane" role="group" aria-label={LANE_NAMES[i]} class:over={over === `l${k}-${i}`} data-testid="lane-{k}-{i}"
                       ondragover={(e) => { e.preventDefault(); e.stopPropagation(); over = `l${k}-${i}`; }} ondragleave={() => (over = null)} ondrop={(e) => drop(k, e, i)}>
                    <b class="dot">{LANE_NAMES[i]}</b>
                    {#if f}
                      <div class="lw"><TrimWave raw={f.raw} bind:a={f.a} bind:b={f.b} height={34} disabled={off} label="{LANE_NAMES[i]}: {t('smp.trimLabel')}" /></div>
                      <label class="lv" title={t("smp.level")}><input type="range" min="0" max="100" value={Math.round(f.lvl * 100)} disabled={off} aria-label="{t('smp.level')} {LANE_NAMES[i]}"
                        oninput={(e) => (f.lvl = +(e.currentTarget as HTMLInputElement).value / 100)} /></label>
                      <button class="ic" aria-label="{t('smp.preview')} {LANE_NAMES[i]}" onclick={() => preview(f)}>▶</button>
                      <button class="ic" aria-label="{t('smp.remove')} {LANE_NAMES[i]}" disabled={off} onclick={() => (d.lanes[i] = null)}>✕</button>
                    {:else}
                      <button class="choose" disabled={off} onclick={() => pickLane(k, i)}>{t("smp.laneEmpty")}</button>
                    {/if}
                  </div>
                {/each}
              </div>
              <div class="acts">
                <Button size="s" onclick={() => sliceFile(k)} disabled={off}>{t("smp.slice")}</Button>
                {#if d.lanes.some(Boolean)}<Button size="s" variant="ghost" disabled={off} onclick={() => (d.lanes = LANE_NAMES.map(() => null))}>{t("smp.clearDraft")}</Button>{/if}
              </div>
            {/if}

            {#if sounds(d).length}
              <div class="room" class:bad={big}>
                <div class="gauge"><div style="width:{Math.min(100, (bytes / MAX_DATA) * 100)}%"></div></div>
                <span>{t("smp.room", { s: secondsOf(bytes).toFixed(1), max: secondsOf(MAX_DATA).toFixed(1), n: sounds(d).length, kb: (bytes / 1024).toFixed(1), maxkb: Math.round(MAX_DATA / 1024) })}</span>
                {#if big}<b>{t("smp.tooBig")}</b>{/if}
              </div>
            {/if}
            {#if busy?.k === k}
              <div class="room"><div class="gauge up"><div style="width:{busy.pct}%"></div></div><span>{busy.what === "erase" ? t("smp.erasing") : t("smp.uploading", { pct: busy.pct })}</span></div>
            {/if}
            {#if sounds(d).length}
              <Button variant="primary" disabled={off || big || !d.name} onclick={() => upload(k)}>{t("smp.sendTo", { n: k + 1 })}</Button>
            {/if}
          </div>
        </section>
      {/each}
    </div>
    <div class="recopts">
      <label>{t("smp.input")}
        <select bind:value={inputId} disabled={recK != null}>
          <option value="">{t("smp.inputDefault")}</option>
          {#each ins as d, i}<option value={d.deviceId}>{d.label || `${t("smp.input")} ${i + 1}`}</option>{/each}
        </select>
      </label>
      <label><input type="checkbox" bind:checked={countIn} /> {t("smp.countInOpt")}</label>
    </div>
  {:else}<p class="meta">…</p>{/if}
</div>

<style>
  .smp { display: flex; flex-direction: column; gap: 14px; max-width: 1500px; }
  .what { margin: 0; color: var(--fg-2); max-width: 75ch; line-height: 1.5; }
  .slots { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 12px; align-items: start; }
  .slot { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 14px; display: flex; flex-direction: column; gap: 10px; }
  .slot.over, .lane.over { border-color: var(--accent); box-shadow: 0 0 0 2px color-mix(in srgb, var(--accent) 30%, transparent); }
  header { display: flex; align-items: center; gap: 12px; }
  h3 { margin: 0; font-size: 20px; font-weight: 400; color: var(--accent); }
  .on { flex: 1; display: flex; flex-direction: column; min-width: 0; }
  .nm { font-size: 15px; }
  .meta { font-size: 12px; color: var(--dim); margin: 0; white-space: nowrap; }
  .bar, .gauge { height: 5px; background: var(--surf-3); border-radius: 3px; overflow: hidden; }
  .bar div, .gauge div { height: 100%; background: var(--accent); }
  .gauge.up div { background: var(--fg); }
  .mix { border-collapse: collapse; font-size: 12px; width: 100%; }
  .mix th { text-align: left; color: var(--dim); font-weight: 500; padding: 2px 4px; }
  .mix td { padding: 2px 4px; color: var(--fg-2); }
  .mix input, .fmix input { width: 48px; background: var(--surf-3); border: 1px solid var(--line); border-radius: 5px; color: var(--fg); padding: 2px 4px; }
  .new { display: flex; flex-direction: column; gap: 10px; border-top: 1px solid var(--line); padding-top: 10px; }
  .row { display: flex; align-items: center; gap: 10px; flex-wrap: wrap; }
  .seg { display: inline-flex; background: var(--surf-3); border-radius: 8px; padding: 2px; }
  .seg button { border: 0; background: transparent; color: var(--fg-2); padding: 5px 10px; border-radius: 6px; font-size: 12.5px; }
  .seg button.sel { background: var(--surf); color: var(--fg); box-shadow: 0 0 0 1px var(--line); }
  .name { display: flex; align-items: center; gap: 6px; font-size: 12.5px; color: var(--dim); }
  .name input { width: 92px; background: var(--surf-3); border: 1px solid var(--line); border-radius: 6px; color: var(--fg); padding: 4px 6px; font-size: 14px; }
  .snd { background: var(--surf-2); border-radius: 8px; padding: 8px; display: flex; flex-direction: column; gap: 6px; }
  .sr { display: flex; gap: 8px; align-items: center; font-size: 12px; color: var(--dim); }
  .fn { flex: 1; min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: var(--fg-2); }
  .root input { width: 42px; background: var(--surf-3); border: 1px solid var(--line); border-radius: 5px; color: var(--fg); padding: 2px 4px; }
  .ic { border: 0; background: var(--surf-4); color: var(--fg-2); border-radius: 5px; padding: 3px 7px; font-size: 11.5px; min-width: 26px; }
  .ic:hover:not(:disabled) { color: var(--fg); }
  .fmix { display: flex; gap: 12px; font-size: 11.5px; color: var(--dim); }
  .hint { margin: 0; font-size: 12.5px; color: var(--dim); line-height: 1.45; }
  .acts { display: flex; gap: 6px; flex-wrap: wrap; }
  .rec { display: flex; align-items: center; gap: 10px; padding: 6px 8px; background: var(--surf-2); border-radius: 8px; }
  .cnt { font-size: 13px; color: var(--fg); }
  .recdot { width: 9px; height: 9px; border-radius: 50%; background: var(--err); animation: bl 1s infinite; }
  @keyframes bl { 50% { opacity: 0.35; } }
  .vu { flex: 1; height: 8px; background: var(--surf-3); border-radius: 4px; overflow: hidden; }
  .vu div { height: 100%; background: var(--ok); transition: width 50ms linear; }
  .vu div.clip { background: var(--err); }
  .lanes { display: flex; flex-direction: column; gap: 4px; }
  .lane { display: grid; grid-template-columns: 56px 1fr 70px auto auto; align-items: center; gap: 6px; border: 1px solid transparent; border-radius: 7px; padding: 2px 4px; background: var(--surf-2); min-height: 40px; }
  .lane b { font-weight: 400; font-size: 12.5px; color: var(--fg-2); }
  .lane .choose { grid-column: 2 / -1; text-align: left; border: 1px dashed var(--line); background: transparent; color: var(--dim); border-radius: 6px; padding: 7px 8px; font-size: 12px; }
  .lane .choose:hover:not(:disabled) { color: var(--fg); border-color: var(--fg-2); }
  .lw { min-width: 0; }
  .lv input { width: 66px; }
  .room { display: flex; flex-direction: column; gap: 4px; font-size: 12px; color: var(--fg-2); }
  .room.bad { color: var(--warn); }
  .room.bad .gauge div { background: var(--warn); }
  .recopts { display: flex; gap: 18px; align-items: center; font-size: 12.5px; color: var(--fg-2); flex-wrap: wrap; }
  .recopts select { margin-left: 6px; background: var(--surf-3); border: 1px solid var(--line); color: var(--fg); border-radius: 6px; padding: 3px 6px; max-width: 280px; }
  @media (max-width: 1250px) { .slots { grid-template-columns: 1fr; } }
  @media (prefers-reduced-motion: reduce) { .recdot { animation: none; } }
</style>
