<script lang="ts">
import { ArrowUpRight, Braces, Menu, Pause, Play, Save, Sparkles } from "lucide-svelte";
import { onMount } from "svelte";
import CanvasStage from "./lib/CanvasStage.svelte";
import { Button } from "./lib/components/ui/button";
import { Separator } from "./lib/components/ui/separator";
import { fiddles, type FiddleId } from "./lib/fiddles";
import { downloadSvg } from "./lib/svg-download";

let selectedId = $state<FiddleId>("text-reflow");
let selected = $derived(fiddles.find((fiddle) => fiddle.id === selectedId) ?? fiddles[0]);
let compactNavigation = $state<boolean>(
  typeof window === "undefined" ? false : window.matchMedia("(max-width: 760px)").matches,
);
// svelte-ignore state_referenced_locally
let navOpen = $state(!compactNavigation);
let animationPaused = $state(false);
let svgWritable = $state(false);
let svgSaving = $state(false);
let canvasStage: { exportSvg(): Promise<Blob> } | undefined = $state();

function selectFiddle(id: FiddleId) {
  selectedId = id;
  animationPaused = false;
  svgWritable = false;
  if (compactNavigation) {
    navOpen = false;
  }
}

async function saveSvg() {
  if (!canvasStage || !animationPaused || !svgWritable || svgSaving) return;

  const filename = `${selected.id}-${Math.floor(Date.now() / 1000)}.svg`;
  svgSaving = true;
  try {
    const blob = await canvasStage.exportSvg();
    downloadSvg(blob, filename);
  } catch (error) {
    console.error("Could not save the SVG frame.", error);
  } finally {
    svgSaving = false;
  }
}

onMount(() => {
  const mediaQuery = window.matchMedia("(max-width: 760px)");
  const updateNavigationMode = (event: MediaQueryListEvent | MediaQueryList) => {
    compactNavigation = event.matches;
    navOpen = !event.matches;
  };

  updateNavigationMode(mediaQuery);
  mediaQuery.addEventListener("change", updateNavigationMode);
  return () => mediaQuery.removeEventListener("change", updateNavigationMode);
});
</script>

<svelte:head>
  <title>{selected.title} · Canvas Wasm Fiddles</title>
</svelte:head>

<div class="app-shell">
  <header class="app-bar">
    <button
      type="button"
      class="nav-toggle"
      aria-label={navOpen ? "Close fiddle navigation" : "Open fiddle navigation"}
      aria-controls="fiddle-sidebar"
      aria-expanded={navOpen}
      onclick={() => (navOpen = !navOpen)}
    >
      <Menu size={22} strokeWidth={2} />
    </button>

    <div class="brand">
      <div class="brand-mark"><Braces size={20} strokeWidth={2.2} /></div>
      <div class="brand-copy">
        <p>Canvas Fiddles</p>
      </div>
    </div>
  </header>

  <div class:nav-closed={!navOpen} class="app-body">
    <aside class="sidebar" id="fiddle-sidebar">
      <div class="nav-intro">
        <p class="eyebrow nav-heading"><Sparkles size={13} /> Fiddles</p>
        <p class="nav-description">
          Some graphic design demos made with Google's Skia 2D graphics library, and rendered by a
          C++ Wasm engine in a web-worker.
        </p>
      </div>

      <Separator />

      <nav aria-label="Fiddles" class="fiddle-nav">
        {#each fiddles as fiddle}
          <div class="fiddle-link-shell" class:active={selectedId === fiddle.id}>
            <Button
              variant="ghost"
              class="fiddle-link"
              aria-current={selectedId === fiddle.id ? "page" : undefined}
              onclick={() => selectFiddle(fiddle.id)}
            >
              <span class="fiddle-icon" style:--fiddle-color={fiddle.color}>
                {fiddle.initials}
              </span>
              <span class="fiddle-link-copy">
                <strong>{fiddle.title}</strong>
                <small>{fiddle.technique}</small>
              </span>
              <ArrowUpRight class="nav-arrow" size={15} />
            </Button>
          </div>
        {/each}
      </nav>

      <div class="sidebar-footer">
        <a
          class="skia-resource-link"
          href="https://skia.org/"
          target="_blank"
          rel="noreferrer"
          aria-label="Skia"
          title="Skia"
        >
          <img src="/images/skia-logo.png" alt="" />
        </a>
        <a href="https://webassembly.org/" target="_blank" rel="noreferrer">
          <span class="wasm-icon" aria-hidden="true">W</span>
          <span>WebAssembly</span>
        </a>
      </div>
    </aside>

    <button
      type="button"
      class="nav-scrim"
      aria-label="Close fiddle navigation"
      tabindex={navOpen && compactNavigation ? 0 : -1}
      onclick={() => (navOpen = false)}
    ></button>

    <main class="main-panel">
      <header class="fiddle-header">
        <h2>{selected.title}</h2>
        <div class="header-actions">
          <button
            type="button"
            class="animation-toggle"
            aria-label={animationPaused ? "Play animation" : "Pause animation"}
            aria-pressed={animationPaused}
            title={animationPaused ? "Play animation" : "Pause animation"}
            onclick={() => (animationPaused = !animationPaused)}
          >
            {#if animationPaused}
              <Play size={17} fill="currentColor" />
            {:else}
              <Pause size={17} fill="currentColor" />
            {/if}
          </button>
          <button
            type="button"
            class="svg-save-button"
            class:unsupported-svg={!svgWritable}
            disabled={!svgWritable || !animationPaused || svgSaving}
            aria-label="Save current frame as SVG"
            title={svgWritable
              ? animationPaused
                ? "Save current frame as SVG"
                : "Pause the animation to save an SVG"
              : "This fiddle cannot be saved as SVG"}
            onclick={saveSvg}
          >
            <Save size={15} />
            <span>{svgSaving ? "Saving…" : "Save SVG"}</span>
          </button>
          <div class:paused={animationPaused} class="render-chip">
            <span></span>
            {animationPaused ? "Paused" : "Worker rendering"}
          </div>
        </div>
      </header>

      <section class="canvas-wrap" aria-live="polite">
        {#key selected.id}
          <CanvasStage
            bind:this={canvasStage}
            fiddle={selected.id}
            paused={animationPaused}
            onSvgWritableChange={(writable) => (svgWritable = writable)}
          />
        {/key}
      </section>

      <footer class="fiddle-meta">
        <p>{selected.summary}</p>
        <div class="fiddle-tags" aria-label="Fiddle tags">
          {#each selected.tags as tag}
            <span>{tag}</span>
          {/each}
        </div>
      </footer>
    </main>
  </div>
</div>
