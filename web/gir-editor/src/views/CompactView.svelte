<script lang="ts">
  // The compact window (desktop app: Window › Compact Window; the web: ?compact=1), to sit beside a DAW: transport,
  // tempo, and for each track its level, mute and solo, the isolator's kills (held, Shift locks) and DEPTH
  import { device } from "../lib/model/device.svelte";
  import { ID } from "../lib/model/tables";
  import { TRACK_COLORS } from "../lib/ui.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import { desktop } from "../lib/desktop";
  import { ic } from "../ui/icons";
  import Param from "../ui/Param.svelte";

  const EQ = [ID.P_EQ_LOW, ID.P_EQ_MID, ID.P_EQ_HIGH];
  let killed = $state<Record<string, { before: number; locked: boolean }>>({});
  async function on(k: number, b: number, lock: boolean) {
    const key = `${k}:${b}`;
    if (killed[key] && lock) return off(k, b, true);
    if (killed[key]) return;
    killed[key] = { before: device.tracks[k].p[EQ[b]], locked: lock };
    await device.setParam(k, EQ[b], 0, { undo: false });
  }
  async function off(k: number, b: number, force = false) {
    const key = `${k}:${b}`, c = killed[key];
    if (!c || (c.locked && !force)) return;
    delete killed[key];
    await device.setParam(k, EQ[b], c.before, { undo: false });
  }
  const bpm = $derived(device.g[ID.G_BPM] ?? 120);
</script>

<div class="compact">
  <header>
    <span class="led" class:live={device.live}></span>
    <button class="tb" class:on={device.playing} aria-label={t("top.play")} onclick={() => device.togglePlay()}><i class="ic">{ic(device.playing ? "control_stop_f" : "control_play_f")}</i></button>
    <span class="bpm dot">{bpm}</span><span class="u">BPM</span>
    <span class="pn dot">{device.proj?.name ?? ""}</span>
    {#if desktop}<button class="x" onclick={() => desktop!.compact(false)} title="Full window">⤢</button>{/if}
  </header>
  <div class="strips">
    {#each device.tracks as tr, k}
      {@const c = TRACK_COLORS[k]}
      <div class="s" style="--c:{c}" class:sel={k === device.sel}>
        <button class="n dot" onclick={() => device.selectTrack(k)}>{k + 1}</button>
        <div class="lvl" role="slider" tabindex="0" aria-label="LEVEL T{k + 1}" aria-valuenow={tr.level} aria-valuemin={0} aria-valuemax={127}
             onwheel={(e) => { e.preventDefault(); device.setLevel(k, Math.max(0, Math.min(127, tr.level + (e.deltaY < 0 ? 2 : -2)))); }}
             onpointerdown={(e) => { const el = e.currentTarget as HTMLElement; el.setPointerCapture(e.pointerId); const set = (ev: PointerEvent) => { const r = el.getBoundingClientRect(); device.setLevel(k, Math.round(Math.max(0, Math.min(1, 1 - (ev.clientY - r.top) / r.height)) * 127)); }; set(e); el.onpointermove = (ev) => set(ev); el.onpointerup = () => (el.onpointermove = null); }}>
          <div style="height:{(tr.level / 127) * 100}%"></div>
        </div>
        <div class="kills">
          {#each ["L", "M", "H"] as nm, b}
            <button class:on={!!killed[`${k}:${b}`]} title={t("mixer.killTip")}
                    onpointerdown={(e) => on(k, b, e.shiftKey)} onpointerup={() => off(k, b)} onpointerleave={() => off(k, b)}>{nm}</button>
          {/each}
        </div>
        <Param track={k} id={ID.P_DEPTH} color={c} size={28} label="DEPTH" />
        <div class="ms">
          <button class:m={!!tr.mute} onclick={() => device.setMute(k, !tr.mute)}>M</button>
          <button class:so={device.soloTrack === k} onclick={() => device.solo(k)}>S</button>
        </div>
      </div>
    {/each}
  </div>
</div>

<style>
  .compact { height: 100%; display: flex; flex-direction: column; padding: 34px 10px 10px; gap: 8px; -webkit-app-region: no-drag; }
  :global(.desktop) .compact header { -webkit-app-region: drag; }
  header { display: flex; align-items: center; gap: 8px; padding: 0 4px; }
  header button { -webkit-app-region: no-drag; }
  .led { width: 8px; height: 8px; border-radius: 50%; background: var(--mute); }
  .led.live { background: var(--ok); }
  .tb { width: 30px; height: 28px; border-radius: 7px; border: 0; background: var(--surf-3); }
  .tb.on { background: color-mix(in srgb, var(--ok) 25%, var(--surf-3)); color: var(--ok); }
  .bpm { font-size: 20px; }
  .u { font-size: 10px; color: var(--dim); font-weight: 700; }
  .pn { flex: 1; text-align: right; color: var(--dim); font-size: 13px; }
  .x { border: 0; background: var(--surf-3); border-radius: 7px; width: 28px; height: 26px; }
  .strips { flex: 1; display: grid; grid-template-columns: repeat(6, 1fr); gap: 6px; min-height: 0; }
  .s { display: flex; flex-direction: column; align-items: center; gap: 6px; background: var(--surf); border-radius: 10px; padding: 6px 4px; border-top: 3px solid var(--c); min-height: 0; }
  .s.sel { background: var(--surf-2); }
  .n { width: 26px; height: 22px; border-radius: 6px; border: 0; background: var(--c); color: #0a0a0a; font-size: 14px; }
  .lvl { flex: 1; width: 22px; min-height: 60px; background: var(--bg); border-radius: 6px; position: relative; overflow: hidden; cursor: ns-resize; }
  .lvl div { position: absolute; bottom: 0; left: 0; right: 0; background: var(--c); opacity: 0.8; }
  .kills { display: flex; gap: 2px; }
  .kills button { width: 20px; height: 20px; border-radius: 4px; border: 1px solid var(--line-2); background: var(--surf-3); font-size: 10px; font-weight: 800; color: var(--dim); padding: 0; }
  .kills button.on { background: var(--err); border-color: var(--err); color: #fff; }
  .ms { display: flex; gap: 2px; }
  .ms button { width: 26px; height: 20px; border-radius: 4px; border: 1px solid var(--line-2); background: var(--surf-3); font-size: 10px; font-weight: 800; color: var(--dim); }
  .ms .m { background: var(--warn); color: #1a1200; }
  .ms .so { background: var(--info); color: #001a17; }
  .s :global(.knob .val) { font-size: 11px; }
</style>
