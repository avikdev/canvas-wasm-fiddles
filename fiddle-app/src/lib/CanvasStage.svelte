<script lang="ts">
import type {
  CanvasWorkerMessage,
  CanvasWorkerStatus,
  FiddleControlDefinition,
} from "@canvas-wasm-fiddles/canvas-worker";
import CanvasWorker from "@canvas-wasm-fiddles/canvas-worker/worker?worker";
import { onMount } from "svelte";
import type { FiddleId } from "./fiddles";

let {
  fiddle,
  paused,
  onSvgWritableChange,
  onControlsChange,
}: {
  fiddle?: FiddleId;
  paused: boolean;
  onSvgWritableChange?: (writable: boolean) => void;
  onControlsChange?: (controls: FiddleControlDefinition[]) => void;
} = $props();
let canvasElement: HTMLCanvasElement;
let stageElement: HTMLDivElement;
let worker: Worker | undefined;
let supported = $state(true);
let loading = $state(true);
let nextSvgRequestId = 1;
const pendingSvgRequests = new Map<
  number,
  { resolve: (blob: Blob) => void; reject: (error: Error) => void }
>();

function send(message: CanvasWorkerMessage, transfer?: Transferable[]) {
  worker?.postMessage(message, transfer ?? []);
}

export function exportSvg(): Promise<Blob> {
  if (!worker) {
    return Promise.reject(new Error("The canvas worker is not ready."));
  }
  const requestId = nextSvgRequestId++;
  return new Promise((resolve, reject) => {
    pendingSvgRequests.set(requestId, { resolve, reject });
    send({ type: "export-svg", requestId });
  });
}

export async function setInput(key: string, value: string | number | File) {
  if (typeof value === "string" || typeof value === "number") {
    send({ type: "input", key, value: String(value) });
    return;
  }
  const bytes = await value.arrayBuffer();
  const imageId = `user-image://${fiddle}/${key}/${Date.now()}/${value.name}`;
  send({ type: "image-input", key, imageId, bytes }, [bytes]);
}

$effect(() => {
  send({ type: "animation", paused });
});

onMount(() => {
  if (!fiddle) {
    onSvgWritableChange?.(false);
    return;
  }

  if (!canvasElement.transferControlToOffscreen) {
    supported = false;
    loading = false;
    return;
  }

  worker = new CanvasWorker();
  worker.addEventListener("message", (event: MessageEvent<CanvasWorkerStatus>) => {
    if (event.data.type === "error") {
      console.error(`[canvas-worker] ${event.data.message}`);
    } else if (event.data.type === "ready") {
      // Do nothing.
    } else if (event.data.type === "first-frame") {
      loading = false;
    } else if (event.data.type === "controls") {
      onControlsChange?.(event.data.controls);
    } else if (event.data.type === "svg-capability") {
      onSvgWritableChange?.(event.data.writable);
    } else if (event.data.type === "svg-export") {
      const pending = pendingSvgRequests.get(event.data.requestId);
      if (!pending) return;
      pendingSvgRequests.delete(event.data.requestId);
      if (event.data.error) {
        pending.reject(new Error(event.data.error));
      } else if (event.data.blob) {
        pending.resolve(event.data.blob);
      } else {
        pending.reject(new Error("The canvas worker returned no SVG file data."));
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
    onControlsChange?.([]);
  };
});
</script>

<div
  class="canvas-stage"
  class:shape-intersection-stage={fiddle === "shape-intersection"}
  bind:this={stageElement}
>
  <canvas
    bind:this={canvasElement}
    aria-label={fiddle ? "Animated preview of the selected fiddle" : "Blank fiddle"}
  ></canvas>
  {#if !fiddle || loading}
    <div
      class="blank-fiddle"
      role="img"
      aria-label={fiddle ? "Loading fiddle" : "No fiddle selected"}
    >
      <svg viewBox="0 0 100 100" aria-hidden="true">
        <rect x="3" y="3" width="94" height="94" rx="3" />
        <circle cx="50" cy="50" r="26" />
        <path d="M31.6 31.6 68.4 68.4" />
      </svg>
    </div>
  {/if}
  {#if !supported}
    <div class="unsupported">
      This browser cannot hand a canvas to a Web Worker. Try the latest Chrome, Edge, or Firefox.
    </div>
  {/if}
</div>
