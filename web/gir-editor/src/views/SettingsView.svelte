<script lang="ts">
  // Tempo and groove (the project's), the FM-1's MENU (as the device describes it: MENU_DESC), the editor's own
  // settings, and what this is
  import { onMount } from "svelte";
  import { device } from "../lib/model/device.svelte";
  import { ID, T } from "../lib/model/tables";
  import { ask, toast } from "../lib/ui.svelte";
  import { t, i18n, setLang } from "../lib/i18n/index.svelte";
  import { EmulatorTransport } from "../lib/transport/emulator";
  import Param from "../ui/Param.svelte";
  import Seg from "../ui/components/Seg.svelte";
  import Button from "../ui/components/Button.svelte";

  onMount(() => { device.readMenu().catch(() => {}); });
  const tabs = $derived([...new Set(device.menu.map((m) => m.tabName || "MENU"))]);
  let vol = $state(0.8);
  $effect(() => { device.emu?.setVolume(vol); });
  async function resetDemo() {
    if (!(await ask({ title: t("set.resetDemo"), body: t("set.resetDemoWhat"), ok: t("dlg.erase"), danger: true }))) return;
    await EmulatorTransport.resetFlash();
    await device.disconnect();
    toast("ok", t("set.resetDemo"));
  }
  const glob = T.PAGES.find((p) => p.title === "GLOBAL" && p.fam === "GLO")!;
</script>

<div class="set">
  <section class="card">
    <h2>{t("set.global")}</h2>
    <div class="row">
      <Param id={ID.G_BPM} scope={1} page={glob} slot={0} where="GLO ×6 › GLOBAL › KNOB 1" size={56} />
      <Param id={ID.G_SWING} scope={1} page={glob} slot={1} where="GLO ×6 › GLOBAL › KNOB 2" size={56} />
    </div>
  </section>

  <section class="card wide">
    <h2>{t("set.menu")}</h2>
    {#if !device.menu.length}<p class="muted">…</p>{/if}
    <div class="tabs">
      {#each tabs as tb}
        <div class="tab">
          <h3 class="dot">{tb}</h3>
          {#each device.menu.filter((m) => (m.tabName || "MENU") === tb) as m (m.id)}
            <div class="item">
              <span class="nm dot">{m.name}</span>
              {#if m.kind === 0 && m.names && m.names.length <= 4}
                <Seg size="s" label={m.name} value={m.value} onchange={(v) => device.setMenu(m.id, v)} options={m.names.map((n, i) => ({ value: m.min + i, label: n }))} />
              {:else if m.kind === 0 && m.names}
                <select value={m.value} onchange={(e) => device.setMenu(m.id, +(e.currentTarget as HTMLSelectElement).value)} aria-label={m.name}>
                  {#each m.names as n, i}<option value={m.min + i}>{n}</option>{/each}
                </select>
              {:else}
                <input type="number" min={m.min} max={m.max} value={m.value} aria-label={m.name}
                       onchange={(e) => device.setMenu(m.id, +(e.currentTarget as HTMLInputElement).value)} /><span class="u">{m.unit}</span>
              {/if}
            </div>
          {/each}
        </div>
      {/each}
    </div>
  </section>

  <section class="card">
    <h2>{t("set.editor")}</h2>
    <div class="item"><span class="nm">{t("set.lang")}</span>
      <Seg size="s" label={t("set.lang")} value={i18n.lang} onchange={(v) => setLang(v)} options={[{ value: "en", label: "English" }, { value: "fr", label: "Français" }]} /></div>
    {#if device.emu}
      <div class="item"><span class="nm">{t("set.volume")}</span><input type="range" min="0" max="1" step="0.05" bind:value={vol} aria-label={t("set.volume")} /></div>
      <div class="item"><span class="nm">{t("set.resetDemo")}</span><Button size="s" variant="danger" onclick={resetDemo}>{t("set.resetDemo")}</Button></div>
    {/if}
  </section>

  <section class="card">
    <h2>{t("set.about")}</h2>
    <dl>
      <dt>Gir Editor</dt><dd>1.0.0-rc1 · GPL-3.0</dd>
      <dt>{t("set.firmware")}</dt><dd>{device.info?.version} · {device.tablesMatch ? "Gir 1.0" : ""}</dd>
      <dt>{t("set.tables")}</dt><dd>{device.tablesMatch ? t("set.tablesMatch") : t("set.tablesOther")}</dd>
      <dt>Link</dt><dd>{device.stats.sent} ↑ · {device.stats.received} ↓ · {device.stats.timeouts} timeouts</dd>
    </dl>
    <h3>{t("set.credits")}</h3>
    <p class="cred">Skiff and its editor: illuminus5. Felucca: Leo Kuroshita (Hügelton Instruments), Fukiai icons (MIT).
      ACID: ATEK303 by Javier Melgar (Animatek), after Open303 by Robin Schmidt. MINI: FMini-1 by Guilherme Ruppert.
      Web emulator after X0X by charlesvestal. DotGothic16 and Inter: SIL Open Font License. Gir: the FM-1 fork for techno.</p>
  </section>
</div>

<style>
  .set { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; max-width: 1200px; }
  .card { background: var(--surf); border: 1px solid var(--line); border-radius: var(--r-l); padding: 16px; display: flex; flex-direction: column; gap: 10px; }
  .wide { grid-column: span 2; }
  h2 { margin: 0; font-size: 13px; color: var(--fg-2); }
  h3 { margin: 6px 0 4px; font-size: 13px; font-weight: 400; color: var(--accent); }
  .row { display: flex; gap: 8px; }
  .tabs { display: grid; grid-template-columns: repeat(auto-fill, minmax(320px, 1fr)); gap: 12px 24px; }
  .item { display: flex; align-items: center; justify-content: space-between; gap: 10px; padding: 6px 0; border-bottom: 1px solid var(--line); }
  .nm { font-size: 13.5px; }
  select, input[type="number"] { height: 28px; border-radius: 6px; border: 1px solid var(--line-2); background: var(--bg); padding: 0 8px; }
  input[type="number"] { width: 80px; }
  .u { color: var(--dim); font-size: 12px; }
  dl { display: grid; grid-template-columns: max-content 1fr; gap: 6px 16px; margin: 0; font-size: 13px; }
  dt { color: var(--dim); }
  dd { margin: 0; }
  .cred { margin: 0; color: var(--fg-2); font-size: 12.5px; line-height: 1.55; }
  .muted { color: var(--dim); }
  @media (max-width: 1000px) { .set { grid-template-columns: 1fr; } .wide { grid-column: auto; } }
</style>
