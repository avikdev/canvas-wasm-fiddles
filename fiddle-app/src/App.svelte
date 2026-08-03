<script lang="ts">
import type { FiddleId } from "@canvas-wasm-fiddles/canvas-worker";
import { ArrowUpRight, Braces, Menu, Sparkles } from "lucide-svelte";
import CanvasStage from "./lib/CanvasStage.svelte";
import { Button } from "./lib/components/ui/button";
import { Separator } from "./lib/components/ui/separator";
import { fiddles } from "./lib/fiddles";

let selectedId = $state<FiddleId>("ribbon-field");
let selected = $derived(fiddles.find((fiddle) => fiddle.id === selectedId) ?? fiddles[0]);
let navOpen = $state(true);
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
        <p>Graphics Fiddles</p>
        <span>Skia / Wasm Playground</span>
      </div>
    </div>
  </header>

  <div class:nav-closed={!navOpen} class="app-body">
    <aside class="sidebar" id="fiddle-sidebar">
      <div class="nav-intro">
        <p class="eyebrow nav-heading"><Sparkles size={13} /> Experiments</p>
        <p class="nav-description">
          Canvas sketches coded with Google's Skia 2D graphics library, and rendered away from the
          main thread by a C++ Wasm engine.
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
              onclick={() => (selectedId = fiddle.id)}
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
          href="https://github.com/avikdev/canvas-wasm-fiddles"
          target="_blank"
          rel="noreferrer"
        >
          <svg viewBox="0 0 24 24" aria-hidden="true">
            <path
              d="M12 .7a11.5 11.5 0 0 0-3.64 22.41c.58.1.79-.25.79-.56v-2.2c-3.22.7-3.9-1.37-3.9-1.37-.52-1.34-1.29-1.7-1.29-1.7-1.05-.72.08-.71.08-.71 1.16.08 1.77 1.2 1.77 1.2 1.04 1.77 2.72 1.26 3.38.96.1-.75.4-1.26.74-1.55-2.57-.3-5.27-1.29-5.27-5.69 0-1.26.45-2.29 1.19-3.09-.12-.29-.52-1.46.11-3.05 0 0 .97-.31 3.16 1.18A10.98 10.98 0 0 1 12 6.14c.98 0 1.95.13 2.87.39 2.2-1.49 3.16-1.18 3.16-1.18.63 1.59.23 2.76.11 3.05.74.8 1.19 1.83 1.19 3.09 0 4.41-2.71 5.39-5.29 5.68.42.36.79 1.07.79 2.15v3.23c0 .31.21.67.8.56A11.5 11.5 0 0 0 12 .7Z"
            />
          </svg>
          <span>GitHub repo</span>
        </a>
        <a href="https://webassembly.org/" target="_blank" rel="noreferrer">
          <span class="wasm-icon" aria-hidden="true">W</span>
          <span>WebAssembly</span>
        </a>
        <a class="text-resource-link" href="https://skia.org/" target="_blank" rel="noreferrer">
          Skia
        </a>
      </div>
    </aside>

    <main class="main-panel">
      <header class="fiddle-header">
        <h2>{selected.title}</h2>
        <div class="render-chip">
          <span></span>
          Worker rendering
        </div>
      </header>

      <section class="canvas-wrap" aria-live="polite">
        {#key selected.id}
          <CanvasStage fiddle={selected.id} />
        {/key}
      </section>

      <footer class="fiddle-meta">
        <p>{selected.summary}</p>
        <div>
          <span>Renderer</span>
          <strong>
            {selected.id === "skia-webgl" ||
            selected.id === "ribbon-field" ||
            selected.id === "elastic-text" ||
            selected.id === "sksl-image-proc" ||
            selected.id === "shape-tracing" ||
            selected.id === "shape-morphing" ||
            selected.id === "vortex-field"
              ? "Skia Ganesh / WebGL"
              : selected.id === "skia-cpu"
                ? "Skia Raster / CPU"
                : "OffscreenCanvas / 2D"}
          </strong>
        </div>
        <div>
          <span>Thread</span>
          <strong>Dedicated worker</strong>
        </div>
      </footer>
    </main>
  </div>
</div>
