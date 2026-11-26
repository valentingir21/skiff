<script lang="ts">
  // The three user sample slots (USR1..USR3, 80 KB each): what is in them, a new sample from WAV files (one sound per
  // file, its root from the file name: "pad_C3.wav"; the original editor's pipeline, byte for byte the firmware's
  // tools), with a trim and a look at it before anything is written, and erasing. Every write asks first.
  import { onMount } from "svelte";
  import { device } from "../lib/model/device.svelte";
  import { req, SMP, parseWav, resample, takeSample, autoTrim, rootFromName, buildSlot, noteName } from "../lib/legacy/proto.js";
  import { ask, toast, pickFile } from "../lib/ui.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import Button from "../ui/components/Button.svelte";

  interface Draft { fname: string; raw: Float64Array; a: number; b: number; root: number }
  let info = $state<{ nslots: number; slotKiB: number; slots: { zones: number; name: string; kib: number }[] } | null>(null);
  let drafts = $state<Record<number, Draft[]>>({});
  let busy = $state<{ k: number; pct: number } | null>(null);
  async function read() { info = await device.ask(req.smpInfo()); }
  onMount(() => { read().catch(() => {}); });

  async function decode(f: File): Promise<Draft> {
    const buf = await f.arrayBuffer();
    let x: Float64Array;
    try { const w = parseWav(buf); x = resample(w.x, w.sr, SMP.RATE); }
    catch {
      const ab = await new OfflineAudioContext(1, 1, SMP.RATE).decodeAudioData(buf.slice(0));
      x = new Float64Array(ab.length);
      for (let c = 0; c < ab.numberOfChannels; c++) { const d = ab.getChannelData(c); for (let i = 0; i < d.length; i++) x[i] += d[i] / ab.numberOfChannels; }
    }
    const raw = Float64Array.from(x);
    return { fname: f.name, raw, a: 0, b: raw.length, root: rootFromName(f.name.replace(/\.[^.]*$/, "")) };
  }
  async function add(k: number) {
    const fs = await pickFile("audio/*,.wav", true);
    try { drafts[k] = [...(drafts[k] ?? []), ...(await Promise.all(fs.map(decode)))].slice(0, 16); }
    catch (e) { toast("err", String((e as Error).message)); }
  }
  function trim(k: number, i: number) { const d = drafts[k][i]; const [a, b] = autoTrim(d.raw); d.a = a; d.b = b; }
  const bytes = (k: number) => { try { return built(k).data.length + 512; } catch { return Infinity; } };
  function built(k: number) {
    const ds = drafts[k] ?? [];
    return buildSlot(ds[0]?.fname.replace(/\.[^.]*$/, "").slice(0, 8).toUpperCase() || "USR" + (k + 1), ds.map((d) => ({ s: takeSample(d.raw, d.a, d.b), root: d.root })));
  }
  async function upload(k: number) {
    let b: { hdr: Uint8Array; data: Uint8Array };
    try { b = built(k); } catch (e) { toast("err", String((e as Error).message)); return; }
    if (!(await ask({ title: t("smp.upload"), body: `USR${k + 1}: ${info?.slots[k]?.zones ? info.slots[k].name + " → " : ""}${drafts[k].length} sound(s), ${Math.round((b.data.length + 512) / 1024)} KB?`, ok: t("dlg.save"), danger: !!info?.slots[k]?.zones }))) return;
    busy = { k, pct: 0 };
    try {
      await device.stop();
      let r = await device.ask<{ rc: number }>(req.smpBegin(k), { timeout: 1500, retries: 0 });
      if (r.rc) throw new Error("begin rc " + r.rc);
      for (let off = 0; off < b.data.length; off += 256) {
        const w = await device.ask<{ rc: number }>(req.smpWrite(k, SMP.DATA_OFF + off, b.data.subarray(off, off + 256)), { timeout: 1500, retries: 1 });
        if (w.rc) throw new Error("write rc " + w.rc);
        busy = { k, pct: Math.round((100 * (off + 256)) / b.data.length) };
      }
      r = await device.ask<{ rc: number }>(req.smpEnd(k, b.hdr), { timeout: 2500, retries: 0 });
      if (r.rc) throw new Error("end rc " + r.rc);
      delete drafts[k];
      toast("ok", `USR${k + 1} ✓`);
    } catch (e) { toast("err", String((e as Error).message)); }
    finally { busy = null; await read().catch(() => {}); }
  }
  async function erase(k: number) {
    if (!(await ask({ title: t("smp.erase"), body: `USR${k + 1} (${info?.slots[k]?.name})?`, ok: t("dlg.erase"), danger: true }))) return;
    try { await device.stop(); await device.ask(req.smpErase(k), { timeout: 3000, retries: 0 }); } catch (e) { toast("err", String((e as Error).message)); }
    await read().catch(() => {});
  }
  function wave(node: HTMLCanvasElement, d: Draft) {
    const draw = (d: Draft) => {
      const c = node.getContext("2d")!, w = (node.width = node.clientWidth * 2), h = (node.height = 80);
      c.clearRect(0, 0, w, h);
      const n = d.raw.length;
      c.fillStyle = "rgba(255,255,255,0.08)";
      c.fillRect((d.a / n) * w, 0, ((d.b - d.a) / n) * w, h);
      c.strokeStyle = getComputedStyle(node).color;
      c.beginPath();
      for (let x = 0; x < w; x++) {
        let mx = 0;
        for (let i = Math.floor((x / w) * n); i < Math.floor(((x + 1) / w) * n); i++) mx = Math.max(mx, Math.abs(d.raw[i]));
        c.moveTo(x, h / 2 - mx * h * 0.45); c.lineTo(x, h / 2 + mx * h * 0.45);
      }
      c.stroke();
    };
    draw(d);
    return { update: draw };
  }
</script>

<div class="smp">
  <p class="what">{t("smp.what")}</p>
  {#if info}
    <div class="slots">
      {#each info.slots as s, k}
        {@const ds = drafts[k] ?? []}
        <section class="slot">
          <header><h3 class="dot">USR{k + 1}</h3><span class="nm dot">{s.zones ? s.name : t("smp.empty")}</span>
            {#if s.zones}<span class="meta">{s.zones} sound(s) · {s.kib} KB</span>{/if}</header>
          <div class="bar"><div style="width:{(s.kib / info.slotKiB) * 100}%"></div></div>
          {#each ds as d, i}
            <div class="draft">
              <canvas use:wave={d}></canvas>
              <div class="dr"><span class="fn">{d.fname}</span><span>root {noteName(d.root)}</span>
                <button onclick={() => trim(k, i)}>trim</button>
                <button onclick={() => (drafts[k] = ds.filter((_, j) => j !== i))}>×</button></div>
            </div>
          {/each}
          {#if ds.length}<p class="meta">{Math.round(bytes(k) / 1024)} / {info.slotKiB} KB</p>{/if}
          {#if busy?.k === k}<div class="bar up"><div style="width:{busy.pct}%"></div></div><p class="meta">{t("smp.uploading", { pct: busy.pct })}</p>{/if}
          <div class="acts">
            <Button size="s" onclick={() => add(k)} disabled={!!busy}>{ds.length ? "+ WAV" : t("smp.upload")}</Button>
            {#if ds.length}<Button size="s" variant="primary" disabled={!!busy || bytes(k) > info.slotKiB * 1024} onclick={() => upload(k)}>{t("dlg.save")}</Button>{/if}
            {#if s.zones}<Button size="s" variant="ghost" disabled={!!busy} onclick={() => erase(k)}>{t("smp.erase")}</Button>{/if}
          </div>
        </section>
      {/each}
    </div>
  {:else}<p class="meta">…</p>{/if}
</div>

<style>
  .smp { display: flex; flex-direction: column; gap: 14px; max-width: 1200px; }
  .what { margin: 0; color: var(--fg-2); }
  .slots { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; }
  .slot { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 14px; display: flex; flex-direction: column; gap: 10px; }
  header { display: flex; align-items: baseline; gap: 10px; flex-wrap: wrap; }
  h3 { margin: 0; font-size: 18px; font-weight: 400; color: var(--accent); }
  .nm { font-size: 15px; }
  .meta { font-size: 12px; color: var(--dim); margin: 0; }
  .bar { height: 4px; background: var(--surf-3); border-radius: 3px; overflow: hidden; }
  .bar div { height: 100%; background: var(--accent); }
  .draft { background: var(--surf-2); border-radius: 8px; padding: 6px; }
  canvas { width: 100%; height: 40px; display: block; color: var(--accent); }
  .dr { display: flex; gap: 8px; align-items: center; font-size: 11.5px; color: var(--dim); }
  .fn { flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: var(--fg-2); }
  .dr button { border: 0; background: var(--surf-4); border-radius: 5px; padding: 2px 7px; font-size: 11px; }
  .acts { display: flex; gap: 6px; flex-wrap: wrap; margin-top: auto; }
  @media (max-width: 1100px) { .slots { grid-template-columns: 1fr; } }
</style>
