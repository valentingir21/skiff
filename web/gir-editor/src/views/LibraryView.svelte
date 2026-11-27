<script lang="ts">
  // Sounds in two places: the FM-1's 32 user slots (U01..U32) and this computer (IndexedDB, files in the original
  // editor's felucca-library format). Drag between them, or use each row's actions. Writing a slot asks first.
  import { onMount } from "svelte";
  import { device } from "../lib/model/device.svelte";
  import { ask, toast, toastKey, download, pickFile, TRACK_COLORS } from "../lib/ui.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import { kvGet, kvSet } from "../lib/store";
  import { bank, libraryFile, readLibraryFile, paramKeys, auditionPatch, cleanPatch } from "../lib/legacy/proto.js";
  import Button from "../ui/components/Button.svelte";

  interface Patch { name: string; engine: number; engineName?: string; p: number[]; pattern?: unknown; grid?: unknown; fm6?: number[]; tags?: string[]; created?: string; modified?: string; id?: string }
  let lib = $state<Patch[]>([]);
  let q = $state("");
  let busy = $state(false);
  let over = $state<string | null>(null);
  let drag = $state<{ from: "slot"; slot: number } | { from: "lib"; i: number } | null>(null);
  const k = $derived(device.sel);
  const rq = (r: [number, number[]], o?: any) => device.rq(r, o);
  const ctx = $derived({ firmware: device.info?.version, keys: device.info ? paramKeys([...device.tdesc, ...Array(8).fill(null)], device.pe0, device.info.pcount) : null, pe0: device.pe0, engines: device.info?.engines });

  onMount(async () => {
    lib = (await kvGet<Patch[]>("library")) ?? [];
    device.readUserSlots().catch(() => {});
  });
  async function saveLib() { await kvSet("library", $state.snapshot(lib)); }
  const shown = $derived(lib.map((x, i) => ({ x, i })).filter(({ x }) => !q || (x.name + " " + (x.engineName ?? "")).toLowerCase().includes(q.toLowerCase())));
  const engName = (e: number) => device.engineName(e);

  async function slotToLib(slot: number) {
    busy = true;
    try {
      const u = await bank.get(rq, device.info, slot);
      if (!u.used) return;
      lib = [...lib, { ...cleanPatch({ ...u, engineName: engName(u.engine) }), created: new Date().toISOString() }];
      await saveLib();
      toastKey("ok", "toast.copied");
    } catch (e) { toast("err", String((e as Error).message)); } finally { busy = false; }
  }
  async function libToSlot(i: number, slot: number) {
    const s = device.userSlots[slot], x = lib[i];
    if (!(await ask({ title: `U${String(slot + 1).padStart(2, "0")}`, body: s?.used ? `${s.name} → ${x.name}?` : `${x.name} → U${String(slot + 1).padStart(2, "0")}?`, ok: t("dlg.save"), danger: !!s?.used }))) return;
    busy = true;
    try {
      const rc = await bank.put(rq, slot, $state.snapshot(x), device.info);
      if (rc) throw new Error("rc " + rc);
      await device.readUserSlots();
      toastKey("ok", "toast.saved", { s: "U" + String(slot + 1).padStart(2, "0") });
    } catch (e) { toast("err", String((e as Error).message)); } finally { busy = false; }
  }
  async function storeHere(slot: number) {
    const s = device.userSlots[slot];
    const name = await ask({ title: t("lib.store"), body: s?.used ? `${s.name} → ${device.tracks[k]?.name}` : "", ok: t("dlg.save"), danger: !!s?.used,
                             input: { label: t("proj.name"), value: device.tracks[k]?.name || "", max: 12 } });
    if (name == null) return;
    try { await device.storeUser(slot, String(name)); toastKey("ok", "toast.saved", { s: "U" + String(slot + 1).padStart(2, "0") }); } catch (e) { toast("err", String((e as Error).message)); }
  }
  async function erase(slot: number) {
    if (!(await ask({ title: t("lib.erase"), body: `${t("lib.erase")} U${String(slot + 1).padStart(2, "0")} (${device.userSlots[slot]?.name})?`, ok: t("dlg.erase"), danger: true }))) return;
    try { await device.eraseUser(slot); } catch (e) { toast("err", String((e as Error).message)); }
  }
  async function audition(i: number) {
    busy = true;
    try {
      await auditionPatch(rq, device.info, $state.snapshot(lib[i]), { track: k, gEng: 20 });
      const d = await device.ask<{ p: number[]; engine: number; preset: number }>([29, [k]]);
      device.tracks[k] = { ...device.tracks[k], engine: d.engine, preset: d.preset, p: d.p, name: lib[i].name };
      await device.readEngineDesc(d.engine);
    } catch (e) { toast("err", String((e as Error).message)); } finally { busy = false; }
  }
  function exportLib(only?: number) {
    const ps = only == null ? lib : [lib[only]];
    download(`${only == null ? "Gir-library" : ps[0].name.replace(/[^A-Za-z0-9_-]+/g, "-")}-${new Date().toISOString().slice(0, 10)}.json`, JSON.stringify(libraryFile("library", $state.snapshot(ps), ctx), null, 1));
  }
  async function importFiles(files?: File[]) {
    files = files ?? (await pickFile(".json,application/json", true));
    let n = 0, skipped = 0;
    for (const f of files) {
      try { const r = readLibraryFile(JSON.parse(await f.text()), ctx); lib = [...lib, ...r.patches.map((x: Patch) => ({ ...x, engineName: engName(x.engine) }))]; n += r.patches.length; skipped += r.skipped; }
      catch (e) { toast("err", `${f.name}: ${(e as Error).message}`); }
    }
    await saveLib();
    if (n) toast("ok", `+${n}${skipped ? ` (${skipped} skipped)` : ""}`);
  }
  async function remove(i: number) {
    if (!(await ask({ title: t("lib.erase"), body: lib[i].name + "?", ok: t("dlg.erase"), danger: true }))) return;
    lib = lib.filter((_, j) => j !== i); await saveLib();
  }
  async function rename(i: number) {
    const n = await ask({ title: t("proj.rename"), body: "", ok: t("proj.rename"), input: { label: t("proj.name"), value: lib[i].name, max: 12 } });
    if (n == null) return;
    lib[i].name = String(n).toUpperCase().slice(0, 12); await saveLib();
  }
  function dropOnSlot(slot: number) { const d = drag; drag = null; over = null; if (d?.from === "lib") libToSlot(d.i, slot); }
  function dropOnLib(e: DragEvent) {
    e.preventDefault(); over = null;
    const d = drag; drag = null;
    if (d?.from === "slot") slotToLib(d.slot);
    else if (e.dataTransfer?.files.length) importFiles([...e.dataTransfer.files]);
  }
</script>

<div class="lib">
  <section class="dev">
    <header><h2>{t("lib.device")}</h2><span class="cnt">{device.userSlots.filter((s) => s.used).length} / 32</span></header>
    <div class="slots">
      {#each device.userSlots as s}
        <div class="slot" class:used={!!s.used} class:over={over === "s" + s.slot} role="listitem" draggable={!!s.used}
             ondragstart={() => (drag = { from: "slot", slot: s.slot })} ondragover={(e) => { if (drag?.from === "lib") { e.preventDefault(); over = "s" + s.slot; } }}
             ondragleave={() => (over = null)} ondrop={(e) => { e.preventDefault(); dropOnSlot(s.slot); }}>
          <span class="u dot">U{String(s.slot + 1).padStart(2, "0")}</span>
          <span class="nm dot">{s.used ? s.name : t("lib.emptySlot")}</span>
          {#if s.used}<span class="en">{engName(s.engine)}</span>{/if}
          <span class="acts">
            {#if s.used}
              <button onclick={() => device.loadUser(k, s.slot).catch((e) => toast("err", e.message))} title={t("lib.load") + ` T${k + 1}`} aria-label={t("lib.load") + ` T${k + 1}`} style="color:{TRACK_COLORS[k]}">▶</button>
              <button onclick={() => slotToLib(s.slot)} title={t("lib.toComputer")} aria-label={t("lib.toComputer")}>↓</button>
              <button onclick={() => erase(s.slot)} title={t("lib.erase")} aria-label={t("lib.erase")}>×</button>
            {/if}
            <button onclick={() => storeHere(s.slot)} title={t("lib.store")} aria-label={t("lib.store")}>＋</button>
          </span>
        </div>
      {/each}
    </div>
  </section>

  <section class="pc" class:over={over === "lib"} aria-label={t("lib.computer")}
           ondragover={(e) => { e.preventDefault(); over = "lib"; }} ondragleave={() => (over = null)} ondrop={dropOnLib}>
    <header>
      <h2>{t("lib.computer")}</h2><span class="cnt">{lib.length}</span>
      <input class="q" bind:value={q} placeholder={t("sound.search")} aria-label={t("sound.search")} />
      <Button size="s" onclick={() => importFiles()}>{t("lib.import")}</Button>
      <Button size="s" variant="ghost" disabled={!lib.length} onclick={() => exportLib()}>{t("lib.export")}</Button>
    </header>
    {#if !lib.length}
      <p class="empty">{t("lib.empty")}<br /><span>{t("lib.drop")}</span></p>
    {:else}
      <ul>
        {#each shown as { x, i } (i)}
          <li draggable="true" ondragstart={() => (drag = { from: "lib", i })}>
            <span class="nm dot">{x.name}</span><span class="en">{x.engineName ?? engName(x.engine)}</span>
            <span class="acts">
              <button disabled={busy} onclick={() => audition(i)} title={t("lib.load") + ` T${k + 1}`} style="color:{TRACK_COLORS[k]}">▶ T{k + 1}</button>
              <button onclick={() => rename(i)} title={t("proj.rename")} aria-label={t("proj.rename")}>✎</button>
              <button onclick={() => exportLib(i)} title={t("lib.export")} aria-label={t("lib.export")}>↧</button>
              <button onclick={() => remove(i)} title={t("lib.erase")} aria-label={t("lib.erase")}>×</button>
            </span>
          </li>
        {/each}
      </ul>
    {/if}
  </section>
</div>

<style>
  .lib { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; align-items: start; }
  header { display: flex; align-items: center; gap: 10px; margin-bottom: 10px; }
  h2 { margin: 0; font-size: 13px; color: var(--fg-2); }
  .cnt { color: var(--dim); font-size: 12px; }
  .slots { display: grid; grid-template-columns: 1fr 1fr; gap: 4px; }
  .slot { display: grid; grid-template-columns: 38px 1fr auto; grid-template-rows: auto auto; align-items: center; column-gap: 8px; padding: 6px 8px; border-radius: 8px; background: var(--surf); border: 1px solid var(--line); min-height: 46px; }
  .slot.used { cursor: grab; }
  .slot:not(.used) .nm { color: var(--mute); }
  .slot.over, .pc.over { outline: 2px dashed var(--accent); }
  .u { grid-row: span 2; font-size: 13px; color: var(--dim); }
  .nm { font-size: 14px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .en { font-size: 10.5px; color: var(--dim); font-weight: 700; }
  .slot .acts { grid-row: 1 / span 2; grid-column: 3; display: flex; gap: 2px; }
  .acts button { min-width: 26px; height: 26px; border-radius: 6px; border: 0; background: none; color: var(--dim); font-weight: 700; }
  .acts button:hover:not(:disabled) { background: var(--surf-3); color: var(--fg); }
  .pc { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 12px; min-height: 300px; }
  .q { flex: 1; height: 28px; border-radius: 7px; border: 1px solid var(--line-2); background: var(--bg); padding: 0 10px; min-width: 80px; }
  ul { list-style: none; margin: 0; padding: 0; display: flex; flex-direction: column; gap: 2px; }
  li { display: grid; grid-template-columns: 1fr auto auto; align-items: center; gap: 10px; padding: 5px 8px; border-radius: 7px; cursor: grab; }
  li:hover { background: var(--surf-2); }
  li .acts { display: flex; gap: 2px; }
  .empty { color: var(--dim); text-align: center; padding: 40px 10px; line-height: 1.6; }
  .empty span { color: var(--mute); font-size: 12px; }
  @media (max-width: 1100px) { .lib { grid-template-columns: 1fr; } }
</style>
