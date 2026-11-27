<script lang="ts">
  // No FM-1 yet: connect one (with what to check first), or try the virtual one. The hero is the FM-1's own HOME
  // screen: its six tracks in their colours.
  import { device } from "../lib/model/device.svelte";
  import { hasWebMidi } from "../lib/transport/webmidi";
  import { t, i18n, setLang } from "../lib/i18n/index.svelte";
  import { ic } from "../ui/icons";
  import { plugin } from "../lib/plugin/bridge";
  import { whereText } from "../lib/plugin/where";

  // (plugin) the FM-1 through the plugin's own MIDI ports
  const midi = hasWebMidi() || !!plugin;
  const held = $derived(device.error?.code === "held" || device.error?.code === "taken");
  const busy = $derived(device.status === "connecting" || device.status === "loading");
  let which = $state<"webmidi" | "emulator" | null>(null);
  async function go(kind: "webmidi" | "emulator", force = false) {
    which = kind;
    try { await device.connect(kind, { force }); } catch { /* device.error says why */ }
  }
  const rows = [
    ["1", "SUPERSAW", "LD SS ANTHEM"], ["2", "ACID", "BS 303"], ["3", "MINI", "BS MOOG HARD"],
    ["4", "HOOVER", "LD MENTASM"], ["5", "DRUM-X", "KT RMB ROLL"], ["6", "DRUM", "KT PUNCH 909"],
  ];
</script>

<div class="welcome">
  <header>
    <div class="brand"><span class="mark" aria-hidden="true"></span><span class="dot">GIR EDITOR</span></div>
    <div class="lang" role="group" aria-label="Language">
      <button class:on={i18n.lang === "en"} onclick={() => setLang("en")}>EN</button>
      <button class:on={i18n.lang === "fr"} onclick={() => setLang("fr")}>FR</button>
    </div>
  </header>

  <div class="grid">
    <section class="left">
      <h1>{t("app.tagline")}</h1>

      <div class="actions">
        <button class="act primary" disabled={!midi || busy} onclick={() => go("webmidi")}>
          <i class="ic" aria-hidden="true">{ic("port_usb_c")}</i>
          <span class="tt">{busy && which === "webmidi" ? (device.status === "loading" ? t("welcome.loading") : t("welcome.connecting")) : t("welcome.connect")}</span>
          <span class="sub">{t(plugin ? "welcome.connectSubPlugin" : "welcome.connectSub")}</span>
        </button>
        <button class="act" disabled={busy} onclick={() => go("emulator")}>
          <i class="ic" aria-hidden="true">{ic("control_play_f")}</i>
          <span class="tt">{busy && which === "emulator" ? t("welcome.loading") : t("welcome.demo")}</span>
          <span class="sub">{t("welcome.demoSub")}</span>
        </button>
      </div>
      {#if device.progress}
        <div class="bar" role="progressbar" aria-valuenow={device.progress.pct}><div style="width:{device.progress.pct}%"></div></div>
      {/if}

      {#if device.error && held}
        <!-- (plugin, desktop app) another Gir Editor talks to the FM-1: where, and taking it over -->
        <div class="error held" role="alert">
          <span>{t("err." + device.error.code, { detail: whereText(device.error.detail ?? "") })}</span>
          <button class="take" disabled={busy} onclick={() => go("webmidi", true)}>{t(device.error.code === "taken" ? "welcome.takeBack" : "welcome.takeOver")}</button>
        </div>
      {:else if device.error}
        <div class="error" role="alert">{t("err." + device.error.code, { detail: device.error.detail ?? "" })}</div>
      {/if}
      {#if !midi}
        <div class="note">{t("welcome.nomidi")}</div>
      {/if}

      <ol class="steps" aria-label={t("welcome.steps")}>
        <li>{t("welcome.step1")}</li>
        <li>{t("welcome.step2")}</li>
        <li>{t(plugin ? "welcome.step3Plugin" : "welcome.step3")}</li>
        <li>{t(plugin ? "welcome.step4Plugin" : "welcome.step4")}</li>
      </ol>
    </section>

    <section class="right" aria-hidden="true">
      <div class="unit">
        <div class="screen">
          {#each rows as [n, e, p], i}
            <div class="row" style="--c:var(--t{i + 1})">
              <span class="n">{n}</span><span class="p">{p}</span><span class="e">{e}</span>
              <span class="meter"><span style="width:{[72, 48, 86, 38, 92, 64][i]}%"></span></span>
            </div>
          {/each}
        </div>
        <div class="knobs">{#each [0, 1, 2, 3] as k}<span class="k" style="--r:{[-60, 20, 95, -10][k]}deg"></span>{/each}</div>
      </div>
    </section>
  </div>

  <footer>
    <nav><a href={plugin ? "https://valentingir21.github.io/skiff/try/" : "../try/"}>{t("welcome.try")}</a><a href="https://github.com/valentingir21/skiff/releases">{t("welcome.desktop")}</a><a href="https://github.com/valentingir21/skiff">Source</a></nav>
    {t("welcome.footer")} · Skiff (illuminus5) · Felucca (Leo Kuroshita, Hügelton Instruments)
  </footer>
</div>

<style>
  .welcome { height: 100%; display: flex; flex-direction: column; padding: 28px 44px; overflow: auto; background:
             radial-gradient(1200px 600px at 85% 30%, rgb(255 89 49 / 7%), transparent 60%), var(--bg); }
  header { display: flex; align-items: center; justify-content: space-between; }
  .brand { display: flex; align-items: center; gap: 12px; font-size: 18px; }
  .mark { width: 22px; height: 22px; border-radius: 6px; background: conic-gradient(var(--t1) 0 60deg, var(--t2) 0 120deg, var(--t3) 0 180deg, var(--t4) 0 240deg, var(--t5) 0 300deg, var(--t6) 0); }
  .lang { display: flex; gap: 2px; background: var(--surf); border-radius: 8px; padding: 2px; }
  .lang button { background: none; border: 0; color: var(--dim); font-weight: 700; font-size: 12px; padding: 4px 9px; border-radius: 6px; }
  .lang button.on { background: var(--surf-4); color: var(--fg); }
  .grid { flex: 1; display: grid; grid-template-columns: minmax(380px, 560px) 1fr; gap: 64px; align-items: center; max-width: 1240px; width: 100%; margin: 0 auto; padding: 32px 0; }
  h1 { font-size: 38px; line-height: 1.12; letter-spacing: -0.02em; font-weight: 700; margin: 0 0 32px; max-width: 16ch; }
  .actions { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
  .act { position: relative; display: grid; grid-template-columns: auto 1fr; grid-template-rows: auto auto; column-gap: 12px; text-align: left; padding: 18px; border-radius: var(--r-xl);
         background: var(--surf-2); border: 1px solid var(--line-2); transition: transform var(--t-fast), border-color var(--t-fast), background var(--t-fast); }
  .act:hover:not(:disabled) { border-color: var(--fg-2); transform: translateY(-1px); }
  .act:disabled { opacity: 0.5; cursor: default; }
  .act .ic { grid-row: span 2; font-size: 22px; width: 40px; height: 40px; border-radius: 12px; display: grid; place-items: center; background: var(--surf-4); }
  .act.primary { background: var(--accent); border-color: var(--accent); color: var(--accent-ink); }
  .act.primary .ic { background: rgb(0 0 0 / 14%); }
  .act.primary .sub { color: rgb(26 8 2 / 72%); }
  .tt { font-size: 16px; font-weight: 700; }
  .sub { font-size: 12.5px; color: var(--dim); }
  .bar { height: 3px; background: var(--surf-3); border-radius: 3px; margin-top: 14px; overflow: hidden; }
  .bar div { height: 100%; background: var(--accent); transition: width var(--t-med); }
  .error { margin-top: 16px; padding: 12px 14px; border-radius: var(--r-m); background: color-mix(in srgb, var(--err) 14%, var(--surf)); border: 1px solid color-mix(in srgb, var(--err) 40%, transparent); color: #ffd0d4; line-height: 1.45; }
  .error.held { display: flex; align-items: center; gap: 14px; justify-content: space-between; }
  .take { flex: none; padding: 8px 14px; border-radius: var(--r-m); background: var(--accent); color: var(--accent-ink); border: 0; font-weight: 700; }
  .note { margin-top: 16px; padding: 12px 14px; border-radius: var(--r-m); background: var(--surf); color: var(--fg-2); line-height: 1.45; }
  .steps { margin: 32px 0 0; padding: 0; list-style: none; counter-reset: s; display: grid; gap: 10px; color: var(--fg-2); line-height: 1.45; }
  .steps li { counter-increment: s; display: grid; grid-template-columns: 26px 1fr; }
  .steps li::before { content: counter(s); font-family: var(--font-dot); color: var(--dim); }
  .right { display: grid; place-items: center; }
  .unit { width: min(440px, 100%); aspect-ratio: 1 / 1.08; border-radius: 34px; background: linear-gradient(160deg, #1d1d21, #111113); border: 1px solid #2b2b31;
          box-shadow: 0 40px 80px rgb(0 0 0 / 55%), inset 0 1px 0 rgb(255 255 255 / 6%); padding: 34px; display: flex; flex-direction: column; gap: 24px; transform: rotate(-2deg); }
  .screen { flex: 1; border-radius: 10px; background: #080808; border: 6px solid #050505; padding: 14px 12px; display: flex; flex-direction: column; justify-content: space-around; font-family: var(--font-dot); }
  .row { display: grid; grid-template-columns: 22px 1fr auto; grid-template-rows: auto 5px; column-gap: 8px; align-items: center; font-size: 15px; }
  .n { grid-row: span 2; width: 22px; height: 22px; display: grid; place-items: center; border-radius: 5px; background: var(--c); color: #080808; font-size: 13px; }
  .p { color: #fffaff; }
  .e { color: var(--c); font-size: 12px; }
  .meter { grid-column: 2 / 4; height: 3px; background: #1e1e1e; border-radius: 2px; overflow: hidden; }
  .meter span { display: block; height: 100%; background: var(--c); opacity: 0.8; }
  .knobs { display: flex; justify-content: space-around; }
  .k { width: 46px; height: 46px; border-radius: 50%; background: radial-gradient(circle at 40% 35%, #3a3a40, #17171a 70%); border: 1px solid #333; position: relative; transform: rotate(var(--r)); }
  .k::after { content: ""; position: absolute; left: 50%; top: 6px; width: 3px; height: 12px; margin-left: -1.5px; border-radius: 2px; background: #eee; }
  footer { color: var(--mute); font-size: 12px; text-align: center; display: flex; flex-direction: column; gap: 8px; }
  footer nav { display: flex; justify-content: center; gap: 18px; }
  footer a { color: var(--fg-2); text-decoration: none; }
  footer a:hover { color: var(--fg); text-decoration: underline; }
  @media (max-width: 900px) { .grid { grid-template-columns: 1fr; } .right { display: none; } .welcome { padding: 20px; } .actions { grid-template-columns: 1fr; } h1 { font-size: 30px; } }
</style>
