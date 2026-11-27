<script lang="ts">
  // The song as the FM-1 keeps it: each track's list of 16 entries (A..P), each a pattern (or a REST) repeated REPS
  // times, entries on CHAIN played as one; SONG OFF / LOOP / ONCE; the 16 scenes; the pool of 64 patterns. Click an
  // entry: launch it (at the LAUNCH point while playing). Right-click: everything else. Drag a pattern from the pool
  // onto an entry to put it there. The device refuses clears while playing; the editor says so.
  import { onDestroy, onMount } from "svelte";
  import { device } from "../lib/model/device.svelte";
  import { TRACK_COLORS, toast, ask } from "../lib/ui.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import { PAT_REST } from "../lib/legacy/proto.js";
  import Seg from "../ui/components/Seg.svelte";
  import Button from "../ui/components/Button.svelte";
  import ApcPanel from "./song/ApcPanel.svelte";

  onMount(() => device.setSongWatch(true));
  onDestroy(() => device.setSongWatch(false));
  const L = $derived(device.lists);
  const P = $derived(device.pool);
  const pf = $derived(device.perform);
  const ENT = "ABCDEFGHIJKLMNOP";
  const two = $derived(device.song2);
  const sndOk = $derived(!!(device.info && device.info.song2 & 2));
  const pat2 = (n: number) => String(n).padStart(2, "0");

  async function run(p: Promise<number>) {
    try {
      const rc = await p;
      if (rc === 3) toast("warn", t("song.stopFirst"));
      else if (rc === 4) toast("warn", t("song.full"));
      else if (rc === 5) toast("err", t("toast.error", { detail: "failed" }));
      return rc;
    } catch (e) { toast("err", String((e as Error).message)); return -1; }
  }
  const op = (o: number, k: number, e: number, v: number | number[] | null = null) => run(device.listOp(o, k, e, v));

  // context menu
  let menu = $state<null | { k: number; e: number; x: number; y: number; sub?: "pat" | "reps" | "sound" }>(null);
  function ctx(k: number, e: number, ev: MouseEvent) { ev.preventDefault(); menu = { k, e, x: Math.min(ev.clientX, innerWidth - 240), y: Math.min(ev.clientY, innerHeight - 380) }; }
  function close() { menu = null; }
  const usedPats = $derived(P ? P.pats.map((p, i) => ({ i, ...p })).filter((p) => p.len > 0) : []);

  // drag from the pool
  let dragPat = $state<number | null>(null);
  let over = $state<string | null>(null);
  async function drop(k: number, e: number) { if (dragPat != null) await op(1, k, e, dragPat + 1); dragPat = null; over = null; }

  async function scene(s: number, store: boolean) {
    if (store) {
      if (pf && (pf.scenes >> s) & 1) {
        const ok = await ask({ title: t("song.store") + ` ${s + 1}`, body: `${t("song.store")} ${s + 1}?`, ok: t("song.store") });
        if (!ok) return;
      }
      await run(device.performOp(1, s));
    } else await run(device.performOp(2, s));
  }
  async function clearUnused() {
    const ok = await ask({ title: t("song.clearUnused"), body: t("song.clearUnused") + "?", ok: t("dlg.erase"), danger: true });
    if (ok) await run(device.poolOp(null, 3));
  }
  const chainStart = (k: number, e: number) => !!L && L.tracks[k].ent[e].chain && (e === 0 || !L.tracks[k].ent[e - 1].chain);
</script>

<svelte:window onclick={(e) => { if (menu && !(e.target as HTMLElement).closest(".menu")) close(); }} onkeydown={(e) => e.key === "Escape" && close()} />

<div class="song">
  <header class="bar">
    <div class="grp"><span class="lb">{t("song.mode")}</span>
      <Seg size="s" label={t("song.mode")} value={L?.song ?? 0} onchange={(v) => op(6, v, 0)}
           options={[{ value: 0, label: t("song.off") }, { value: 1, label: t("song.on") }, ...(two ? [{ value: 2, label: t("song.once") }] : [])]} />
    </div>
    <div class="grp"><span class="lb">{t("song.launch")}</span>
      <Seg size="s" label={t("song.launch")} value={pf?.launch ?? 0} onchange={(v) => run(device.performOp(5, v))}
           options={[0, 1, 2, 3].map((v) => ({ value: v, label: t("song.launch" + v) }))} />
    </div>
    <p class="help">{t("song.help")}</p>
  </header>

  {#if L}
    <div class="tl" role="grid" aria-label={t("song.title")}>
      <div class="hrow" role="row"><span></span>{#each ENT as c}<span class="eh dot">{c}</span>{/each}</div>
      {#each L.tracks as x, k}
        {@const c = TRACK_COLORS[k]}
        {@const mt = pf?.tracks[k]}
        <div class="row" role="row" style="--c:{c}">
          <button class="th" onclick={() => run(device.performOp(3, k))} title={t("tracks.mute")} class:muted={mt?.muted} class:staged={!!mt?.staged}>
            <span class="n dot">{k + 1}</span><span class="mute">{mt?.muted ? "M" : ""}</span>
          </button>
          {#each x.ent as en, e}
            {@const key = `${k}:${e}`}
            <button class="ent" role="gridcell" class:empty={!en.pat} class:rest={en.pat === PAT_REST} class:cur={x.cur === e} class:next={x.next === e}
                    class:chain={en.chain} class:cstart={chainStart(k, e)} class:snd={en.snd[0] > 0} class:over={over === key}
                    aria-label={`T${k + 1} ${ENT[e]}: ${en.pat === PAT_REST ? "REST" : en.pat ? "pattern " + en.pat : "empty"}`}
                    onclick={() => en.pat && op(3, k, e)} oncontextmenu={(ev) => ctx(k, e, ev)}
                    ondragover={(ev) => { ev.preventDefault(); over = key; }} ondragleave={() => (over = null)} ondrop={(ev) => { ev.preventDefault(); drop(k, e); }}>
              {#if en.pat === PAT_REST}<span class="p">REST</span>
              {:else if en.pat}<span class="p dot">{pat2(en.pat)}</span>{/if}
              {#if en.pat && en.reps > 1}<span class="r">×{en.reps}</span>{/if}
            </button>
          {/each}
        </div>
      {/each}
    </div>

    <section class="scenes">
      <h3>{t("song.scenes")}</h3>
      <div class="sc">
        {#each Array(16) as _, s}
          {@const kept = pf ? (pf.scenes >> s) & 1 : 0}
          <button class="scn" class:kept={!!kept} class:last={pf?.last === s} title={`${t("song.recall")} (Shift: ${t("song.store")})`}
                  onclick={(e) => scene(s, e.shiftKey || !kept)}>{s + 1}</button>
        {/each}
        <span class="hint">{t("song.recall")} · Shift-click: {t("song.store")}</span>
      </div>
    </section>

    <section class="pool">
      <header><h3>{t("song.pool")}</h3><span class="cnt">{usedPats.length} / 64</span>
        <Button size="s" variant="ghost" onclick={clearUnused}>{t("song.clearUnused")}</Button></header>
      <div class="pg">
        {#each P?.pats ?? [] as p, i}
          <div class="pt" class:empty={!p.len} class:unused={p.len > 0 && !p.refs} draggable={p.len > 0} role="listitem"
               ondragstart={(e) => { dragPat = i; e.dataTransfer?.setData("text/plain", String(i)); }} ondragend={() => { dragPat = null; over = null; }}
               title={p.len ? `${pat2(i + 1)} · ${p.len} steps · ${p.refs ? t("song.used", { n: p.refs }) : t("song.unused")}` : t("song.empty")}>
            <span class="pn dot">{pat2(i + 1)}</span>
            {#if p.len}<span class="pl">{p.len}</span>{/if}
            {#if p.len && !p.refs}
              <button class="del" aria-label={t("song.clearPattern")} onclick={async () => { if (await ask({ title: t("song.clearPattern"), body: `${t("song.clearPattern")} ${pat2(i + 1)}?`, ok: t("dlg.erase"), danger: true })) run(device.poolOp(i, 1)); }}>×</button>
            {/if}
          </div>
        {/each}
      </div>
    </section>
    <ApcPanel />
  {:else}
    <p class="loading">…</p>
  {/if}
</div>

{#if menu && L}
  {@const en = L.tracks[menu.k].ent[menu.e]}
  <div class="menu" role="menu" style="left:{menu.x}px; top:{menu.y}px; --c:{TRACK_COLORS[menu.k]}">
    <div class="mh dot">T{menu.k + 1} · {ENT[menu.e]}</div>
    {#if !menu.sub}
      {#if en.pat}<button role="menuitem" onclick={() => { op(3, menu!.k, menu!.e); close(); }}>{t("song.play")}</button>{/if}
      <button role="menuitem" onclick={() => (menu!.sub = "pat")}>{t("song.setPattern")} ›</button>
      {#if two}<button role="menuitem" onclick={() => { op(7, menu!.k, menu!.e); close(); }}>{t("song.new")}</button>{/if}
      <button role="menuitem" onclick={() => { op(1, menu!.k, menu!.e, PAT_REST); close(); }}>{t("song.rest")}</button>
      {#if en.pat && en.pat !== PAT_REST}
        <button role="menuitem" onclick={() => { op(2, menu!.k, menu!.e); close(); }}>{t("song.copy")}</button>
        {#if two}<button role="menuitem" onclick={() => { op(8, menu!.k, menu!.e); close(); }}>{t("song.copyPlus")}</button>{/if}
      {/if}
      {#if en.pat}<button role="menuitem" onclick={() => (menu!.sub = "reps")}>{t("song.reps")} (×{en.reps}) ›</button>
        {#if two}<button role="menuitem" onclick={() => { op(5, menu!.k, menu!.e, en.chain ? 1 : 0); close(); }}>{en.chain ? "✓ " : ""}{t("song.chain")}</button>{/if}
        {#if sndOk}<button role="menuitem" onclick={() => (menu!.sub = "sound")}>{t("song.sound")} ›</button>{/if}
        <button role="menuitem" class="danger" onclick={() => { op(4, menu!.k, menu!.e); close(); }}>{t("song.clear")}</button>
      {/if}
    {:else if menu.sub === "pat"}
      <div class="pgrid">
        {#each usedPats as p}<button onclick={() => { op(1, menu!.k, menu!.e, p.i + 1); close(); }} class="dot">{pat2(p.i + 1)}</button>{/each}
      </div>
    {:else if menu.sub === "reps"}
      <div class="pgrid">{#each Array(16) as _, r}<button class:on={en.reps === r + 1} onclick={() => { op(5, menu!.k, menu!.e, r + 1); close(); }}>×{r + 1}</button>{/each}</div>
    {:else if menu.sub === "sound"}
      {@const e0 = device.tracks[menu.k]?.engine ?? 0}
      <div class="slist scroll">
        <button onclick={() => { op(9, menu!.k, menu!.e, [0, 0]); close(); }}>{t("song.noSound")}</button>
        {#each device.names[e0] ?? [] as nm, i}<button class="dot" onclick={() => { op(9, menu!.k, menu!.e, [e0 + 1, i]); close(); }}>{nm}</button>{/each}
      </div>
    {/if}
  </div>
{/if}

<style>
  .song { display: flex; flex-direction: column; gap: 18px; }
  .bar { display: flex; align-items: center; gap: 22px; flex-wrap: wrap; }
  .grp { display: flex; align-items: center; gap: 8px; }
  .lb { font-size: 12px; font-weight: 700; color: var(--dim); }
  .help { margin: 0 0 0 auto; font-size: 12px; color: var(--dim); max-width: 46ch; }
  .tl { display: flex; flex-direction: column; gap: 6px; background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 10px; overflow-x: auto; }
  .hrow, .row { display: grid; grid-template-columns: 48px repeat(16, minmax(48px, 1fr)); gap: 4px; }
  .eh { text-align: center; color: var(--dim); font-size: 13px; }
  .th { display: flex; align-items: center; gap: 4px; background: none; border: 0; padding: 0; }
  .th .n { width: 30px; height: 40px; display: grid; place-items: center; border-radius: 8px; background: var(--c); color: #0a0a0a; font-size: 16px; }
  .th.muted .n { background: var(--surf-4); color: var(--dim); }
  .th.staged .n { animation: blink 0.6s steps(2) infinite; }
  .mute { font-size: 10px; font-weight: 800; color: var(--warn); }
  .ent { position: relative; height: 40px; border-radius: 7px; border: 1px solid color-mix(in srgb, var(--c) 30%, var(--line-2)); background: color-mix(in srgb, var(--c) 16%, var(--surf-2));
         display: flex; align-items: center; justify-content: center; gap: 3px; padding: 0; transition: background var(--t-fast), border-color var(--t-fast); }
  .ent:hover { border-color: var(--c); }
  .ent.empty { background: var(--surf-2); border: 1px dashed var(--line-2); }
  .ent.rest { background: repeating-linear-gradient(135deg, var(--surf-2) 0 6px, var(--surf-3) 6px 12px); }
  .ent.cur { background: var(--c); border-color: var(--c); }
  .ent.cur .p, .ent.cur .r { color: #0a0a0a; }
  .ent.next { animation: blink 0.5s steps(2) infinite; border-color: var(--fg); }
  .ent.chain::after { content: ""; position: absolute; right: -6px; top: 50%; width: 8px; height: 3px; background: var(--c); z-index: 1; }
  .ent.snd::before { content: ""; position: absolute; top: 0; right: 0; border-style: solid; border-width: 0 9px 9px 0; border-color: transparent var(--bg-2) transparent transparent; }
  .ent.over { outline: 2px dashed var(--fg); }
  .p { font-size: 15px; color: var(--fg); }
  .ent .p:not(.dot) { font-size: 10px; font-weight: 800; color: var(--dim); }
  .r { font-size: 10px; font-weight: 700; color: var(--fg-2); }
  @keyframes blink { 50% { opacity: 0.45; } }
  h3 { margin: 0 0 8px; font-size: 13px; color: var(--fg-2); }
  .sc { display: flex; flex-wrap: wrap; gap: 5px; align-items: center; }
  .scn { width: 40px; height: 34px; border-radius: 7px; border: 1px dashed var(--line-2); background: var(--surf-2); color: var(--dim); font-weight: 700; }
  .scn.kept { border-style: solid; color: var(--fg); background: var(--surf-3); }
  .scn.last { background: var(--ok); color: #0a1400; border-color: var(--ok); }
  .hint { color: var(--mute); font-size: 12px; margin-left: 8px; }
  .pool header { display: flex; align-items: center; gap: 10px; margin-bottom: 8px; }
  .pool h3 { margin: 0; }
  .cnt { color: var(--dim); font-size: 12px; }
  .pg { display: grid; grid-template-columns: repeat(16, 1fr); gap: 4px; }
  .pt { position: relative; height: 42px; border-radius: 7px; background: var(--surf-3); border: 1px solid var(--line-2); padding: 4px 6px; cursor: grab; display: flex; flex-direction: column; }
  .pt.empty { background: transparent; border-style: dashed; cursor: default; opacity: 0.5; }
  .pt.unused { border-color: var(--warn); }
  .pn { font-size: 13px; }
  .pl { font-size: 10px; color: var(--dim); }
  .del { position: absolute; top: 2px; right: 2px; width: 18px; height: 18px; border-radius: 4px; border: 0; background: none; color: var(--dim); font-size: 14px; line-height: 1; }
  .del:hover { background: var(--err); color: #fff; }
  .menu { position: fixed; z-index: 60; width: 230px; background: var(--surf-2); border-radius: var(--r-l); box-shadow: var(--shadow-pop); padding: 6px; display: flex; flex-direction: column; }
  .mh { padding: 4px 8px 6px; color: var(--c); font-size: 14px; }
  .menu > button, .slist button { text-align: left; background: none; border: 0; border-radius: 6px; padding: 7px 10px; font-size: 13px; color: var(--fg-2); }
  .menu > button:hover, .slist button:hover { background: var(--surf-4); color: var(--fg); }
  .menu .danger { color: #ff9aa4; }
  .pgrid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 4px; max-height: 260px; overflow: auto; }
  .pgrid button { height: 30px; border-radius: 6px; border: 1px solid var(--line-2); background: var(--surf-3); font-size: 13px; }
  .pgrid button.on, .pgrid button:hover { border-color: var(--c); color: var(--c); }
  .slist { max-height: 300px; display: flex; flex-direction: column; }
  .loading { color: var(--dim); }
</style>
