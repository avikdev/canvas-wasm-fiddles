<script lang="ts">
import type { FiddleId } from "@canvas-wasm-fiddles/canvas-worker";
import { ArrowUpRight, Braces, GitBranch, Sparkles } from "lucide-svelte";
import CanvasStage from "./lib/CanvasStage.svelte";
import { Button } from "./lib/components/ui/button";
import { Separator } from "./lib/components/ui/separator";
import { fiddles } from "./lib/fiddles";

let selectedId = $state<FiddleId>("orbital-bloom");
let selected = $derived(fiddles.find((fiddle) => fiddle.id === selectedId) ?? fiddles[0]);
</script>

<svelte:head>
  <title>{selected.title} · Canvas Wasm Fiddles</title>
</svelte:head>

<div class="app-shell">
  <aside class="sidebar">
    <div class="brand">
      <div class="brand-mark"><Braces size={20} strokeWidth={2.2} /></div>
      <div>
        <p>Canvas + Wasm</p>
        <span>Fiddle cabinet</span>
      </div>
    </div>

    <div class="sidebar-copy">
      <p class="eyebrow"><Sparkles size={13} /> Experiments</p>
      <h1>Small studies in motion.</h1>
    <p>Canvas sketches rendered away from the main thread by a C++ Wasm engine.</p>
    </div>

    <Separator />

    <nav aria-label="Fiddles" class="fiddle-nav">
      <p class="nav-label">Choose a fiddle</p>
      {#each fiddles as fiddle}
        <Button
          variant="ghost"
          class={`fiddle-link${selectedId === fiddle.id ? " active" : ""}`}
          aria-current={selectedId === fiddle.id ? "page" : undefined}
          onclick={() => (selectedId = fiddle.id)}
        >
          <span class="fiddle-number">{fiddle.number}</span>
          <span class="fiddle-icon" style:--fiddle-color={fiddle.color}>
            <fiddle.icon size={17} />
          </span>
          <span class="fiddle-link-copy">
            <strong>{fiddle.title}</strong>
            <small>{fiddle.technique}</small>
          </span>
          <ArrowUpRight class="nav-arrow" size={15} />
        </Button>
      {/each}
    </nav>

    <div class="sidebar-footer">
      <span><i></i> Web Worker online</span>
      <a
        href="https://github.com/avikdev/canvas-wasm-fiddles"
        aria-label="Open the project on GitHub"
      >
        <GitBranch size={17} />
      </a>
    </div>
  </aside>

  <main class="main-panel">
    <header class="fiddle-header">
      <div>
        <p class="eyebrow">Fiddle {selected.number}</p>
        <h2>{selected.title}</h2>
      </div>
      <div class="index-counter">{selected.number} <span>/</span> {String(fiddles.length).padStart(3, "0")}</div>
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
        <strong>{selected.id === "skia-pulse" ? "Skia Graphite / WebGPU" : "OffscreenCanvas / 2D"}</strong>
      </div>
      <div>
        <span>Thread</span>
        <strong>Dedicated worker</strong>
      </div>
    </footer>
  </main>
</div>
