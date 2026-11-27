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
  import CardGrid from "../ui/CardGrid.svelte";

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
  // (rc3) under six strips in sight: they scroll sideways, and the view says how many are out of sight
  let strips: HTMLDivElement | undefined = $state();
  let hidden = $state(0);
  function measure() {
    if (!strips) return;
    const r = strips.getBoundingClientRect();
    hidden = [...strips.children].filter((c) => { const b = c.getBoundingClientRect(); return b.right > r.right + 2 || b.left < r.left - 2; }).length;
  }
  $effect(() => { if (!strips) return; measure(); const o = new ResizeObserver(measure); o.observe(strips); return () => o.disconnect(); });
  const levelText = (v: number) => { const d = device.tdesc[ID.P_LEVEL]; return d ? fmtValue(d, v).join(" ") : String(v); };
</script>

<div class="mixer">
  <!-- (rc3) six strips that breathe: every knob compact, on one grid; under six in sight, they scroll sideways and say so -->
  <div class="strips scroll" data-scroll-x role="group" aria-label={t("view.mixer")} bind:this={strips} onscroll={measure}>
    {#each device.tracks as tr, k}
      {@const c = TRACK_COLORS[k]}
      <section class="strip" class:sel={k === device.sel} style="--c:{c}" aria-label="T{k + 1}">
        <button class="hd" onclick={() => device.selectTrack(k)} aria-label={t("tracks.select", { n: k + 1 })}>
          <span class="n dot">{k + 1}</span><span class="nm dot">{tr.name}</span><span class="en">{device.engineName(tr.engine)}</span>
        </button>
        <div class="sec">{t("mixer.eq")}</div>
        <div class="g2">
          {#each EQ as id, b}
            {@const kk = killed[`${k}:${b}`]}
            <div class="band">
              <Param track={k} {id} color={c} size={34} label={BAND[b]} where={`GLO ×2 › EQ › KNOB ${b + 1}`} />
              <button class="kill" class:on={!!kk} class:locked={kk?.locked} title={t("mixer.killTip")} aria-pressed={!!kk} aria-label={`${t("mixer.kill")} ${BAND[b]} T${k + 1}`}
                      onpointerdown={(e) => killOn(k, b, e.shiftKey)} onpointerup={() => killOff(k, b)} onpointerleave={() => killOff(k, b)}
                      onkeydown={(e) => { if (e.key === " " || e.key === "Enter") { e.preventDefault(); killOn(k, b, true); } }}>{t("mixer.kill")}</button>
            </div>
          {/each}
          <div class="band"><Param track={k} id={ID.P_DEPTH} color={c} size={34} where="GLO ×2 › EQ › KNOB 4" /></div>
        </div>
        <div class="sec">{t("mixer.sends")}</div>
        <div class="g2">
          <Param track={k} id={ID.P_DIST} color={c} size={34} where="FX › FX › KNOB 1" />
          <Param track={k} id={ID.P_CHOR} color={c} size={34} where="FX › FX › KNOB 2" />
          <Param track={k} id={ID.P_DLY} color={c} size={34} where="FX › FX › KNOB 3" />
          <Param track={k} id={ID.P_REV} color={c} size={34} where="FX › FX › KNOB 4" />
          <Param track={k} id={ID.P_DKSRC} color={c} size={34} where="FX › DUCK › KNOB 1" label="DUCK BY" />
          <Param track={k} id={ID.P_DKAMT} color={c} size={34} where="FX › DUCK › KNOB 2" label="DUCK" />
        </div>
        <div class="bottom">
          <Fader value={tr.level} def={T.TP[ID.P_LEVEL].def} label="LEVEL T{k + 1}" text={levelText(tr.level)} color={c} height={128}
                 onchange={(v) => device.setLevel(k, v)} />
          <div class="side">
            <Param track={k} id={ID.P_PAN} color={c} size={34} where="GLO › MIXER › KNOB 2" />
            <button class="m" class:on={!!tr.mute} aria-pressed={!!tr.mute} onclick={() => device.setMute(k, !tr.mute)}>{t("tracks.mute")}</button>
            <button class="s" class:on={device.soloTrack === k} aria-pressed={device.soloTrack === k} onclick={() => device.solo(k)}>{t("tracks.solo")}</button>
          </div>
        </div>
      </section>
    {/each}
  </div>
  {#if hidden > 0}<p class="more">{t("mixer.scrollMore", { n: hidden })}</p>{/if}

  <h2 class="mh">{t("mixer.master")}</h2>
  <CardGrid items={glo}>
    {#snippet card(pg, wide)}<PageCard page={pg} k={device.sel} color="var(--fg-2)" {wide} />{/snippet}
  </CardGrid>
</div>

<style>
  .strips { display: grid; grid-auto-flow: column; grid-auto-columns: minmax(150px, 1fr); gap: 8px; overflow-x: auto; overflow-y: hidden; padding-bottom: 6px; scrollbar-width: auto; }
  .strip { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 10px 6px 12px; display: flex; flex-direction: column; gap: 8px; align-items: stretch; border-top: 3px solid var(--c); min-width: 0; }
  .strip.sel { background: var(--surf-2); border-color: color-mix(in srgb, var(--c) 40%, var(--line)); border-top-color: var(--c); }
  .hd { width: 100%; display: grid; grid-template-columns: 26px minmax(0, 1fr); grid-template-rows: auto auto; column-gap: 8px; text-align: left; background: none; border: 0; padding: 0 4px; }
  .n { grid-row: span 2; width: 26px; height: 26px; display: grid; place-items: center; background: var(--c); color: #0a0a0a; border-radius: 7px; font-size: 15px; }
  .nm { font-size: 13px; line-height: 1.3; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .en { font-size: 10.5px; color: var(--dim); font-weight: 600; }
  .sec { font-size: 10.5px; font-weight: 700; color: var(--dim); text-align: center; padding-top: 8px; border-top: 1px solid var(--line); }
  .g2 { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); justify-items: center; align-items: start; row-gap: 10px; }
  .band { display: flex; flex-direction: column; align-items: center; gap: 8px; }
  .kill { height: 20px; padding: 0 10px; border-radius: 5px; border: 1px solid var(--line-2); background: var(--surf-3); color: var(--dim); font-size: 10px; font-weight: 800; letter-spacing: 0.04em; }
  .kill:hover { color: var(--fg); }
  .kill.on { background: var(--err); border-color: var(--err); color: #fff; }
  .kill.locked { box-shadow: 0 0 0 2px color-mix(in srgb, var(--err) 40%, transparent); }
  .bottom { display: flex; align-items: flex-start; justify-content: center; gap: 4px; padding-top: 10px; border-top: 1px solid var(--line); width: 100%; }
  .side { display: flex; flex-direction: column; align-items: center; gap: 8px; }
  .side button { width: 60px; height: 26px; border-radius: 6px; border: 1px solid var(--line-2); background: var(--surf-3); color: var(--dim); font-size: 11px; font-weight: 700; }
  .side button:hover { color: var(--fg); }
  .m.on { background: var(--warn); border-color: var(--warn); color: #1a1200; }
  .s.on { background: var(--info); border-color: var(--info); color: #001a17; }
  .more { margin: 6px 0 0; font-size: 12px; color: var(--dim); }
  .strips { mask-image: linear-gradient(90deg, #000 calc(100% - 28px), transparent); }
  .strips:not(:has(+ .more)) { mask-image: none; }
  .mh { font-size: 13px; color: var(--fg-2); margin: 24px 0 10px; }
</style>
