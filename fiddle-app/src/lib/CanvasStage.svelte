<script lang="ts">
import type { CanvasWorkerMessage, FiddleId } from "@canvas-wasm-fiddles/canvas-worker";
import CanvasWorker from "@canvas-wasm-fiddles/canvas-worker/worker?worker";
import { onMount } from "svelte";

let { fiddle }: { fiddle: FiddleId } = $props();
let canvasElement: HTMLCanvasElement;
let stageElement: HTMLDivElement;
let worker: Worker | undefined;
let supported = $state(true);

function send(message: CanvasWorkerMessage, transfer?: Transferable[]) {
  worker?.postMessage(message, transfer ?? []);
}

$effect(() => {
  // Read the prop unconditionally so Svelte tracks it even before the worker
  // is created during onMount.
  const selectedFiddle = fiddle;
  if (worker) send({ type: "select", fiddle: selectedFiddle });
});

onMount(() => {
  if (!canvasElement.transferControlToOffscreen) {
    supported = false;
    return;
  }

  worker = new CanvasWorker();
  const offscreen = canvasElement.transferControlToOffscreen();

  const resize = () => {
    const bounds = stageElement.getBoundingClientRect();
    const dimensions = {
      width: Math.max(1, bounds.width),
      height: Math.max(1, bounds.height),
      dpr: Math.min(window.devicePixelRatio, 2),
    };

    send({ type: "resize", ...dimensions });
    return dimensions;
  };

  const dimensions = resize();
  send({ type: "init", canvas: offscreen, fiddle, ...dimensions }, [offscreen]);

  const observer = new ResizeObserver(resize);
  observer.observe(stageElement);

  return () => {
    observer.disconnect();
    worker?.terminate();
    worker = undefined;
  };
});
</script>

<div class="canvas-stage" bind:this={stageElement}>
  <canvas bind:this={canvasElement} aria-label="Animated preview of the selected fiddle"></canvas>
  {#if !supported}
    <div class="unsupported">
      This browser cannot hand a canvas to a Web Worker. Try the latest Chrome, Edge, or Firefox.
    </div>
  {/if}
  <div class="canvas-chip">
    <span></span>
    Worker rendering
  </div>
</div>
