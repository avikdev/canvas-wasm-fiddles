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
  onWasmReady,
  onWasmError,
}: {
  fiddle?: FiddleId;
  paused: boolean;
  onSvgWritableChange?: (writable: boolean) => void;
  onControlsChange?: (controls: FiddleControlDefinition[]) => void;
  onWasmReady?: () => void;
  onWasmError?: (message: string) => void;
} = $props();
let canvasElement: HTMLCanvasElement;
let stageElement: HTMLDivElement;
let worker: Worker | undefined;
let supported = $state(true);
let loading = $state(true);
let wasmReady = $state(false);
let workerFiddle: FiddleId | undefined;
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

$effect(() => {
  const requestedFiddle = fiddle;
  if (!wasmReady || !worker || !requestedFiddle || requestedFiddle === workerFiddle) return;
  workerFiddle = requestedFiddle;
  loading = true;
  send({ type: "select", fiddle: requestedFiddle });
});

onMount(() => {
  if (!canvasElement.transferControlToOffscreen) {
    supported = false;
    loading = false;
    onWasmError?.("This browser does not support OffscreenCanvas.");
    return;
  }

  worker = new CanvasWorker();
  worker.addEventListener("message", (event: MessageEvent<CanvasWorkerStatus>) => {
    if (event.data.type === "error") {
      console.error(`[canvas-worker] ${event.data.message}`);
    } else if (event.data.type === "ready") {
      wasmReady = true;
      onWasmReady?.();
    } else if (event.data.type === "initialization-error") {
      loading = false;
      onWasmError?.(event.data.message);
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
  workerFiddle = fiddle;
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
    wasmReady = false;
    workerFiddle = undefined;
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
        <path d="M30 17h40M30 83h40" />
        <path d="M35 17v12c0 9 5.5 14.5 15 21-9.5 6.5-15 12-15 21v12" />
        <path d="M65 17v12c0 9-5.5 14.5-15 21 9.5 6.5 15 12 15 21v12" />
        <path d="M39 73c3-7 7-10 11-13 4 3 8 6 11 13z" />
      </svg>
      <div class="blank-fiddle-copy">
        <p class="blank-fiddle-title">NO OUTPUT</p>
        <p>Loading output, (select from left panel)</p>
      </div>
    </div>
  {/if}
  {#if !supported}
    <div class="unsupported">
      This browser cannot hand a canvas to a Web Worker. Try the latest Chrome, Edge, or Firefox.
    </div>
  {/if}
</div>
