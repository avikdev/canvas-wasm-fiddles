<script lang="ts">
import type { CanvasWorkerMessage, CanvasWorkerStatus } from "@canvas-wasm-fiddles/canvas-worker";
import CanvasWorker from "@canvas-wasm-fiddles/canvas-worker/worker?worker";
import { onMount } from "svelte";
import type { FiddleId } from "./fiddles";

let {
  fiddle,
  paused,
  onSvgWritableChange,
}: {
  fiddle: FiddleId;
  paused: boolean;
  onSvgWritableChange?: (writable: boolean) => void;
} = $props();
let canvasElement: HTMLCanvasElement;
let stageElement: HTMLDivElement;
let worker: Worker | undefined;
let supported = $state(true);
let nextSvgRequestId = 1;
const pendingSvgRequests = new Map<
  number,
  { resolve: (svg: string) => void; reject: (error: Error) => void }
>();

function send(message: CanvasWorkerMessage, transfer?: Transferable[]) {
  worker?.postMessage(message, transfer ?? []);
}

export function exportSvg(): Promise<string> {
  if (!worker) {
    return Promise.reject(new Error("The canvas worker is not ready."));
  }
  const requestId = nextSvgRequestId++;
  return new Promise((resolve, reject) => {
    pendingSvgRequests.set(requestId, { resolve, reject });
    send({ type: "export-svg", requestId });
  });
}

$effect(() => {
  send({ type: "animation", paused });
});

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
    } else if (event.data.type === "svg-capability") {
      onSvgWritableChange?.(event.data.writable);
    } else if (event.data.type === "svg-export") {
      const pending = pendingSvgRequests.get(event.data.requestId);
      if (!pending) return;
      pendingSvgRequests.delete(event.data.requestId);
      if (event.data.error) {
        pending.reject(new Error(event.data.error));
      } else if (event.data.svg) {
        pending.resolve(event.data.svg);
      } else {
        pending.reject(new Error("The canvas worker returned an empty SVG."));
      }
    } else {
      console.info(`[canvas-worker] ${event.data.message}`);
    }
  });
  const offscreen = canvasElement.transferControlToOffscreen();
  let resizeTimer: ReturnType<typeof setTimeout> | undefined;
  let lastPixelWidth = 0;
  let lastPixelHeight = 0;
  let lastDpr = 0;

  const measure = () => {
    const bounds = stageElement.getBoundingClientRect();
    return {
      width: Math.max(1, bounds.width),
      height: Math.max(1, bounds.height),
      dpr: Math.min(window.devicePixelRatio, 2),
    };
  };

  const rememberDimensions = (dimensions: ReturnType<typeof measure>) => {
    lastPixelWidth = Math.max(1, Math.round(dimensions.width * dimensions.dpr));
    lastPixelHeight = Math.max(1, Math.round(dimensions.height * dimensions.dpr));
    lastDpr = dimensions.dpr;
  };

  const resize = () => {
    const dimensions = measure();
    const pixelWidth = Math.max(1, Math.round(dimensions.width * dimensions.dpr));
    const pixelHeight = Math.max(1, Math.round(dimensions.height * dimensions.dpr));
    if (
      pixelWidth === lastPixelWidth &&
      pixelHeight === lastPixelHeight &&
      dimensions.dpr === lastDpr
    ) {
      return;
    }
    rememberDimensions(dimensions);
    send({ type: "resize", ...dimensions });
  };

  const scheduleResize = () => {
    if (resizeTimer !== undefined) clearTimeout(resizeTimer);
    resizeTimer = setTimeout(() => {
      resizeTimer = undefined;
      resize();
    }, 150);
  };

  const dimensions = measure();
  rememberDimensions(dimensions);
  send(
    {
      type: "init",
      canvas: offscreen,
      fiddle,
      assetBaseUrl: import.meta.env.BASE_URL,
      paused,
      ...dimensions,
    },
    [offscreen],
  );

  const observer = new ResizeObserver(scheduleResize);
  observer.observe(stageElement);

  return () => {
    observer.disconnect();
    if (resizeTimer !== undefined) clearTimeout(resizeTimer);
    worker?.terminate();
    worker = undefined;
    for (const pending of pendingSvgRequests.values()) {
      pending.reject(new Error("The canvas worker was stopped."));
    }
    pendingSvgRequests.clear();
    onSvgWritableChange?.(false);
  };
});
</script>

<div
  class="canvas-stage"
  class:shape-intersection-stage={fiddle === "shape-intersection"}
  bind:this={stageElement}
>
  <canvas bind:this={canvasElement} aria-label="Animated preview of the selected fiddle"></canvas>
  {#if !supported}
    <div class="unsupported">
      This browser cannot hand a canvas to a Web Worker. Try the latest Chrome, Edge, or Firefox.
    </div>
  {/if}
</div>
