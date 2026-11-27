<script lang="ts">
  // The six tracks, always visible: colour, number, engine, sound, level and activity, mute / solo / record.
  // Clicking one selects it here and on the FM-1.
  import { device } from "../lib/model/device.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import { TRACK_COLORS } from "../lib/ui.svelte";
  import { GIR_ENGINES } from "../lib/model/tables";
  import { isNarrow } from "../lib/layout.svelte";
  // (rc3) under 1180 px: compact, the number and colour, a short name, the three buttons small
  const compact = $derived(isNarrow());

  let now = $state(Date.now());
  $effect(() => { const i = setInterval(() => (now = Date.now()), 150); return () => clearInterval(i); });
</script>

<aside class="col scroll" class:compact aria-label={t("tracks.title")}>
  {#each device.tracks as tr, k}
    {@const sel = k === device.sel}
    {@const eng = device.engineName(tr.engine)}
    {@const active = now - (device.activity[k] ?? 0) < 400}
    <div class="trk" class:sel class:muted={!!tr.mute} style="--c:{TRACK_COLORS[k]}">
      <button class="main" onclick={() => device.selectTrack(k)} aria-pressed={sel} aria-label={t("tracks.select", { n: k + 1 })}>
        <span class="num dot">{k + 1}</span>
        <span class="txt">
          <span class="name dot">{tr.name || "—"}</span>
          <span class="eng">{eng}{#if GIR_ENGINES.has(eng)}<em>Gir</em>{/if}</span>
        </span>
        <span class="act" class:on={active} aria-hidden="true"></span>
      </button>
      <div class="lvl" aria-hidden="true"><span style="width:{(tr.level / 127) * 100}%"></span></div>
      <div class="btns">
        <button class="m" class:on={!!tr.mute} aria-pressed={!!tr.mute} title={t("tracks.mute")} aria-label={`${t("tracks.mute")} T${k + 1}`} onclick={() => device.setMute(k, !tr.mute)}>M</button>
        <button class="s" class:on={device.soloTrack === k} aria-pressed={device.soloTrack === k} title={t("tracks.solo")} aria-label={`${t("tracks.solo")} T${k + 1}`} onclick={() => device.solo(k)}>S</button>
        <button class="r" class:on={!!(device.recMask & (1 << k))} title={t("tracks.arm")} aria-label={`${t("tracks.arm")} T${k + 1}`}
                onclick={async () => { await device.selectTrack(k); await device.rec(); }}>●</button>
      </div>
    </div>
  {/each}
</aside>

<style>
  .col { width: var(--tracks-w); flex: none; padding: 2px 10px 12px 12px; display: flex; flex-direction: column; gap: 6px; overflow-y: auto; }
  .trk { position: relative; border-radius: var(--r-l); background: transparent; border: 1px solid transparent; padding: 8px 8px 8px 8px; transition: background var(--t-fast), border-color var(--t-fast); }
  .trk:hover { background: var(--surf); }
  .trk.sel { background: var(--surf-2); border-color: color-mix(in srgb, var(--c) 45%, var(--line)); box-shadow: inset 3px 0 0 var(--c); }
  .main { width: 100%; display: grid; grid-template-columns: 30px 1fr 8px; gap: 10px; align-items: center; background: none; border: 0; padding: 0; text-align: left; }
  .num { width: 30px; height: 30px; display: grid; place-items: center; border-radius: 8px; background: var(--c); color: #0a0a0a; font-size: 17px; }
  .muted .num { background: var(--surf-4); color: var(--dim); }
  .txt { min-width: 0; display: flex; flex-direction: column; }
  .name { font-size: 14px; color: var(--fg); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .eng { font-size: 11px; font-weight: 600; color: var(--dim); display: flex; gap: 6px; align-items: center; }
  .eng em { font-style: normal; font-size: 9.5px; color: var(--c); border: 1px solid color-mix(in srgb, var(--c) 50%, transparent); border-radius: 4px; padding: 0 4px; line-height: 14px; }
  .act { width: 8px; height: 8px; border-radius: 50%; background: var(--surf-4); transition: background 220ms; }
  .act.on { background: var(--c); box-shadow: 0 0 8px var(--c); transition: none; }
  .lvl { height: 3px; margin: 8px 0 6px 40px; background: var(--surf-3); border-radius: 2px; overflow: hidden; }
  .lvl span { display: block; height: 100%; background: var(--c); opacity: 0.7; }
  .btns { display: flex; gap: 4px; margin-left: 40px; }
  .btns button { flex: 1; height: 22px; border-radius: 5px; border: 1px solid var(--line-2); background: var(--surf-2); color: var(--dim); font-size: 10.5px; font-weight: 800; }
  .btns button:hover { color: var(--fg); }
  .m.on { background: var(--warn); border-color: var(--warn); color: #1a1200; }
  .s.on { background: var(--info); border-color: var(--info); color: #001a17; }
  .r.on { background: var(--err); border-color: var(--err); color: #fff; }
  .col.compact { width: 88px; padding: 2px 6px 12px 8px; }
  .compact .trk { padding: 6px 5px; }
  .compact .main { grid-template-columns: 1fr; gap: 4px; justify-items: center; text-align: center; position: relative; }
  .compact .num { width: 28px; height: 28px; font-size: 16px; }
  .compact .txt { width: 100%; align-items: center; }
  .compact .name { font-size: 11px; max-width: 100%; }
  .compact .eng { display: none; }
  .compact .act { position: absolute; top: 0; right: 2px; }
  .compact .lvl { margin: 6px 2px 5px; }
  .compact .btns { margin-left: 0; gap: 3px; }
  .compact .btns button { height: 20px; font-size: 10px; padding: 0; min-width: 0; }
</style>
