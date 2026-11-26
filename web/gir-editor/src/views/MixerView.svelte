<script lang="ts">
  // Six channel strips as the FM-1's GLO › MIXER and GLO › EQ: level, pan, mute / solo, the isolator (LOW MID HIGH, each
  // with a KILL that cuts while held and locks with Shift), DEPTH, the sends, DUCK; then the global buses
  import { device } from "../lib/model/device.svelte";
  import { ID, T } from "../lib/model/tables";
  import { TRACK_COLORS } from "../lib/ui.svelte";
  import { t } from "../lib/i18n/index.svelte";
  import { fmtValue } from "../lib/legacy/proto.js";
  import Param from "../ui/Param.svelte";
  import Fader from "../ui/components/Fader.svelte";
  import PageCard from "./sound/PageCard.svelte";

  const EQ = [ID.P_EQ_LOW, ID.P_EQ_MID, ID.P_EQ_HIGH];
  const BAND = ["LOW", "MID", "HIGH"];
  // a kill: the band's value before it, while held or locked (key "k:band")
  let killed = $state<Record<string, { before: number; locked: boolean }>>({});
  async function killOn(k: number, b: number, lock: boolean) {
    const key = `${k}:${b}`, cur = killed[key];
    if (cur && lock) { await killOff(k, b, true); return; }
    if (cur) return;
    killed[key] = { before: device.tracks[k].p[EQ[b]], locked: lock };
    await device.setParam(k, EQ[b], 0, { undo: false });
  }
  async function killOff(k: number, b: number, force = false) {
    const key = `${k}:${b}`, cur = killed[key];
    if (!cur || (cur.locked && !force)) return;
    delete killed[key];
    await device.setParam(k, EQ[b], cur.before, { undo: false });
  }
  const glo = T.PAGES.filter((p) => p.fam === "GLO" && p.title !== "EQ");
  const levelText = (v: number) => { const d = device.tdesc[ID.P_LEVEL]; return d ? fmtValue(d, v).join(" ") : String(v); };
</script>

<div class="mixer">
  <div class="strips">
    {#each device.tracks as tr, k}
      {@const c = TRACK_COLORS[k]}
      <section class="strip" class:sel={k === device.sel} style="--c:{c}" aria-label="T{k + 1}">
        <button class="hd" onclick={() => device.selectTrack(k)}>
          <span class="n dot">{k + 1}</span><span class="nm dot">{tr.name}</span><span class="en">{device.engineName(tr.engine)}</span>
        </button>
        <div class="eq">
          <span class="sec">{t("mixer.eq")}</span>
          {#each EQ as id, b}
            {@const kk = killed[`${k}:${b}`]}
            <div class="band">
              <Param track={k} {id} color={c} size={40} label={BAND[b]} where={`GLO ×2 › EQ › KNOB ${b + 1}`} />
              <button class="kill" class:on={!!kk} class:locked={kk?.locked} title={t("mixer.killTip")} aria-pressed={!!kk}
                      onpointerdown={(e) => killOn(k, b, e.shiftKey)} onpointerup={() => killOff(k, b)} onpointerleave={() => killOff(k, b)}
                      onkeydown={(e) => { if (e.key === " " || e.key === "Enter") { e.preventDefault(); killOn(k, b, true); } }}>{t("mixer.kill")}</button>
            </div>
          {/each}
          <div class="band"><Param track={k} id={ID.P_DEPTH} color={c} size={40} where="GLO ×2 › EQ › KNOB 4" /></div>
        </div>
        <div class="sends">
          <span class="sec">{t("mixer.sends")}</span>
          <div class="g4">
            <Param track={k} id={ID.P_DIST} color={c} size={30} where="FX › FX › KNOB 1" />
            <Param track={k} id={ID.P_CHOR} color={c} size={30} where="FX › FX › KNOB 2" />
            <Param track={k} id={ID.P_DLY} color={c} size={30} where="FX › FX › KNOB 3" />
            <Param track={k} id={ID.P_REV} color={c} size={30} where="FX › FX › KNOB 4" />
          </div>
          <div class="g2">
            <Param track={k} id={ID.P_DKSRC} color={c} size={30} where="FX › DUCK › KNOB 1" label="DUCK BY" />
            <Param track={k} id={ID.P_DKAMT} color={c} size={30} where="FX › DUCK › KNOB 2" label="DUCK" />
          </div>
        </div>
        <div class="bottom">
          <Fader value={tr.level} def={T.TP[ID.P_LEVEL].def} label="LEVEL T{k + 1}" text={levelText(tr.level)} color={c} height={128}
                 onchange={(v) => device.setLevel(k, v)} />
          <div class="side">
            <Param track={k} id={ID.P_PAN} color={c} size={36} where="GLO › MIXER › KNOB 2" />
            <button class="m" class:on={!!tr.mute} aria-pressed={!!tr.mute} onclick={() => device.setMute(k, !tr.mute)}>{t("tracks.mute")}</button>
            <button class="s" class:on={device.soloTrack === k} aria-pressed={device.soloTrack === k} onclick={() => device.solo(k)}>{t("tracks.solo")}</button>
          </div>
        </div>
      </section>
    {/each}
  </div>

  <h2 class="mh">{t("mixer.master")}</h2>
  <div class="glo">
    {#each glo as pg}<PageCard page={pg} k={device.sel} color="var(--fg-2)" />{/each}
  </div>
</div>

<style>
  .strips { display: grid; grid-template-columns: repeat(6, minmax(150px, 1fr)); gap: 8px; }
  .strip { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 10px 6px 12px; display: flex; flex-direction: column; gap: 8px; align-items: center; border-top: 3px solid var(--c); }
  .strip.sel { background: var(--surf-2); border-color: color-mix(in srgb, var(--c) 40%, var(--line)); border-top-color: var(--c); }
  .hd { width: 100%; display: grid; grid-template-columns: 26px 1fr; grid-template-rows: auto auto; column-gap: 8px; text-align: left; background: none; border: 0; padding: 0 4px; }
  .n { grid-row: span 2; width: 26px; height: 26px; display: grid; place-items: center; background: var(--c); color: #0a0a0a; border-radius: 7px; font-size: 15px; }
  .nm { font-size: 13px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .en { font-size: 10.5px; color: var(--dim); font-weight: 600; }
  .sec { width: 100%; font-size: 10.5px; font-weight: 700; color: var(--dim); text-align: center; }
  .eq { width: 100%; display: grid; grid-template-columns: 1fr 1fr; justify-items: center; row-gap: 2px; padding-top: 8px; border-top: 1px solid var(--line); }
  .eq .sec { grid-column: span 2; }
  .eq :global(.knob) { width: 66px; }
  .sends { width: 100%; display: flex; flex-direction: column; align-items: center; gap: 4px; padding-top: 8px; border-top: 1px solid var(--line); }
  .band { display: flex; flex-direction: column; align-items: center; gap: 2px; }
  .kill { height: 20px; padding: 0 10px; border-radius: 5px; border: 1px solid var(--line-2); background: var(--surf-3); color: var(--dim); font-size: 10px; font-weight: 800; letter-spacing: 0.04em; margin-bottom: 4px; }
  .kill:hover { color: var(--fg); }
  .kill.on { background: var(--err); border-color: var(--err); color: #fff; }
  .kill.locked { box-shadow: 0 0 0 2px color-mix(in srgb, var(--err) 40%, transparent); }
  .g4 { display: grid; grid-template-columns: 1fr 1fr; }
  .g2 { display: grid; grid-template-columns: 1fr 1fr; }
  .g4 :global(.knob), .g2 :global(.knob) { width: 66px; }
  .bottom { display: flex; align-items: flex-start; justify-content: center; gap: 4px; padding-top: 8px; border-top: 1px solid var(--line); width: 100%; }
  .side { display: flex; flex-direction: column; align-items: center; gap: 6px; }
  .side button { width: 62px; flex: 1; height: 26px; border-radius: 6px; border: 1px solid var(--line-2); background: var(--surf-3); color: var(--dim); font-size: 11px; font-weight: 700; }
  .side button:hover { color: var(--fg); }
  .m.on { background: var(--warn); border-color: var(--warn); color: #1a1200; }
  .s.on { background: var(--info); border-color: var(--info); color: #001a17; }
  .mh { font-size: 13px; color: var(--fg-2); margin: 24px 0 10px; }
  .glo { display: grid; grid-template-columns: repeat(auto-fill, minmax(286px, 1fr)); gap: 10px; }
  @media (max-width: 1100px) { .strips { grid-template-columns: repeat(3, 1fr); } }
</style>
