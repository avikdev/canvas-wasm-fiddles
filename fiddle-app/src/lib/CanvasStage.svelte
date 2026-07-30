<script lang="ts">
import type {
  CanvasWorkerMessage,
  CanvasWorkerStatus,
  FiddleId,
} from "@canvas-wasm-fiddles/canvas-worker";
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

onMount(() => {
  if (!canvasElement.transferControlToOffscreen) {
    supported = false;
    return;
  }

  worker = new CanvasWorker();
  worker.addEventListener("message", (event: MessageEvent<CanvasWorkerStatus>) => {
    if (event.data.type === "error") {
      console.error(`[canvas-worker] ${event.data.message}`);
    } else if (event.data.type === "ready") {
      console.info("[canvas-worker] C++ renderer ready.");
    } else {
      console.info(`[canvas-worker] ${event.data.message}`);
    }
  });
  const offscreen = canvasElement.transferControlToOffscreen();
  let resizeTimer: ReturnType<typeof setTimeout> | undefined;

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

  const scheduleResize = () => {
    if (resizeTimer !== undefined) clearTimeout(resizeTimer);
    resizeTimer = setTimeout(() => {
      resizeTimer = undefined;
      resize();
    }, 150);
  };

  const dimensions = resize();
  send({ type: "init", canvas: offscreen, fiddle, ...dimensions }, [offscreen]);

  const observer = new ResizeObserver(scheduleResize);
  observer.observe(stageElement);

  return () => {
    observer.disconnect();
    if (resizeTimer !== undefined) clearTimeout(resizeTimer);
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
