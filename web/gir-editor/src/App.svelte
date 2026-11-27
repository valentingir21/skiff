<script lang="ts">
  import { device } from "./lib/model/device.svelte";
  import { ui, go, toastKey, VIEWS, TRACK_COLORS, type View } from "./lib/ui.svelte";
  import { t, i18n } from "./lib/i18n/index.svelte";
  import Welcome from "./shell/Welcome.svelte";
  import TopBar from "./shell/TopBar.svelte";
  import TrackColumn from "./shell/TrackColumn.svelte";
  import ViewTabs from "./shell/ViewTabs.svelte";
  import HelpBar from "./shell/HelpBar.svelte";
  import CommandPalette from "./shell/CommandPalette.svelte";
  import Shortcuts from "./shell/Shortcuts.svelte";
  import Toasts from "./ui/components/Toasts.svelte";
  import Interrupted from "./shell/Interrupted.svelte";
  import ConfirmDialog from "./ui/components/ConfirmDialog.svelte";
  import SoundView from "./views/SoundView.svelte";
  import SeqView from "./views/SeqView.svelte";
  import SongView from "./views/SongView.svelte";
  import MixerView from "./views/MixerView.svelte";
  import LibraryView from "./views/LibraryView.svelte";
  import ProjectsView from "./views/ProjectsView.svelte";
  import SamplesView from "./views/SamplesView.svelte";
  import Fm6View from "./views/Fm6View.svelte";
  import ScreenView from "./views/ScreenView.svelte";
  import SettingsView from "./views/SettingsView.svelte";
  import DemoView from "./views/DemoView.svelte";
  import CompactView from "./views/CompactView.svelte";
  import { initDesktop } from "./lib/desktop";
  initDesktop();

  device.onEvent = (e) => toastKey(e.level, e.key, e.vars);

  const connected = $derived(device.status === "ready" || device.status === "lost" || device.status === "reconnecting");
  const tc = $derived(TRACK_COLORS[device.sel] ?? "var(--t1)");
  // the "demo" view: the components' gallery (?view=demo), for the design review
  const qs = new URLSearchParams(location.search);
  if (qs.get("view") === "demo") ui.view = "demo";
  if (qs.get("demo") === "1" && device.status === "off") device.connect("emulator").catch(() => {});
  const compact = qs.get("compact") === "1";

  $effect(() => { document.documentElement.lang = i18n.lang; });

  function isTyping(e: KeyboardEvent) {
    const el = e.target as HTMLElement;
    return el && (el.tagName === "INPUT" || el.tagName === "TEXTAREA" || el.tagName === "SELECT" || el.isContentEditable);
  }
  function onKey(e: KeyboardEvent) {
    const mod = e.metaKey || e.ctrlKey;
    if (mod && e.key.toLowerCase() === "k") { e.preventDefault(); ui.palette = !ui.palette; return; }
    if (ui.palette || ui.confirm) return;
    if (e.key === "Escape") { ui.help = false; return; }
    if (isTyping(e)) return;
    if (mod && e.key.toLowerCase() === "z") { e.preventDefault(); if (e.shiftKey) device.redo(); else device.undo(); return; }
    if (mod && e.key.toLowerCase() === "y") { e.preventDefault(); device.redo(); return; }
    if (e.key === "?" ) { ui.help = !ui.help; return; }
    if (!connected) return;
    if (e.code === "Space" && !mod) { e.preventDefault(); device.togglePlay(); return; }
    if (e.altKey && /^Digit[1-9]$/.test(e.code)) { e.preventDefault(); const v = VIEWS[+e.code.slice(5) - 1]; if (v) go(v); return; }
    if (!mod && !e.altKey && /^[1-6]$/.test(e.key)) { device.selectTrack(+e.key - 1); return; }
  }
</script>

<svelte:window onkeydown={onKey} />

{#if ui.view === "demo"}
  <DemoView />
{:else if !connected}
  <Welcome />
{:else if compact}
  <div class="app" style="--tc:{tc}; --accent:{tc}"><CompactView /></div>
{:else}
  <div class="app" style="--tc:{tc}; --accent:{tc}">
    <TopBar />
    <div class="body">
      <TrackColumn />
      <main class="main">
        <ViewTabs />
        <section class="view scroll" aria-label={t("view." + ui.view)}>
          {#if ui.view === "sound"}<SoundView />
          {:else if ui.view === "seq"}<SeqView />
          {:else if ui.view === "song"}<SongView />
          {:else if ui.view === "mixer"}<MixerView />
          {:else if ui.view === "library"}<LibraryView />
          {:else if ui.view === "projects"}<ProjectsView />
          {:else if ui.view === "samples"}<SamplesView />
          {:else if ui.view === "fm6"}<Fm6View />
          {:else if ui.view === "screen"}<ScreenView />
          {:else if ui.view === "settings"}<SettingsView />
          {/if}
        </section>
        <HelpBar />
      </main>
    </div>
    {#if device.status === "lost" || device.status === "reconnecting"}
      <div class="lost" role="alert">
        <span class="pulse"></span>
        {device.status === "lost" ? t("toast.lost") : t("status.reconnecting") + "…"}
        <button onclick={() => device.connect(device.kind ?? "webmidi").catch(() => {})}>{t("welcome.connect")}</button>
      </div>
    {/if}
  </div>
{/if}

<CommandPalette />
<Shortcuts />
<ConfirmDialog />
<Interrupted />
<Toasts />

<style>
  .app { height: 100%; display: flex; flex-direction: column; }
  .body { flex: 1; min-height: 0; display: flex; }
  .main { flex: 1; min-width: 0; display: flex; flex-direction: column; background: var(--bg-2); border-top-left-radius: var(--r-xl); border: 1px solid var(--line); border-right: 0; border-bottom: 0; }
  .view { flex: 1; min-height: 0; padding: 20px 24px 24px; }
  .lost { position: fixed; top: 64px; left: 50%; transform: translateX(-50%); z-index: 80; display: flex; align-items: center; gap: 12px; padding: 10px 12px 10px 16px;
          background: color-mix(in srgb, var(--warn) 18%, var(--surf-2)); border: 1px solid color-mix(in srgb, var(--warn) 50%, transparent); border-radius: var(--r-l); box-shadow: var(--shadow-pop); }
  .lost button { background: var(--warn); color: #1a1200; border: 0; border-radius: 6px; padding: 5px 10px; font-weight: 700; }
  .pulse { width: 9px; height: 9px; border-radius: 50%; background: var(--warn); animation: pl 1.2s infinite; }
  @keyframes pl { 50% { opacity: 0.3; } }
</style>
