<script lang="ts">
  // (rc2) The Akai APC mini mk2, as the original's Song tab drives it, drawn here as its lights are: what each pad
  // does is written on it, and it can be clicked (a pad pressed). A clear word when none is plugged in.
  import { onMount } from "svelte";
  import { apc, apcStart, press, APC } from "../../lib/apc.svelte";
  import { APC_PALETTE } from "../../lib/legacy/proto.js";
  import { t } from "../../lib/i18n/index.svelte";
  import Button from "../../ui/components/Button.svelte";

  onMount(() => { if (localStorage.getItem("gir.apc") === "1") apcStart(); });
  function connect() { localStorage.setItem("gir.apc", "1"); apcStart(); }
  const ENT = "ABCDEFGHIJKLMNOP";
  const led = $derived(new Map(apc.leds.map(([n, st, v]) => [n, [st, v] as [number, number]])));
  const col = (v: number) => "#" + APC_PALETTE.slice(v * 6, v * 6 + 6);
  function look(n: number) {
    const x = led.get(n);
    if (!x || !x[1]) return "background: var(--surf-3)";
    const [st, v] = x, a = st === APC.FULL || st === APC.BLINK ? 1 : st === APC.KEPT ? 0.55 : st === APC.REST ? 0.3 : 1;
    return `background: color-mix(in srgb, ${col(v)} ${Math.round(a * 100)}%, var(--surf-3))`;
  }
  const blink = (n: number) => led.get(n)?.[0] === APC.BLINK;
  const btnOn = (n: number) => (led.get(n)?.[1] ?? 0) > 0;
  const label = (c: number, row: number) => c < 6 ? `T${c + 1} ${ENT[row + 8 * apc.page]}` : `S${row + 8 * (c - 6) + 1}`;
</script>

<section class="apc" data-testid="apc">
  <header>
    <h3>{t("apc.title")}</h3>
    {#if apc.status === "on"}<span class="ok"><i></i>{apc.name}</span>{/if}
    {#if apc.status !== "on"}<Button size="s" onclick={connect}>{t("apc.connect")}</Button>{/if}
  </header>
  {#if apc.status === "off"}<p class="msg">{t("apc.what")}</p>
  {:else if apc.status === "nomidi"}<p class="msg warn">{t("apc.nomidi")}</p>
  {:else if apc.status === "denied"}<p class="msg warn">{t("apc.denied")}</p>
  {:else if apc.status === "absent"}<p class="msg warn" role="status">{t("apc.absent")}</p>
  {:else}
    <div class="dev">
      <div class="grid" role="grid" aria-label="APC mini mk2">
        {#each Array.from({ length: 8 }, (_, r) => r) as row}
          {#each Array.from({ length: 8 }, (_, c) => c) as c}
            {@const n = (7 - row) * 8 + c}
            <button class="pad" class:blink={blink(n)} class:scene={c >= 6} style={look(n)} aria-label={label(c, row)} title={label(c, row)}
                    onclick={() => press([0x90, n, 127])}><span>{label(c, row)}</span></button>
          {/each}
        {/each}
        {#each Array.from({ length: 6 }, (_, k) => k) as k}
          <button class="rb" class:on={btnOn(APC.MUTE + k)} onclick={() => press([0x90, APC.MUTE + k, 127])}>M{k + 1}</button>
        {/each}
        {#each [0, 1] as pg}
          <button class="rb" class:on={btnOn(APC.PAGE + pg)} onclick={() => press([0x90, APC.PAGE + pg, 127])}>{pg ? "I–P" : "A–H"}</button>
        {/each}
      </div>
      <div class="side">
        <button class="rb play" class:on={btnOn(APC.PLAY)} onclick={() => press([0x90, APC.PLAY, 127])}>▶ PLAY</button>
        <ul class="legend">
          <li><i style="background:{col(APC.GREEN)}"></i>{t("apc.lPlaying")}</li>
          <li><i class="blink" style="background:{col(APC.GREEN)}"></i>{t("apc.lNext")}</li>
          <li><i style="background:var(--t1)"></i>{t("apc.lEntry")}</li>
          <li><i style="background:#fff"></i>{t("apc.lScene")}</li>
        </ul>
        <p class="map">{t("apc.map")}</p>
        {#if apc.last}<p class="last">{t("apc.last")} <b class="dot">{apc.last}</b></p>{/if}
      </div>
    </div>
  {/if}
</section>

<style>
  .apc { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 14px; display: flex; flex-direction: column; gap: 10px; }
  header { display: flex; align-items: center; gap: 12px; }
  h3 { margin: 0; font-size: 13px; color: var(--fg-2); flex: 1; }
  .ok { display: flex; align-items: center; gap: 6px; font-size: 12.5px; color: var(--fg-2); }
  .ok i { width: 8px; height: 8px; border-radius: 50%; background: var(--ok); }
  .msg { margin: 0; color: var(--dim); font-size: 13px; line-height: 1.5; max-width: 80ch; }
  .msg.warn { color: var(--fg-2); }
  .dev { display: flex; gap: 18px; align-items: flex-start; flex-wrap: wrap; }
  .grid { display: grid; grid-template-columns: repeat(8, 46px); gap: 5px; padding: 10px; background: #111113; border-radius: 10px; border: 1px solid var(--line-2); }
  .pad { height: 34px; border: 0; border-radius: 5px; font-size: 9.5px; color: rgb(255 255 255 / 70%); padding: 0; text-shadow: 0 1px 2px #000; }
  .pad span { pointer-events: none; }
  .pad.scene { border-radius: 9px; }
  .pad:focus-visible, .rb:focus-visible { outline: 2px solid var(--fg); outline-offset: 1px; }
  .blink { animation: bk 0.5s steps(2) infinite; }
  @keyframes bk { 50% { opacity: 0.35; } }
  .rb { height: 22px; border: 0; border-radius: 4px; background: var(--surf-4); color: var(--dim); font-size: 10px; }
  .rb.on { background: #c33; color: #fff; }
  .side { display: flex; flex-direction: column; gap: 10px; min-width: 220px; flex: 1; }
  .play { width: 90px; height: 28px; font-size: 11px; }
  .play.on { background: var(--ok); color: #111; }
  .legend { list-style: none; margin: 0; padding: 0; display: flex; flex-direction: column; gap: 4px; font-size: 12px; color: var(--fg-2); }
  .legend i { display: inline-block; width: 12px; height: 12px; border-radius: 3px; margin-right: 8px; vertical-align: -2px; }
  .map, .last { margin: 0; font-size: 12px; color: var(--dim); line-height: 1.5; max-width: 50ch; }
  .last b { color: var(--fg); font-weight: 400; }
  @media (prefers-reduced-motion: reduce) { .blink { animation: none; } }
</style>
